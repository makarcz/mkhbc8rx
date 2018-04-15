/*
 *
 * File:	texted.c
 * Purpose:	Code of text editor.
 * Author:	Marek Karcz
 * Created:	3/18/2018
 *
 * Revision history:
 *
 * 3/18/2018
 *  Created.
 *  NOTE: This is UNTESTED CODE.
 *
 * 3/21/2018
 *  First working prototype implemented.
 *  Functions 'a' (add) and 'l' (list).
 *
 * 3/27/2018
 *  Function 'd' (delete) is working.
 *
 * 3/28/2018
 *  Added functions 'b' (bank), 'e' (erase) and 'm' (memory information).
 *  Added text selection, listing the selection, line numbers switch for
 *  listing all content or selection.
 *
 * 3/29/2018
 *  Added function 'c' (cut).
 *
 * 3/31/2018
 *  Testing and corrections of select / cut functions.
 *  Refactored (created checkbuf() function, renamed some functions).
 *
 * 4/2/2018
 *  Working on insert function.
 *  Found and corrected bug in delete function.
 *  Some code optimizations.
 *  Added timeout for null header search operation.
 *  Corrected bug in delete selection.
 *  Removed commands 'a' and '@EOT' from the text listing output.
 *
 * 4/3/2018
 *  Fixed bug that I planted in isnull_hdr() func.
 *  Added set_timeout(), is_timeout() functions.
 *  Refactoring.
 *
 * 4/4/2018
 *  Added copy selection function.
 *  Adder renumber() function.
 *  Fixed bugs in recently added code.
 *
 * 4/5/2018
 *  Added comments.
 *  Added options to 'g' command.
 *  Some refactoring and small changes.
 *
 * 4/10/2018
 *  Implemented find text function and date / time.
 *
 * 4/11/2018
 *  Removed debug code.
 *  Changed renumber function to be more flexible.
 *  Optimized delete_text() function.
 *  Added assert() function.
 *
 * 4/13/2018
 *  Reduced clipboard size due to memory corruption.
 *  Code optimized. Bugs fixed.
 *
 * 4/15/2018
 *  Clipboard size set at 3.5 kB and tested to function properly.
 *  Added checking the consistency of RAM bank setup - this detects memory
 *  management malfunctions.
 *
 *  ..........................................................................
 *  TO DO:
 *
 *  ..........................................................................
 *  BUGS:
 *
 *  1) When clipboard size is 4 kB, program malfunctions. I am not sure if this
 *     is a cc65 issue or bug in my code. Wher I reduce the size to 3.5 kB all
 *     is good again. Compilation / linking doesn't warn about any memory
 *     segments overlapping and I examined carefully listing and map files and
 *     found no reason for this problem. For now the issue is averted by
 *     reducing the size of the clipboard buffer.
 *
 *  ..........................................................................
 */

#include <stdlib.h>
#include <stdint.h>
#include <string.h>
#include <peekpoke.h>
#include "mkhbcos_serialio.h"
#include "mkhbcos_ml.h"
#include "mkhbcos_ds1685.h"
#include "romlib.h"

#define IBUF1_SIZE      20
#define IBUF2_SIZE      20
#define IBUF3_SIZE      20
#define PROMPTBUF_SIZE  80
#define TEXTLINE_SIZE   80
#define RADIX_DEC       10
#define RADIX_HEX       16
#define RADIX_BIN       2
#define MAX_LINES       ((END_BRAM-START_BRAM)/6)
#define CLIPBRDBUF_SIZE 512*7   // 3.5 kB, don't go above it

// command codes
enum cmdcodes
{
    CMD_NULL = 0,
    CMD_HELP,
    CMD_EXIT,
    CMD_LIST,
    CMD_ADD,
    CMD_GOTO,
    CMD_DELETE,
    CMD_SELECT,
    CMD_COPY,
    CMD_ERASE,
    CMD_DTTM,
    CMD_BANK,
    CMD_INFO,
    CMD_INSERT,
    CMD_CUTSEL,
    CMD_FIND,
	//-------------
	CMD_UNKNOWN
};

const char *ver = "1.3.9.\n\r";

// next pointer in last line's header in text buffer should point to
// such content in memory
const char null_txt_hdr[] = {0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0x00};
// this string is entered by operator to end text adding / insertion
const char *eot_str = "@EOT";

// some other commonly used constants, defining them here and using pointers
// saves some code space
const char *divider = "--------------------------------------------\n\r";
const char *unk3q = "???";
const char *unk2q = "??";
const char *spcolosp = " : ";
const char *zerochr = "0";
const char *spcchr = " ";
const char *commspc = ", ";

// Calendar constants.
const char *daysofweek[8] =
{
    "Sunday",
    "Monday",
    "Tuesday",
    "Wednesday",
    "Thursday",
    "Friday",
    "Saturday"
};

const char *monthnames[13] =
{
    "January",
    "February",
    "March",
    "April",
    "May",
    "June",
    "July",
    "August",
    "September",
    "October",
    "November",
    "December"
};

// error codes
enum eErrors {
    ERROR_OK = 0,
    ERROR_BADCMD,
    ERROR_FULLBUF,
    ERROR_BADARG,
    ERROR_NOTEXT,
    ERROR_BUFNOTINIT,
    ERROR_ADDROOR,
    ERROR_TIMEOUT,
    ERROR_CLIPBRDOVF,
    ERROR_EMPTYCLIPBRD,
    ERROR_END,
    //------------------
    ERROR_UNKNOWN
};

// Error messages.
const char *ga_errmsg[13] =
{
    "OK.",
    "Unknown command.",
    "Buffer full. Enter @EOT to exit to prompt.",
    "Check arguments.",
    "No text in buffer.",
    "Buffer was never initialized.",
    "Address out of range.",
    "Timeout during text buffer search operation.",
    "Clipboard buffer overflow.",
    "Clipboard is empty.",
    "Search reached the end of text.",
    "Unknown."
};

