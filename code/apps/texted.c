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
 *  ..........................................................................
 *  TO DO:
 *  1) Implement remaining functions from original requirements (insert, date
 *     and time, select text, delete selection, copy selection).
 *     STATUS: IN PROGRESS.
 *     DETAILS:
 *      Text selection added.
 *      Cut (delete) selection added.
 *      Function checkbuf() added which validates text buffer and adjusts
 *      global variables / flags.
 *      Insert function added.
 *  2) Optimize code for size and speed.
 *     STATUS: IN PROGRESS.
 *  3) Limit number of text buffer banks to 7 (0-6) and use the last bank (7)
 *     as a clipboard for copy operations.
 *     STATUS: NOT STARTED.
 *  ..........................................................................
 *  BUGS:
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

// Uncomment line(s) below to compile extra debug code.
//#define DEBUG
//#define DBG2

#define IBUF1_SIZE      20
#define IBUF2_SIZE      20
#define IBUF3_SIZE      20
#define PROMPTBUF_SIZE  80
#define TEXTLINE_SIZE   80
#define RADIX_DEC       10
#define RADIX_HEX       16
#define RADIX_BIN       2
#define MAX_LINES       ((END_BRAM-START_BRAM)/6)

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
	//-------------
	CMD_UNKNOWN
};

enum eErrors {
    ERROR_OK = 0,
    ERROR_BADCMD,
    ERROR_FULLBUF,
    ERROR_BADARG,
    ERROR_NTD,
    ERROR_NOTEXT,
    ERROR_BUFNOTINIT,
    ERROR_ADDROOR,
    ERROR_TIMEOUT,
    ERROR_UNKNOWN
};

const int ver_major = 1;
const int ver_minor = 2;
const int ver_build = 0;

// next pointer in last line's header in text buffer should point to
// such content in memory
const char null_txt_hdr[] = {0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0x00};
// this string is entered by operator to end text adding / insertion
const char *eot_str = "@EOT";

const char *ga_errmsg[11] =
{
    "OK.",
    "Unknown command.",
    "Buffer full. Enter @EOT to exit to prompt.",
    "Check arguments.",
    "Nothing to delete.",
    "No text in buffer.",
    "Buffer was never initialized.",
    "Address out of range.",
    "Timeout during text buffer search operation."
    "Unknown."
};

const char *helptext[37] =
{
    "\n\r",
    " b : select / show current file # (bank #).\n\r",
    "     b [00..07]\n\r",
    " l : List text buffer contents.\n\r",
    "     l all [n] | sel [n] | [from_line#[-to_line#]] [n]\n\r",
    "       n - show line numbers\n\r",
    "       all - list all content\n\r",
    "       sel - list only selected content\n\r",
    " a : start adding text at the end of buffer.\n\r",
    " i : start inserting text at line# (or current line).\n\r",
    "     i [line#]\n\r",
    "     NOTE: 'a' and 'i' functions - type '@EOT' in a new line\n\r",
    "           and press ENTER to finish and exit to command prompt.\n\r",
    " g : go to line.\n\r",
    "     g <line#>\n\r",
    " d : delete text.\n\r",
    "     d [from_line#[-to_line#]]\n\r",
    " s : select text.\n\r",
    "     s [from_line#[-to_line#]]\n\r",
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
    "NOTE:\n\r",
    "   Default line#, from_line#, to_line# is the current line.\n\r",
    "   Arguments are <mandatory> OR [optional].\n\r",
    "   When range is provided, SPACE can be used instead of '-'.\n\r",
    "\n\r",
    "@EOH"
};

struct text_line {
    unsigned int num;
    unsigned char len;
    uint16_t next_ptr;
    char text[TEXTLINE_SIZE];
} CurrLine;

int  cmd_code = CMD_NULL;
char prompt_buf[PROMPTBUF_SIZE];
char ibuf1[IBUF1_SIZE], ibuf2[IBUF2_SIZE], ibuf3[IBUF3_SIZE];
char bankinit_flags[8];
unsigned int line_num, sel_begin, sel_end, last_line, line_count;
uint16_t curr_addr, lastaddr;
int bank_num;
unsigned long tmr64;

void    getline(void);
void    parse_cmd(void);
void    texted_help(void);
void    texted_version(void);
void    texted_shell(void);
void    texted_banner(void);
void    prnerror(int errnum);
int     adv2nxttoken(int idx);
int     adv2nextspc(int idx);
void    texted_prndt(void);
int     exec_cmd(void);
void    texted_add(void);
void    texted_list(void);
void    texted_goto(void);
void    texted_delete(void);
void    pause_sec64(uint16_t delay);
int     isnull_hdr(const char *hdr);
void    texted_initbuf(void);
void    texted_membank(void);
void    texted_info(void);
void    texted_select(void);
void    delete_text(unsigned int from_line, unsigned int to_line);
void    texted_cut(void);
int     checkbuf(void);
void    texted_insert(void);
void    goto_line(unsigned int gtl);
int     isaddr_oor(uint16_t addr);
void    set_timeout(int secs);
int     is_timeout(void);
void    write_text2mem(uint16_t addr);

/*
 * Write text header and contents to the memory address addr.
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
    if (0 == strlen(prompt_buf))
    {
        cmd_code = CMD_NULL;
    }
    else if (*prompt_buf == 'l')
    {
        cmd_code = CMD_LIST;
    }
    else if (*prompt_buf == 'a')
    {
        cmd_code = CMD_ADD;
    }
    else if (0 == strncmp(prompt_buf, "g ", 2))
    {
        cmd_code = CMD_GOTO;
    }
    else if (*prompt_buf == 'd')
    {
        cmd_code = CMD_DELETE;
    }
    else if (*prompt_buf == 's')
    {
        cmd_code = CMD_SELECT;
    }
    else if (*prompt_buf == 'h')
    {
        cmd_code = CMD_HELP;
    }
    else if (*prompt_buf == 'p')
    {
        cmd_code = CMD_COPY;
    }
    else if (1 == strlen(prompt_buf) && *prompt_buf == 'x')
    {
        cmd_code = CMD_EXIT;
    }
    else if (1 == strlen(prompt_buf) && *prompt_buf == 'e')
    {
        cmd_code = CMD_ERASE;
    }
    else if (*prompt_buf == 't')
    {
        cmd_code = CMD_DTTM;
    }
    else if (*prompt_buf == 'b')
    {
        cmd_code = CMD_BANK;
    }
    else if (*prompt_buf == 'm')
    {
        cmd_code = CMD_INFO;
    }
    else if (*prompt_buf == 'i')
    {
        cmd_code = CMD_INSERT;
    }
    else if (*prompt_buf == 'c')
    {
        cmd_code = CMD_CUTSEL;
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
        if (0 == (i%20)) {
            puts("\n\r");
            puts("Press any key to continue...");
            while(!kbhit());    // wait for key press...
            getc();            // consume key
            puts("\n\r\n\r");
        }
    }
}

void texted_version(void)
{
    strcpy(ibuf1, itoa(ver_major, ibuf3, RADIX_DEC));
    strcat(ibuf1, ".");
    strcat(ibuf1, itoa(ver_minor, ibuf3, RADIX_DEC));
    strcat(ibuf1, ".");
    strcat(ibuf1, itoa(ver_build, ibuf3, RADIX_DEC));
    strcat(ibuf1, ".\n\r");
    puts(ibuf1);
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
        default:
            break;
    }

    return ret;
}

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
#ifdef DEBUG
        itoa(CurrLine.next_ptr, ibuf1, RADIX_HEX);
        puts ("DBG: next_ptr = ");
        puts (ibuf1);
        puts ("\n\r");
#endif
        if (CurrLine.next_ptr > (END_BRAM - 6)) {
            // Text buffer full, terminate text entry here:
            // Loop while consuming user input until magic string is entered.
            // Produce the error message each time new line is entered.
            // This is to prevent entering accidental command while loading
            // text via serial port - instead: ignore all incoming text after
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
 * Insert text in the middle of the buffer, at the beginning or at the end.
 */