// Usage help.
const char *helptext[43] =
{
    "\n\r",
    " b : select / show current file # (bank #).\n\r",
    "     b [00..07]\n\r",
    " l : List text buffer contents.\n\r",
    "     l all [n] | sel [n] | clb | [from_line#[-to_line# | -end]] [n]\n\r",
    "      n - show line numbers\n\r",
    "      all - list all content\n\r",
    "      sel - list only selected content\n\r",
    "      clb - list clipboard contents\n\r",
    " a : start adding text at the end of buffer.\n\r",
    " i : start inserting text at line# (or current line).\n\r",
    "     i [line#]\n\r",
    "     NOTE: 'a' and 'i' functions - type '@EOT' in a new line\n\r",
    "           and press ENTER to finish and exit to command prompt.\n\r",
    " g : go to specified line in text buffer.\n\r",
    "     g start | end | [line#]\n\r",
    " d : delete text.\n\r",
    "     d [from_line#[-to_line# | -end]]\n\r",
    " s : select text. (copies to clipboard)\n\r",
    "     s [from_line#[-to_line# | -end]]\n\r",
    " c : delete (cut) selection.\n\r",
    " p : copy selected text.\n\r",
    "     p [line#]\n\r",
    " x : exit editor, leave text in buffer.\n\r",
    " e : erase all text.\n\r",
    " t : insert current date / time.\n\r",
    " m : show file and memory information.\n\r",
    "     Current file # (memory bank #).\n\r",
    "     # of lines in file.\n\r",
    "     Next free address (hex) in buffer.\n\r",
    "     Free memory in current buffer.\n\r",
    " f : find a text, search starts in next line from current.\n\r",
    "     Move cursor to the line of occurrence if found.\n\r",
    "     f [text]\n\r",
    "NOTE:\n\r",
    "   Default line#, from_line#, to_line# is the current line.\n\r",
    "   Arguments are <mandatory> OR [optional].\n\r",
    "   When range is provided, SPACE can be used instead of '-'.\n\r",
    "   Argument 'text' can contain spaces.\n\r",
    "   'start' - 1-st line of text, 'end' - last line of text.\n\r",
    "\n\r",
    "@EOH"
};

// Buffer for current text record.
struct text_line {
    unsigned int num;
    unsigned char len;
    uint16_t next_ptr;
    char text[TEXTLINE_SIZE];
} CurrLine;

// RTC data buffer.
struct ds1685_clkdata	clkdata;

// globals
int  cmd_code = CMD_NULL;
char prompt_buf[PROMPTBUF_SIZE];
char ibuf1[IBUF1_SIZE], ibuf2[IBUF2_SIZE], ibuf3[IBUF3_SIZE];
char clipbrd[CLIPBRDBUF_SIZE];
char bankinit_flags[8];
unsigned int line_num, sel_begin, sel_end, last_line, line_count;
uint16_t curr_addr, lastaddr;
int bank_num;
unsigned long tmr64;

// Functions prototypes.
void        getline(void);
void        parse_cmd(void);
void        texted_help(void);
void        texted_shell(void);
void        texted_banner(void);
void        prnerror(int errnum);
int         adv2nxttoken(int idx);
int         adv2nextspc(int idx);
void        texted_prndt(void);
int         exec_cmd(void);
void        texted_add(void);
void        texted_list(void);
void        texted_goto(void);
void        texted_delete(void);
void        pause_sec64(uint16_t delay);
int         isnull_hdr(const char *hdr);
void        texted_initbuf(void);
void        texted_membank(void);
void        texted_info(void);
void        texted_select(void);
void        delete_text(unsigned int from_line, unsigned int to_line);
void        texted_cut(void);
int         checkbuf(void);
void        texted_insert(void);
uint16_t    goto_line(unsigned int gtl);
int         isaddr_oor(uint16_t addr);
void        set_timeout(int secs);
int         is_timeout(void);
void        write_text2mem(uint16_t addr);
int         copy2clipbrd(unsigned int from_line, unsigned int to_line);
void        texted_copy(void);
uint16_t    renumber(uint16_t addr, int offs);
void        list_clipbrd(void);
void        texted_find(void);
void        texted_insdttm(void);
void        texted_assert(uint16_t line, int cond, const char *msg);
void        prninfo_addtext(void);

////////////////////////////// CODE /////////////////////////////////////

/*
 * Print information about text adding function.
 */
void prninfo_addtext(void)
{
    puts("\n\rType lines of text no longer than 80 characters.\n\r");
    puts("Press BAKSPACE to delete characters, ENTER to add new line.\n\r");
    puts("Type '@EOT' in the new line and press ENTER to finish.\n\r\n\r");
}

/*
 * If the condition evaluates to zero (false), prints a message and aborts
 * the program.
 */
void texted_assert(uint16_t line, int cond, const char *msg)
{
    if (!cond) {
        puts("\n\rASSERT in line ");
        puts(utoa(line, ibuf3, RADIX_DEC));
        puts("\n\rMessage: ");
        puts(msg);
        puts("\n\r");
        pause_sec64(65);
        exit(0);
    }
}

/*
 * Read date/time from DS1685 IC and insert it into current line.
 * Format: "DayOfWeek, Month Day, Year, at hh : mm"
 */
void texted_insdttm(void)
{
    if (RTCDETECTED) {

        ds1685_rdclock (&clkdata);

        memset(ibuf3, 0, IBUF3_SIZE);
        memset(&CurrLine, 0, sizeof (struct text_line));

        if (clkdata.dayofweek > 0 && clkdata.dayofweek < 8)	{
           strcpy(CurrLine.text, daysofweek[clkdata.dayofweek-1]);
        } else {
            strcpy(CurrLine.text, unk3q);
        }

        strcat(CurrLine.text, commspc);

        if (clkdata.month > 0 && clkdata.month < 13)
            strcat(CurrLine.text, monthnames[clkdata.month-1]);
        else
            strcat(CurrLine.text, unk3q);

        strcat(CurrLine.text, spcchr);

        strcat(CurrLine.text, itoa(clkdata.date, ibuf3, RADIX_DEC));

        strcat(CurrLine.text, commspc);

        if (clkdata.century < 100)
            strcat(CurrLine.text, itoa(clkdata.century, ibuf3, RADIX_DEC));
        else
            strcat(CurrLine.text, unk2q);
        if(clkdata.year < 100)
        {
            if (clkdata.year < 10)
                strcat(CurrLine.text, zerochr);
            strcat(CurrLine.text, itoa(clkdata.year, ibuf3, RADIX_DEC));
        }
        else
            strcat(CurrLine.text, unk2q);

        strcat(CurrLine.text, commspc);
        strcat(CurrLine.text, "at ");

        if (clkdata.hours < 10)
            strcat(CurrLine.text, zerochr);
        strcat(CurrLine.text, itoa(clkdata.hours, ibuf3, RADIX_DEC));
        strcat(CurrLine.text, spcolosp);
        if (clkdata.minutes < 10)
            strcat(CurrLine.text, zerochr);
        strcat(CurrLine.text, itoa(clkdata.minutes, ibuf3, RADIX_DEC));

        CurrLine.num = line_num;
        CurrLine.len = strlen(CurrLine.text);
        CurrLine.next_ptr = curr_addr + CurrLine.len + 6;

        if (isnull_hdr((const char *)curr_addr)) { // add at the end
                if (0 == isaddr_oor(CurrLine.next_ptr + 6)) {
                    CurrLine.num++;
                    write_text2mem(curr_addr);
                    strcpy((char *)CurrLine.next_ptr, null_txt_hdr);
                } else {
                    prnerror(ERROR_FULLBUF);
                }
        } else {    // insert in the middle
            if (END_BRAM > (lastaddr + CurrLine.len + 6)) {
                // there is space for new line, so continue...
                // 1. Move text down
                memmove((void *)CurrLine.next_ptr,
                        (const void *)curr_addr,
                        (size_t)(lastaddr - curr_addr + 1));
                // 2. Insert newly entered line
                write_text2mem(curr_addr);
                // 3. Renumber following lines
                renumber(CurrLine.next_ptr, 1);
            } else {
                // buffer full, end here
                prnerror(ERROR_FULLBUF);
            }
        }
        checkbuf();
    }
}

/*
 * List clipboard contents.
 */
void list_clipbrd(void)
{
    unsigned int llen;
    char *s = clipbrd;

    set_timeout(5);
    while(0 == is_timeout()) {
        if (isnull_hdr((const char *)s)) {
            break;
        }
        puts(s);
        puts("\n\r");
        llen = strlen(s);
        s += (llen + 1);
    }
}

/*
 * Copy selected block of text to the clipboard.
 * To save space, only text is copied, not the headers.
 * In case the selected block is too big for the clipboard, error is printed
 * and only the amount of text that can fit in the clipboard is copied.
 * Return # of lines copied.
 */
int copy2clipbrd(unsigned int from_line, unsigned int to_line)
{
    unsigned int line, llen;
    uint16_t addr = START_BRAM;
    int clipbrd_index = 0;
    int ret = 0, i = 0;

    // clear the clipboard
    for ( ; i < CLIPBRDBUF_SIZE; i++) {
        clipbrd[i] = 0;
    }
    strcpy(clipbrd, null_txt_hdr);
    // copy selected text to clipboard
    while(addr < END_BRAM) {
        if (isnull_hdr((const char *)addr)) {
            break;  // end of text buffer reached
        }
        line = PEEKW(addr);
        if (line > to_line) {
            break;  // all selected lines copied
        }
        if (line >= from_line) { // copy text from record in buffer to clipbrd
            llen = PEEK(addr + 2);
            if ((clipbrd_index + llen + strlen(null_txt_hdr))
                   < CLIPBRDBUF_SIZE) {

                *(clipbrd + clipbrd_index) = 0;
                strcpy((char *)(clipbrd + clipbrd_index),
                       (const char *)(addr + 5));
                clipbrd_index += (llen + 1);
                // put null header at the end
                strcpy((char *)(clipbrd + clipbrd_index), null_txt_hdr);
                ret++;
            } else { // clipboard buffer overflow
                prnerror(ERROR_CLIPBRDOVF);
                break;
            }
        }
        addr = PEEKW(addr + 3); // next text record
        if (isaddr_oor(addr)) {
            break;  // address out of range
        }
    }

    return ret;
}

/*
 * Write single text record (header and contents) to the memory address addr.
 */
void write_text2mem(uint16_t addr)
{
    POKEW(addr, CurrLine.num);
    POKE(addr + 2, CurrLine.len);
    POKEW(addr + 3, CurrLine.next_ptr);
    strcpy((char *)(addr + 5), CurrLine.text);
}

/*
 *  Return non-zero if addr is out of range START_BRAM .. END_BRAM.
 */
int isaddr_oor(uint16_t addr)
{
    if (addr < START_BRAM || addr >= END_BRAM)
    {
        prnerror(ERROR_ADDROOR);
        puts("addr = $");
        puts(utoa(addr, ibuf1, RADIX_HEX));
        puts("\n\r");

        return 1;
    }

    return 0;
}

/*
 * Return non-zero if the text record header pointed by hdr is a null header.
 */
int isnull_hdr(const char *hdr)
{
    int i = 1;
    int ret = 1;

    if (hdr == NULL
        || hdr[0] != null_txt_hdr[0]
        || strlen(hdr) < strlen(null_txt_hdr)) {
        ret = 0;
    } else {
        while(hdr[i] != 0 && null_txt_hdr[i] != 0) {
            if (hdr[i] != null_txt_hdr[i]) {
                ret = 0;
                break;
            }
            i++;
        }
    }

    return ret;
}

/*
 * Pause specified # of 1/64-ths of a second.
 */
void pause_sec64(uint16_t delay)
{
    // current timer + desired delay in 1/64 sec intervals
    // will be the number we wait for in a loop
    unsigned long t64hz = *TIMER64HZ + delay;

    if (RTCDETECTED) {
        while (*TIMER64HZ < t64hz) {
            // Intentionally EMPTY
        }
    }
}

/*
 * Set global variable tmr64 to a value that will be reached by system timer
 * after certain # of seconds (secs).
 */
void set_timeout(int secs)
{
    if (RTCDETECTED) {
        tmr64 = *TIMER64HZ + (secs * 64);
    }
}

/*
 * Return 1 if the timeout set by set_timeout() function has expired.
 */
int is_timeout(void)
{
    int ret = 0;

    if (RTCDETECTED) {
        if (tmr64 < *TIMER64HZ) {
            ret = 1;
        }
    }

    return ret;
}