void texted_insert(void)
{
    int n0 = 2;
    int n1;
    uint16_t addr, nxtaddr, lnum;

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
                while(1) {
                    if (isnull_hdr((const char *)addr)) {
                        break;
                    }
                    lnum = PEEKW(addr) + 1;
                    POKEW(addr, lnum);
                    nxtaddr = addr + PEEK(addr + 2) + 6;
                    if (isaddr_oor(nxtaddr)) {
                        return;
                    }
                    POKEW(addr + 3, nxtaddr);
                    addr = nxtaddr;
                }
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
 * List text buffer contents.
 */
void texted_list(void)
{
    uint16_t from_line = line_num;
    uint16_t to_line = line_num;
    uint16_t addr = START_BRAM;
    uint16_t line = 0;
    int      show_num = 0;
    int      n0, n;

    if (0 == checkbuf()) {
        return;
    }
    if (isnull_hdr((const char *)START_BRAM)) {
        prnerror(ERROR_NOTEXT);
        return;
    }
    if (strlen(prompt_buf) > 1) {
        n0 = adv2nxttoken(2);
        n = adv2nextspc(n0);
        if (0 == strncmp(prompt_buf + n0, "sel", 3)) {
#ifdef DEBUG
            puts("DBG(texted_list): List selected content.\n\r");
#endif
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
                        to_line = atoi (prompt_buf + n);
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
#ifdef DEBUG
            puts("DBG(texted_list): List all content.\n\r");
#endif
            n = adv2nxttoken(n);
            n0 = adv2nextspc(n);
            show_num = (prompt_buf[n] == 'n');
            from_line = 0;
            while (1) {
                if (isnull_hdr((const char *)addr)) {
#ifdef DEBUG
                puts("DBG(texted_list): Found null header at ");
                itoa(addr, ibuf1, RADIX_HEX);
                puts (ibuf1);
                puts (".\n\r");
#endif
                    break;  // found last line
                }
                to_line = PEEKW(addr);
                addr = PEEKW(addr + 3);
                if (isaddr_oor(addr)) {
                    return;
                }
#ifdef DEBUG
                itoa(to_line, ibuf1, RADIX_DEC);
                itoa(addr, ibuf2, RADIX_HEX);
                puts ("DBG(texted_list): to_line = ");
                puts (ibuf1);
                puts (", addr = $");
                puts (ibuf2);
                puts ("\n\r");
#endif
            }
            addr = START_BRAM;
        }
    }
#ifdef DEBUG
    itoa(from_line, ibuf1, RADIX_DEC);
    itoa(to_line, ibuf2, RADIX_DEC);
    puts ("DBG(texted_list): from_line = ");
    puts (ibuf1);
    puts (", ");
    puts ("to_line = ");
    puts (ibuf2);
    puts ("\n\r");
#endif
    //puts ("a\n\r");
    while (1) {
        if (isnull_hdr((const char *)addr)) {
#ifdef DEBUG
            puts("DBG(texted_list): Found null header at ");
            itoa(addr, ibuf1, RADIX_HEX);
            puts (ibuf1);
            puts (".\n\r");
#endif
            break; // EOT, null text header is next
        }
        line = PEEKW(addr);
#ifdef DEBUG
        itoa(line, ibuf1, RADIX_DEC);
        itoa(addr, ibuf2, RADIX_HEX);
        puts ("DBG(texted_list): line = ");
        puts (ibuf1);
        puts (", ");
        puts ("addr = $");
        puts (ibuf2);
        puts (".\n\r");
#endif
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
    //puts("@EOT\n\r");
}

/*
 * Position current line at gtl.
 */
void goto_line(unsigned int gtl)
{
    unsigned int line_to = gtl;
    uint16_t addr = START_BRAM;
    unsigned int line;
    uint16_t nxtaddr;

    while (1) {
        line = PEEKW(addr);
#ifdef DEBUG
        itoa(line, ibuf1, RADIX_DEC);
        itoa(addr, ibuf2, RADIX_HEX);
        puts ("DBG(goto_line): line = ");
        puts (ibuf1);
        puts (", ");
        puts ("addr = $");
        puts (ibuf2);
        puts ("\n\r");
#endif
        nxtaddr = PEEKW(addr + 3);
        if (line == line_to
            ||
            isnull_hdr((const char *)nxtaddr)) {

            line_num = line;
            curr_addr = addr;
#ifdef DEBUG
            itoa(line_num, ibuf1, RADIX_DEC);
            itoa(addr, ibuf2, RADIX_HEX);
            puts ("DBG(goto_line): line_num = ");
            puts (ibuf1);
            puts (", ");
            puts ("addr = $");
            puts (ibuf2);
            puts ("\n\r");
#endif
            break;
        }
        addr = nxtaddr;
        if (isaddr_oor(addr)) {
            return;
        }
    }
}

/*
 * Move the current line marker to the line specified in command arg.
 */
void texted_goto(void)
{
    int n;
    unsigned int line_to;

    if (0 == checkbuf()) {
        return;
    }
    if (isnull_hdr((const char *)START_BRAM)) {
        prnerror(ERROR_NOTEXT);
        return;
    }

    n = adv2nxttoken(2);
    adv2nextspc(n);
    line_to = atoi(prompt_buf + n);

    if (line_to > last_line) {
        prnerror(ERROR_BADARG);
        return;
    }
    goto_line(line_to);
}

/*
 * Delete current line or specified range of lines.
 */
void texted_delete(void)
{
    uint16_t addr = START_BRAM;
    unsigned int from_line, to_line;
    int n1, n2;

    if (0 == checkbuf()) {
        return;
    }
    if (isnull_hdr((const char *)START_BRAM)) {
        prnerror(ERROR_NOTEXT);
        return;
    }
    // current line is default
    from_line = line_num;
    to_line = line_num;
    // parse command line
    if (strlen(prompt_buf) > 2) {
        n1 = adv2nxttoken(2);
        n2 = adv2nextspc(n1);
        from_line = atoi(prompt_buf + n1);
        n1 = adv2nxttoken(n2);
        n2 = adv2nextspc(n1);
        if (n2 > n1) {
            to_line = atoi(prompt_buf + n1);
        } else {
            to_line = from_line; // only one argument supplied
        }
    }
    if (from_line > to_line) {
        prnerror (ERROR_BADARG);
        return;
    }
    //n1 = to_line - from_line + 1;   // number of lines to delete
#ifdef DEBUG
    itoa(from_line, ibuf1, RADIX_DEC);
    itoa(to_line, ibuf2, RADIX_DEC);
    puts ("DBG(texted_delete): from_line = ");
    puts (ibuf1);
    puts (", ");
    puts ("to_line = ");
    puts (ibuf2);
    puts ("\n\r");
#endif
    delete_text(from_line, to_line);
    checkbuf(); // update globals after text buffer was altered
}

/*
 * Delete text from buffer.
 */
void delete_text(unsigned int from_line, unsigned int to_line)
{
    uint16_t addr = START_BRAM;
    uint16_t nxtaddr, tmpaddr, endaddr;
    unsigned int line;
    int n2;

    // the actual delete procedure
    // 1. Go to the from_line position.
    addr = START_BRAM;
    while (1) {
        line = PEEKW(addr);
        nxtaddr = PEEKW(addr + 3);
        if (line == from_line) {
            break;  // found it
        }
        if (isnull_hdr((const char *)nxtaddr)) {
            // something is wrong, from_line is not found
            prnerror (ERROR_BADARG);
            return;
        }
        addr = nxtaddr;
        if (isaddr_oor(addr)) {
            return;
        }
    }
    tmpaddr = addr; // remember current addr - we shift up to this addr
    // 2. Find address of the next line after to_line.
    while (1) {
        line = PEEKW(addr);
        nxtaddr = PEEKW(addr + 3);
        endaddr = nxtaddr;
        if (line == to_line) {
            break;
        }
        if (isnull_hdr((const char *)nxtaddr)) {
            break; // end of text
        }
        addr = nxtaddr;
        if (isaddr_oor(addr)) {
            return;
        }
    }
#ifdef DEBUG
    itoa(tmpaddr, ibuf1, RADIX_HEX);
    itoa(endaddr, ibuf2, RADIX_HEX);
    itoa(lastaddr, ibuf3, RADIX_HEX);
    puts ("DBG(delete_text): tmpaddr (begin) = $");
    puts (ibuf1);
    puts (", ");
    puts ("endaddr = $");
    puts (ibuf2);
    puts (", ");
    puts ("lastaddr = $");
    puts (ibuf3);
    puts ("\n\r");
#endif
    if (lastaddr <= endaddr) {
        // nothing to  delete
        prnerror(ERROR_NTD);
        return;
    }
    // 3. Delete lines from_line .. to_line by moving text up in the buffer.
    memmove((void *)tmpaddr, (void *)endaddr,
            lastaddr - endaddr + strlen(null_txt_hdr));
    // renumerate lines and change pointer to next line in each
    addr = tmpaddr;
    lastaddr = lastaddr - (endaddr - tmpaddr);  // last address is now closer
    while (1) {
        if (isnull_hdr((const char *)addr)) {
            // reached the null header, leave it in place and break
#ifdef DEBUG
            itoa(addr, ibuf1, RADIX_HEX);
            puts ("DBG(delete_text): Renumbering, found null header at ");
            puts (ibuf1);
            puts (".\n\r");
#endif
            break;
        }
        line = PEEKW(addr);
        // line# = line# - #-of-deleted-lines (lines were shifted up)
        POKEW(addr, line - (to_line - from_line + 1));
        n2 = PEEK(addr + 2);    // get length of the text in current line
        nxtaddr = addr + n2 + 6; // calculate new pointer to next line
        POKEW(addr + 3, nxtaddr);   // store new pointer to next line
        if (nxtaddr >= lastaddr) {
            // reached the end, we're done, store null header and break
            // NOTE:
            // this should not happen as the first null header compare test
            // above should have caugt this - addresses seem to be out of
            // alignment
            strcpy((char *)nxtaddr, null_txt_hdr);
#ifdef DEBUG
            itoa(addr, ibuf1, RADIX_HEX);
            puts ("DBG(delete_text): WARNING!!! Renumbering, found null header at ");
            puts (ibuf1);
            puts (".\n\r");
            puts ("DBG(delete_text): Addresses may be out of alignment.\n\r");
            puts ("DBG(delete_text): Check your algorithm!\n\r");
#endif
            break;
        }
        addr = nxtaddr;             // do the next line
        if (isaddr_oor(addr)) {
            return;
        }
    }
}

/*
 * Select text in buffer.
 */
void texted_select(void)
{
    uint16_t addr = START_BRAM;
    unsigned int from_line = line_num;
    unsigned int to_line = line_num;
    int n0, n1;

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
        from_line = atoi(prompt_buf + n0);
        n0 = adv2nxttoken(n1);
        n1 = adv2nextspc(n0);
        if (n1 > n0) {
#ifdef DEBUG
            puts("DBG(texted_select): 2-nd argument detected.\n\r");
#endif
            to_line = atoi(prompt_buf + n0);
        } else {
            to_line = from_line;
        }
    }
#ifdef DEBUG
    itoa(from_line, ibuf1, RADIX_DEC);
    itoa(to_line, ibuf2, RADIX_DEC);
    itoa(last_line, ibuf3, RADIX_DEC);
    puts("DBG(texted_select): from_line: ");
    puts(ibuf1);
    puts(", to_line: ");
    puts(ibuf2);
    puts(", last_line: ");
    puts(ibuf3);
    puts("\n\r");
#endif
    if (from_line > last_line || to_line > last_line || to_line < from_line) {
        prnerror(ERROR_BADARG);
        return;
    }
    sel_begin = from_line;
    sel_end = to_line;
#ifdef DEBUG
    itoa(sel_begin, ibuf1, RADIX_DEC);
    itoa(sel_end, ibuf2, RADIX_DEC);
    puts("DBG(texted_select): begin: ");
    puts(ibuf1);
    puts(", end: ");
    puts(ibuf2);
    puts("\n\r");
#endif
}

/*
 * Delete (cut) text selection.
 */
void texted_cut(void)
{
    uint16_t addr = START_BRAM;

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
    memset(prompt_buf,0,PROMPTBUF_SIZE);
    puts("\n\r");
    puts("Welcome to Text Editor ");
    texted_version();
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
 */
void texted_membank(void)
{
    __asm__("jsr %w", MOS_BRAMSEL);

    if (*RAMBANKNUM != bank_num) {
        puts("RAM bank has been switched.\n\r");
        puts("Cursor position and text selection markers were reset.\n\r");
        line_num = 0;
        sel_begin = 0;
        sel_end = 0;
        curr_addr = START_BRAM;
        last_line = 0;
        line_count = 0;
        texted_info();  // set bank_num, validate buffer and display info
    }
}

/*
 * Set bank_num, validate buffer and display text buffer and memory use info.
 */
void texted_info(void)
{
    bank_num = *RAMBANKNUM;
    checkbuf();
    puts("--------------------------------------------\n\r");
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
    puts("--------------------------------------------\n\r");
#ifdef DBG2
    puts("DBG2(texted_info): START_BRAM = $");
    puts(utoa((unsigned int)START_BRAM, ibuf1, RADIX_HEX));
    puts(", END_BRAM = $");
    puts(utoa((unsigned int)END_BRAM, ibuf1, RADIX_HEX));
    puts("\n\r");
    puts("DBG2(texted_info): MAX_LINES = ");
    puts(utoa((unsigned int)MAX_LINES, ibuf1, RADIX_DEC));
    puts("\n\r");
#endif
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

    bank_num = *RAMBANKNUM;
    if (0 == bankinit_flags[bank_num]) {
        // Check if the buffer was ever initialized.
        // Several protective measures applied to prevent this loop from
        // looping forever.
        puts("Text buffer sanity check ... ");
#ifdef DBG2
        puts("\n\r");
#endif
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
#ifdef DBG2
        puts("DBG2(checkbuf): tmr64 = ");
        puts(ltoa((long)tmr64, ibuf1, RADIX_DEC));
        puts(", *TIMER64HZ = ");
        puts(ltoa((long)(*TIMER64HZ), ibuf1, RADIX_DEC));
        puts("\n\r");
        puts("DBG2(checkbuf): lcount = ");
        puts(utoa((unsigned int)lcount, ibuf1, RADIX_DEC));
        puts("\n\r");
#endif
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
#ifdef DBG2
                puts("DBG2(checkbuf): null hdr found, addr = $");
                puts(utoa((unsigned int)addr, ibuf1, RADIX_HEX));
                puts("\n\r");
#endif
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
#ifdef DBG2
        puts("DBG2(checkbuf): tmr64 = ");
        puts(ltoa((long)tmr64, ibuf1, RADIX_DEC));
        puts(", *TIMER64HZ = ");
        puts(ltoa((long)(*TIMER64HZ), ibuf1, RADIX_DEC));
        puts("\n\r");
        puts("DBG2(checkbuf): lcount = ");
        puts(utoa((unsigned int)lcount, ibuf1, RADIX_DEC));
        puts("\n\r");
#endif
        if (ret) {
            lastaddr = addr + strlen(null_txt_hdr);
            if (0 == line_count || sel_begin > last_line || sel_end > last_line) {
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
    //texted_initbuf();
    texted_info();
    #ifdef DEBUG
        itoa(curr_addr, ibuf1, RADIX_HEX);
        puts ("DBG: curr_addr = $");
        puts (ibuf1);
        puts ("\n\r");
    #endif
    texted_shell();
    return 0;
}