// Command parsing helpers.
/*
 * Advance index to next token.
 */
int adv2nxttoken(int idx)
{
    while (prompt_buf[idx] == 32) {
        idx++;
    }

    return idx;
}

/*
 * Advance index to next space.
 */
int adv2nextspc(int idx)
{
    while (prompt_buf[idx] != 0) {

        if (prompt_buf[idx] == 32
            || prompt_buf[idx] == '-') {

            prompt_buf[idx] = 0;
            idx++;
            break;
        }
        idx++;
    }

    return idx;
}

/* print error message in a separate line */
void prnerror(int errnum)
{
    puts("\n\rERROR: ");
    puts(ga_errmsg[errnum]);
    puts("\r\n");
}

/* get line of text from input */
void getline(void)
{
    memset(prompt_buf, 0, PROMPTBUF_SIZE);
    gets(prompt_buf);
    puts("\n\r");
}

/* parse the input buffer, set command code */
void parse_cmd(void)
{
    if (0 == strlen(prompt_buf)) {
        cmd_code = CMD_NULL;
    }
    else if (*prompt_buf == 'l') {
        cmd_code = CMD_LIST;
    }
    else if (*prompt_buf == 'a') {
        cmd_code = CMD_ADD;
    }
    else if (0 == strncmp(prompt_buf, "g ", 2)) {
        cmd_code = CMD_GOTO;
    }
    else if (*prompt_buf == 'd') {
        cmd_code = CMD_DELETE;
    }
    else if (*prompt_buf == 's') {
        cmd_code = CMD_SELECT;
    }
    else if (*prompt_buf == 'h') {
        cmd_code = CMD_HELP;
    }
    else if (*prompt_buf == 'p') {
        cmd_code = CMD_COPY;
    }
    else if (1 == strlen(prompt_buf) && *prompt_buf == 'x') {
        cmd_code = CMD_EXIT;
    }
    else if (1 == strlen(prompt_buf) && *prompt_buf == 'e') {
        cmd_code = CMD_ERASE;
    }
    else if (*prompt_buf == 't') {
        cmd_code = CMD_DTTM;
    }
    else if (*prompt_buf == 'b') {
        cmd_code = CMD_BANK;
    }
    else if (*prompt_buf == 'm') {
        cmd_code = CMD_INFO;
    }
    else if (*prompt_buf == 'i') {
        cmd_code = CMD_INSERT;
    }
    else if (*prompt_buf == 'c') {
        cmd_code = CMD_CUTSEL;
    }
    else if (*prompt_buf == 'f') {
        cmd_code = CMD_FIND;
    }
    else
        cmd_code = CMD_UNKNOWN;
}

/* print help */
void texted_help(void)
{
    unsigned char cont = 1, i = 0;

    while(cont)
    {
        if (0 == strncmp(helptext[i],"@EOH",4))
            cont = 0;
        else {
            //puts("   ");  // 3 spaces
            puts(helptext[i++]);
        }
        if (0 == (i%22)) {
            puts("\n\r");
            puts("Press any key to continue...");
            while(!kbhit());    // wait for key press...
            getc();            // consume key
            puts("\n\r\n\r");
        }
    }
}

/*
 * Execute command.
 */
int exec_cmd(void)
{
    int ret = 1;

    switch (cmd_code)
    {
        case CMD_NULL:
            break;
        case CMD_UNKNOWN:
            prnerror(ERROR_BADCMD);
            break;
        case CMD_EXIT:
            puts("Bye!\n\r");
            ret = 0;
            break;
        case CMD_HELP:
            texted_help();
            break;
        case CMD_ADD:
            texted_add();
            break;
        case CMD_LIST:
            texted_list();
            break;
        case CMD_GOTO:
            texted_goto();
            break;
        case CMD_DELETE:
            texted_delete();
            break;
        case CMD_ERASE:
            texted_initbuf();
            break;
        case CMD_BANK:
            texted_membank();
            break;
        case CMD_INFO:
            texted_info();
            break;
        case CMD_SELECT:
            texted_select();
            break;
        case CMD_CUTSEL:
            texted_cut();
            break;
        case CMD_INSERT:
            texted_insert();
            break;
        case CMD_COPY:
            texted_copy();
            break;
        case CMD_FIND:
            texted_find();
            break;
        case CMD_DTTM:
            texted_insdttm();
            break;
        default:
            break;
    }

    return ret;
}

/*
 *  Add text at the end of the text buffer.
 */
void texted_add(void)
{
    uint16_t nxtaddr;

    curr_addr = START_BRAM;
    line_num = 0;
    if (0 == checkbuf()) {
        return;
    }
    // Position current address at the NULL line after text.
    while (1) {
        if (isnull_hdr((const char *)curr_addr)) {
            break; // found null header
        }
        line_num = PEEKW(curr_addr) + 1;
        nxtaddr = PEEKW(curr_addr + 3);
        curr_addr = nxtaddr;
        if (isaddr_oor(curr_addr)) {
            return;
        }
    }
    prninfo_addtext();
    while (1) {
        // create and store null text header at current address
        strcpy((char *)curr_addr, null_txt_hdr);
        // get line of text from user input
        getline();
        if (0 == strncmp(eot_str, prompt_buf, strlen(eot_str))) {
            break;  // user entered command to end text entry
        }
        strcpy (CurrLine.text, prompt_buf);
        CurrLine.len = strlen(prompt_buf);
        CurrLine.next_ptr = curr_addr + CurrLine.len + 6;
        if (CurrLine.next_ptr > (END_BRAM - 6)) {
            // Text buffer full, terminate text entry here.
            // User must still enter '@EOT' to exit loop.
            // This is to prevent entering accidental command while loading
            // text via serial port and buffer overflows while input data is
            // still coming. Instead ignore all incoming text after
            // buffer was filled until end-of-text command is entered.
            while(1) {
                prnerror(ERROR_FULLBUF);
                getline();
                if (0 == strncmp(eot_str, prompt_buf, strlen(eot_str))) {
                    break;  // user entered command to end text entry
                }
            }
            break;
        }
        // update header and text at current address
        CurrLine.num = line_num;
        write_text2mem(curr_addr);
        // proceed to next address
        line_num++;
        curr_addr = CurrLine.next_ptr;
        if (isaddr_oor(curr_addr)) {
            return;
        }
    }
    checkbuf(); // update globals after text buffer was altered
}

/*
 * Renumber line numbers in text records starting at address addr.
 * Also, refresh the next record pointers in these text records as this
 * function is called after the text has been relocated in buffer.
 * Return address where the renumbering ended.
 */
uint16_t renumber(uint16_t addr, int offs)
{
    unsigned int lnum;
    uint16_t nxtaddr;

    while(1) {
        if (isnull_hdr((const char *)addr)) {
            break;
        }
        lnum = PEEKW(addr) + offs;
        POKEW(addr, lnum);
        nxtaddr = addr + PEEK(addr + 2) + 6;
        if (isaddr_oor(nxtaddr)) {
            break;
        }
        POKEW(addr + 3, nxtaddr);
        addr = nxtaddr;
    }

    return addr;
}

/*
 * Insert text anywhere in the text buffer.
 * Command is expected in prompt_buf in following format before calling
 * this function:
 * "i [line#]"
 * NOTE: This function will always insert text. If you provide line# equal to
 *       last line in the text, the last line in the text will be effectively
 *       pushed down by newly added lines.
 *       To add lines at the end, use texted_add() function.
 */
void texted_insert(void)
{
    int n1;
    uint16_t addr;
    int n0 = 2;

    if (0 == checkbuf()) {
        return;
    }
    n0 = adv2nxttoken(n0);
    n1 = adv2nextspc(n0);
    if (n1 > n0) {
        line_num = atoi(prompt_buf + n0);
    }
    if (line_num > last_line) {
        prnerror(ERROR_BADARG);
        return;
    }
    if (isnull_hdr((const char *)START_BRAM)) {
        texted_add();   // if we are inserting into empty buffer
                        // it is the same as adding text
    } else {
        prninfo_addtext();
        while (1) {
            goto_line(line_num);
            // get the text from user input
            getline();
            if (0 == strncmp(eot_str, prompt_buf, strlen(eot_str))) {
                break;  // user entered command to end text entry
            }
            strcpy (CurrLine.text, prompt_buf);
            CurrLine.len = strlen(prompt_buf);
            CurrLine.next_ptr = curr_addr + CurrLine.len + 6;
            // make space for new line by moving all the text below down
            if (END_BRAM > (lastaddr + CurrLine.len + 6)) {
                // there is space for new line, so continue...
                // 1. Move text down
                memmove((void *)CurrLine.next_ptr,
                        (const void *)curr_addr,
                        (size_t)(lastaddr - curr_addr + 1));
                // 2. Insert newly entered line
                CurrLine.num = line_num;
                write_text2mem(curr_addr);
                // 3. Renumber following lines
                addr = CurrLine.next_ptr;
                addr = renumber(addr, 1);
                line_num++;
                checkbuf();
            } else {
                // buffer full, end here
                prnerror(ERROR_FULLBUF);
                break;
            }
        }
    }
}

/*
 * Copy text from clipboard to a specified (or current) line.
 * Command in following format is expected in prompt_buf:
 * "p [line#]"
 * NOTE: This function will always insert text. If you provide line# equal to
 *       last line in the text, the last line in the text will be effectively
 *       pushed down by newly added lines.
 *       To copy selection / clipboard at the end, first add an empty line to
 *       the text (if not already there) and then call this function with
 *       line# that equals last line number in text buffer.
 */
void texted_copy(void)
{
    int n1;
    uint16_t addr;
    int n0 = 2;
    int clipbrd_index = 0;
    int adding = 0;

    if (0 == checkbuf()) {
        return;
    }
    if (isnull_hdr((const char *)clipbrd)) {
        prnerror(ERROR_EMPTYCLIPBRD);
        return;
    }
    n0 = adv2nxttoken(n0);
    n1 = adv2nextspc(n0);
    if (n1 > n0) {
        line_num = atoi(prompt_buf + n0);
    }
    if (line_num > last_line) {
        prnerror(ERROR_BADARG);
        return;
    }
    if (isnull_hdr((const char *)START_BRAM)) {
        line_num = 0;
        curr_addr = START_BRAM;
        adding = 1;
    } else {
        goto_line(line_num);
    }
    while(clipbrd_index < CLIPBRDBUF_SIZE) {
        if (isnull_hdr((const char *)(clipbrd + clipbrd_index))) {
            break; // reached the end of text in clipboard
        }
        adding = isnull_hdr((const char *)curr_addr);
        strcpy(CurrLine.text, (const char *)(clipbrd + clipbrd_index));
        CurrLine.len = strlen((const char *)(clipbrd + clipbrd_index));
        clipbrd_index += (CurrLine.len + 1);
        CurrLine.num = line_num;
        CurrLine.next_ptr = curr_addr + CurrLine.len + 6;
        if (END_BRAM > (lastaddr + CurrLine.len + 6)) {
            // there is space for new line, so continue...
            // 1. Move text down (if inserting)
            if (0 == adding) {
                memmove((void *)CurrLine.next_ptr,
                        (const void *)curr_addr,
                        (size_t)(lastaddr - curr_addr + 1));
            }
            // 2. Insert newly entered line
            write_text2mem(curr_addr);
            // 3. Renumber following lines (if inserting)
            addr = CurrLine.next_ptr;
            curr_addr = addr;
            if (adding) {
                strcpy((char *)addr, null_txt_hdr); // write null header
            } else {
                addr = renumber(addr, 1);
            }
            lastaddr = addr + strlen(null_txt_hdr);
            line_num++;
            //checkbuf();
        } else {
            // buffer full, end here
            prnerror(ERROR_FULLBUF);
            break;
        }
    }
}

/*
 * Find text in buffer.
 * Command is expected in prompt_buf before calling this function:
 * "f [text]"
 * Search starts in the next line from current line.
 * Search ends when 1-st occurrence is found.
 * Cursor / current line is changed to the line where the text was found.
 * If end is reached, message is printed.
 * If no argument is provided, previous one (if valid) is used.
 * With this implementation, each next search will find the next occurrences
 * with minimum use of memory.
 */
void texted_find(void)
{
    uint16_t nxt_line;
    int n;
    char *tptr;
    unsigned int line = line_num + 1;   // start search in next line

    if (0 == checkbuf()) {
        return;
    }
    // if there is argument, new search string is provided by user
    // otherwise search for one used previously
    if (strlen(prompt_buf) > 2) {
        memset(CurrLine.text, 0, TEXTLINE_SIZE);
        n = adv2nxttoken(1);
        strcpy(CurrLine.text, prompt_buf + n);
    }
    // sanity check
    CurrLine.len = strlen(CurrLine.text);
    if (0 == CurrLine.len) {
        prnerror(ERROR_BADARG);
        return;
    }
    if (line > last_line) {
        prnerror(ERROR_END);    // search reached end of text buffer
        return;
    }
    n = 0;
    while(1) {
        puts("Searching ");
        puts(utoa(line, ibuf1, RADIX_DEC));
        putchar(0x0A);
        nxt_line = goto_line(line);
        tptr = (char *) (curr_addr + 5);
        while(*tptr != 0) {
            if (0 == strncmp(tptr, CurrLine.text, CurrLine.len)) {
                n = 1;
                break; // found text, current line already set
            }
            tptr++;
        }
        if (0 != n || 0xFFFF == nxt_line) {
            break;  // no more text to search
        }
        line = nxt_line;
    }
    if (n) {
        puts("* Found *\n\r");
        puts((const char *)(curr_addr + 5));
        puts("\n\r");
    } else {
        puts("* Not found *  \n\r");
    }
}

/*
 * List text buffer contents.
 * Command is expected in prompt_buf before calling this function:
 * "l all [n] | l sel [n] | l clb | l [from_line#[-to_line#]] [n]"
 */
void texted_list(void)
{
    int      n0, n;
    uint16_t from_line = line_num;
    uint16_t to_line = line_num;
    uint16_t addr = START_BRAM;
    uint16_t line = 0;
    int      show_num = 0;

    if (0 == checkbuf()) {
        return;
    }
    if (strlen(prompt_buf) > 1) {
        n0 = adv2nxttoken(2);
        n = adv2nextspc(n0);
        if (0 == strncmp(prompt_buf + n0, "clb", 3)) {

            if (0 == isnull_hdr((const char *)clipbrd)) {
                list_clipbrd();
            } else {
                prnerror(ERROR_EMPTYCLIPBRD);
            }
            return;

        } else if (0 == strncmp(prompt_buf + n0, "sel", 3)) {

            n = adv2nxttoken(n);
            n0 = adv2nextspc(n);
            show_num = (prompt_buf[n] == 'n');
            from_line = sel_begin;
            to_line = sel_end;

        } else if (strncmp(prompt_buf + n0, "all", 3)) {
            // range specified in command line
            show_num = (prompt_buf[n0] == 'n');
            if (!show_num) {
                from_line = atoi (prompt_buf + n0);
                to_line = from_line;
                n = adv2nxttoken(n);
                n0 = adv2nextspc(n);
                if (n0 > n) {
                    show_num = (prompt_buf[n] == 'n');
                    if (!show_num) {
                        if (0 == strncmp((const char *)(prompt_buf + n), "end", 3)) {
                            to_line = last_line;
                        } else {
                            to_line = atoi (prompt_buf + n);
                        }
                        if (to_line < from_line) {
                            prnerror(ERROR_BADARG);
                            return;
                        }
                        n = adv2nxttoken(n0);
                        adv2nextspc(n);
                        show_num = (prompt_buf[n] == 'n');
                    }
                }
            }
        } else {    // list all content

            n = adv2nxttoken(n);
            n0 = adv2nextspc(n);
            show_num = (prompt_buf[n] == 'n');
            from_line = 0;
            while (1) {
                if (isnull_hdr((const char *)addr)) {
                    break;  // found last line
                }
                to_line = PEEKW(addr);
                addr = PEEKW(addr + 3);
                if (isaddr_oor(addr)) {
                    return;
                }
            }
            addr = START_BRAM;
        }
    }
    if (isnull_hdr((const char *)START_BRAM)) {
        prnerror(ERROR_NOTEXT);
        return;
    }

    while (1) {
        if (isnull_hdr((const char *)addr)) {
            break; // EOT, null text header is next
        }
        line = PEEKW(addr);
        if (line > to_line) {
            break;
        }
        if (line >= from_line) {
            if (show_num) {
                itoa(line, ibuf1, RADIX_DEC);
                puts(ibuf1);
                puts(":");
            }
            puts ((const char *)(addr + 5));
            puts ("\n\r");
        }
        addr = PEEKW(addr + 3);
        if (isaddr_oor(addr)) {
            return;
        }
    }
    pause_sec64(150);   // just a bit more than 2 seconds delay to allow
                        // PropTermMK to time out in SaveOutput2FileSD.
}

/*
 * Position current line at gtl.
 * Return # of next line or 0xFFFF if null header.
 */
uint16_t goto_line(unsigned int gtl)
{
    unsigned int line;
    uint16_t nxtaddr;
    unsigned int line_to = gtl;
    uint16_t addr = START_BRAM;
    uint16_t ret = 0;

    while (1) {
        line = PEEKW(addr);
        nxtaddr = PEEKW(addr + 3);
        if (line == line_to
            ||
            isnull_hdr((const char *)nxtaddr)) {

            line_num = line;
            curr_addr = addr;
            ret = PEEKW(nxtaddr);
            break;
        }
        addr = nxtaddr;
        if (isaddr_oor(addr)) {
            break;
        }
    }

    return ret;
}

/*
 * Move the current line marker to the line specified in command arg.
 * Command is expected in prompt_buf before calling this function:
 * "g start | end | [line#]"
 */
void texted_goto(void)
{
    int n0, n1;
    unsigned int line_to;

    if (0 == checkbuf()) {
        return;
    }
    if (isnull_hdr((const char *)START_BRAM)) {
        prnerror(ERROR_NOTEXT);
        return;
    }

    n0 = adv2nxttoken(2);
    n1 = adv2nextspc(n0);
    if (n1 > n0) {
        if (0 == strncmp(prompt_buf + n0, "start", 5)) {
            line_to = 0;
        } else if (0 == strncmp(prompt_buf + n0, "end", 3)) {
            line_to = last_line;
        } else {
            line_to = atoi(prompt_buf + n0);
        }
    } else {
        line_to = line_num;
    }

    if (line_to > last_line) {
        prnerror(ERROR_BADARG);
        return;
    }
    goto_line(line_to);
}

/*
 * Delete current line or specified range of lines.
 * Command is expected in prompt_buf before this function is called:
 * "d [from_line#[-to_line#]]"
 */
void texted_delete(void)
{
    int n0, n1;
    unsigned int from_line = line_num;
    unsigned int to_line = line_num;

    if (0 == checkbuf()) {
        return;
    }
    if (isnull_hdr((const char *)START_BRAM)) {
        prnerror(ERROR_NOTEXT);
        return;
    }
    // parse command line
    if (strlen(prompt_buf) > 2) {
        n0 = adv2nxttoken(2);
        n1 = adv2nextspc(n0);
        if (n1 > n0) {
            from_line = atoi(prompt_buf + n0);
            n0 = adv2nxttoken(n1);
            n1 = adv2nextspc(n0);
            if (n1 > n0) {
                if (0 == strncmp((const char *)(prompt_buf + n0), "end", 3)) {
                    to_line = last_line;
                } else {
                    to_line = atoi(prompt_buf + n0);
                }
            } else {
                to_line = from_line; // only one argument supplied
            }
        }
    }
    if (from_line > to_line || from_line > last_line || to_line > last_line) {
        prnerror (ERROR_BADARG);
        return;
    }
    delete_text(from_line, to_line);
    checkbuf(); // update globals after text buffer was altered
}

/*
 * Delete text from buffer.
 */
void delete_text(unsigned int from_line, unsigned int to_line)
{
    uint16_t tmpaddr, endaddr;
    uint16_t addr = START_BRAM;

    if (to_line < from_line) {
        prnerror (ERROR_BADARG);
        return;
    }
    // the actual delete procedure
    // 1. Go to the from_line position.
    goto_line(from_line);
    tmpaddr = curr_addr; // remember current addr - we shift up to this addr
    // 2. Find address of the next line after to_line.
    goto_line(to_line);
    endaddr = PEEKW(curr_addr + 3);
    utoa((unsigned int)endaddr, ibuf1, RADIX_HEX);
    strcat(ibuf1, " ");
    strcat(ibuf1, utoa((unsigned int)lastaddr, ibuf2, RADIX_HEX));
    texted_assert(__LINE__, (endaddr <= lastaddr), ibuf1);
    // 3. Delete lines from_line .. to_line by moving text up in the buffer.
    memmove((void *)tmpaddr, (void *)endaddr,
            lastaddr - endaddr + strlen(null_txt_hdr));
    // renumerate lines and change pointer to next line in each
    addr = tmpaddr;
    lastaddr = lastaddr - (endaddr - tmpaddr);  // last address is now closer
    addr = renumber(addr, (from_line - to_line - 1));
    if (addr >= lastaddr && 0 == isaddr_oor(addr)) {
        strcpy((char *)addr, null_txt_hdr);
    }
}

/*
 * Select text in buffer.
 * Command is expected in prompt_buf before calling this function:
 * "s [from_line#[-to_line#]]"
 */
void texted_select(void)
{
    int n0, n1;
    unsigned int from_line = line_num;
    unsigned int to_line = line_num;

    if (0 == checkbuf()) {
        return;
    }
    if (isnull_hdr((const char *)START_BRAM)) {
        prnerror(ERROR_NOTEXT);
        return;
    }
    // parse command args
    if (strlen(prompt_buf) > 2) {
        n0 = adv2nxttoken(2);
        n1 = adv2nextspc(n0);
        if (n1 > n0) {
            from_line = atoi(prompt_buf + n0);
            n0 = adv2nxttoken(n1);
            n1 = adv2nextspc(n0);
            if (n1 > n0) {
                if (0 == strncmp((const char *)(prompt_buf + n0), "end", 3)) {
                    to_line = last_line;
                } else {
                    to_line = atoi(prompt_buf + n0);
                }
            } else {
                to_line = from_line;
            }
        }
    }
    if (from_line > last_line || to_line > last_line || to_line < from_line) {
        prnerror(ERROR_BADARG);
        return;
    }
    sel_begin = from_line;
    sel_end = to_line;
    copy2clipbrd(sel_begin, sel_end);
}

/*
 * Delete (cut) text selection.
 */
void texted_cut(void)
{
    if (0 == checkbuf()) {
        return;
    }
    if (isnull_hdr((const char *)START_BRAM)) {
        prnerror(ERROR_NOTEXT);
        return;
    }
    if (sel_end < sel_begin || sel_end > last_line || sel_begin > last_line) {
        prnerror(ERROR_BADARG);
        return;
    }
    delete_text(sel_begin, sel_end);
    checkbuf(); // update and validate global variables after text was deleted
}

/*
 * Initialization and command shell loop.
 */
void texted_shell(void)
{
    int cont = 1;

    while(cont)
    {
        puts("texted>");
        getline();
        parse_cmd();
        cont = exec_cmd();
    }
}

void texted_banner(void)
{
    pause_sec64(128);
    while (kbhit()) getc(); // flush RX buffer
    puts("\n\r");
    puts("Welcome to Text Editor ");
    puts(ver);
    puts("(C) Marek Karcz 2012-2018. All rights reserved.\n\r");
    puts("Type 'h' for guide.\n\r\n\r");
}

/*
 * Initialize text buffer by putting null text header 1-st.
 */
void texted_initbuf(void)
{
    puts("Initialize text buffer? (Y/N) ");
    getline();
    if (*prompt_buf == 'y' || *prompt_buf == 'Y') {
        strcpy((char *)START_BRAM, null_txt_hdr);
        line_num = 0;
        sel_begin = 0;
        sel_end = 0;
        bank_num = *RAMBANKNUM;
        curr_addr = START_BRAM;
        last_line = 0;
        line_count = 0;
        lastaddr = curr_addr + strlen(null_txt_hdr);
        bankinit_flags[bank_num] = 1;
    }
}

/*
 * Show / set current memory bank.
 * Command is expected in prompt_buf before calling this function:
 * "b [hex_bank#]" (e.g.: "b 02")
 */
void texted_membank(void)
{
    __asm__("jsr %w", MOS_BRAMSEL);

    if (*RAMBANKNUM != bank_num) {
        puts("WARNING: RAM bank has been switched.\n\r");
        puts("Cursor position and text selection markers were reset.\n\r");
        line_num = 0;
        sel_begin = 0;
        sel_end = 0;
        curr_addr = START_BRAM;
        last_line = 0;
        line_count = 0;
        bank_num = *RAMBANKNUM;
        texted_info();  // set bank_num, validate buffer and display info
    }
}

/*
 * Set bank_num, validate buffer and display text buffer and memory use info.
 */
void texted_info(void)
{
    checkbuf();
    puts(divider);
    puts("Current RAM bank#........: ");
    puts(utoa((unsigned int)bank_num, ibuf1, RADIX_DEC));
    puts("\n\r");
    puts("Current memory address...: $");
    puts(utoa((unsigned int)curr_addr, ibuf1, RADIX_HEX));
    puts("\n\r");
    puts("Next free memory address.: $");
    puts(utoa((unsigned int)lastaddr, ibuf1, RADIX_HEX));
    puts("\n\r");
    puts("Current line#............: ");
    puts(utoa((unsigned int)line_num, ibuf1, RADIX_DEC));
    puts("\n\r");
    puts("Last line#...............: ");
    puts(utoa((unsigned int)last_line, ibuf1, RADIX_DEC));
    puts("\n\r");
    puts("Lines in buffer..........: ");
    puts(utoa((unsigned int)line_count, ibuf1, RADIX_DEC));
    puts("\n\r");
    puts("Selected text lines from.: ");
    puts(utoa((unsigned int)sel_begin, ibuf1, RADIX_DEC));
    puts(", to.: ");
    puts(utoa((unsigned int)sel_end, ibuf1, RADIX_DEC));
    puts("\n\r");
    puts(divider);
}

/*
 * Sanity check.
 * Check the text buffer and set global variables.
 * Return non-zero if buffer has text or was ever initialized.
 * Return 0 if buffer has non-initialized (random) data.
 */
int checkbuf(void)
{
    uint16_t addr = START_BRAM;
    uint16_t lcount = MAX_LINES;
    int ret = 0;

    // RAM bank# sanity check:
    if (bank_num != *RAMBANKNUM) {
        puts("\n\rWARNING: Application RAM bank# is different than indicated by system variable.");
        puts("\n\rSystem bank#.....: ");
        puts(itoa(*RAMBANKNUM, ibuf1, RADIX_DEC));
        puts("\n\rApplication bank#: ");
        puts(itoa(bank_num, ibuf1, RADIX_DEC));
        puts("\n\rWhich value should be set?");
        puts("\n\r   a|A - app. / s|S - system / number - Other ");
        getline();
        puts("\n\r");
        if (*prompt_buf == 's' ||  *prompt_buf == 'S') {
            bank_num = *RAMBANKNUM;
        } else if (*prompt_buf != 'a' && *prompt_buf != 'A') {
            bank_num = atoi(prompt_buf);
            if (bank_num < 0 || bank_num > 7) {
                prnerror(ERROR_BADARG);
                bank_num = 0;
                puts("bank# = 0\n\r");
            }
        }
        __asm__("lda %v", bank_num);
        __asm__("jsr %w", MOS_BANKEDRAMSEL);
    }
    //bank_num = *RAMBANKNUM;
    if (0 == bankinit_flags[bank_num]) {
        // Check if the buffer was ever initialized.
        // Several protective measures applied to prevent this loop from
        // looping forever.
        puts("Text buffer sanity check ... ");
        set_timeout(30); // 30 seconds timeout
        while (lcount > 0 && 0 == is_timeout()) {

           if (isnull_hdr((const char *)addr)) {
               ret = 1;
               break;
           }
           addr = PEEKW(addr + 3);
           if (isaddr_oor(addr)) {
               break;
           }
           lcount--;
        }
        puts("done.\n\r");
    } else {
        ret = 1;
    }
    if (ret) {
        bankinit_flags[bank_num] = 1;   // set the flag, buffer is sane
        addr = START_BRAM;
        line_count = 0;
        // Find last line (pointing to null_txt_hdr) and line count.
        ret = 0;
        set_timeout(30);   // 30 seconds timeout
        lcount = MAX_LINES;
        while (lcount > 0 && 0 == is_timeout()) {
            if (isnull_hdr((const char *)addr)) {
                ret = 1;
                break;  // found last line
            }
            last_line = PEEKW(addr);
            addr = PEEKW(addr + 3);
            if (isaddr_oor(addr)) {
                break;
            }
            line_count++;
            lcount--;
        }
        if (ret) {
            lastaddr = addr + strlen(null_txt_hdr);
            if (0 == line_count
                || sel_begin > last_line
                || sel_end > last_line) {

                sel_begin = 0;
                sel_end = 0;
                puts("WARNING: Text selection was reset to line #0.\n\r");
            }
            if (line_num > last_line) {

                line_num = last_line;
                curr_addr = addr;
                puts("WARNING: Current line was reset to last line #");
                puts(utoa(line_num, ibuf1, RADIX_DEC));
                puts("\n\r");
            }
        } else {
            prnerror(ERROR_TIMEOUT);
            puts("Text buffer may be corrupted.");
            texted_initbuf();
            ret = isnull_hdr((const char *)START_BRAM);
        }
    } else {
        prnerror(ERROR_BUFNOTINIT);
        texted_initbuf();
        ret = isnull_hdr((const char *)START_BRAM);
    }

    return ret;
}

int main(void)
{
    int i;

    // initialize global variables
    line_num = 0;
    sel_begin = 0;
    sel_end = 0;
    bank_num = 0;
    curr_addr = START_BRAM;
    last_line = 0;
    line_count = 0;
    lastaddr = START_BRAM;

    // initialize text buffer init flags
    for (i=0; i<8; i++) {
        bankinit_flags[i] = 0;
    }
    texted_banner();
    // Select memory bank 0
    __asm__("lda %v", bank_num);
    __asm__("jsr %w", MOS_BANKEDRAMSEL);
    texted_info();
    texted_shell();
    return 0;
}
