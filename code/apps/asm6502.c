/*
 *
 * File:	asm6502.c
 * Purpose:	Code of 6502 assembler (assembly language compiler).
 * Author:	Marek Karcz
 * Created:	4/18/2018
 *
 * Revision history:
 *
 *  ..........................................................................
 *  TO DO:
 *
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

#define IBUF1_SIZE      20
#define IBUF2_SIZE      20
#define IBUF3_SIZE      20
#define PROMPTBUF_SIZE  80
#define TEXTLINE_SIZE   80
#define RADIX_DEC       10
#define RADIX_HEX       16
#define RADIX_BIN       2
#define MAX_LINES       ((END_BRAM-START_BRAM)/6)

// command codes
enum cmdcodes
{
    CMD_NULL = 0,
    CMD_HELP,
    CMD_EXIT,
    CMD_LIST,
    CMD_BANK,
    CMD_INFO,
	//-------------
	CMD_UNKNOWN
};

const char *ver = "1.0.0.\n\r";

// next pointer in last line's header in text buffer should point to
// such content in memory
const char null_txt_hdr[] = {0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0x00};

// some other commonly used constants, defining them here and using pointers
// saves some code space
const char *divider = "--------------------------------------------\n\r";

// error codes
enum eErrors {
    ERROR_OK = 0,
    ERROR_BADCMD,
    ERROR_BADARG,
    ERROR_NOTEXT,
    ERROR_BUFNOTINIT,
    ERROR_ADDROOR,
    ERROR_TIMEOUT,
    //------------------
    ERROR_UNKNOWN
};

// Error messages.
const char *ga_errmsg[9] =
{
    "OK.",
    "Unknown command.",
    "Check arguments.",
    "No text in buffer.",
    "Buffer was never initialized.",
    "Address out of range.",
    "Timeout during text buffer search operation.",
    "Unknown."
};

// Usage help.
const char *helptext[24] =
{
    "\n\r",
    " b : set buffers (bank #-s) for source code and for output\n\r",
    "     b <src_bank#> [out_bank#]\n\r",
    " l : List buffer contents.\n\r",
    "     l <src | out> <all | from_line#>[-to_line# | -end] [n]\n\r",
    "      src - source code buffer\n\r",
    "      out - output buffer\n\r",
    "      n - show line numbers\n\r",
    "      all - list all content\n\r",
    " a : assembly (compile) source code.\n\r",
    "     NOTE: output will be appended at the end of output buffer.\n\r",
    "           This way it is possible to compile multiple source code\n\r",
    "           buffers into single output buffer.\n\r",
    " x : exit program, leave contents in buffers.\n\r",
    " e : erase output buffer.\n\r",
    " m : show memory information and symbols table.\n\r",
    "NOTE:\n\r",
    "   Default to_line# is the same as from_line#.\n\r",
    "   Arguments are <mandatory> OR [optional].\n\r",
    "   For range arguments, SPACE can be used instead of '-'.\n\r",
    "   'end' - last line of text.\n\r",
    "\n\r",
    "@EOH"
};

// Buffer for current source code record (line).
struct text_line {
    unsigned int num;
    unsigned char len;
    uint16_t next_ptr;
    char text[TEXTLINE_SIZE];
} CurrLine;

// globals
int  cmd_code = CMD_NULL;
char prompt_buf[PROMPTBUF_SIZE];
char ibuf1[IBUF1_SIZE], ibuf2[IBUF2_SIZE], ibuf3[IBUF3_SIZE];
char bankinit_flags[8];
unsigned int line_num, line_count;
uint16_t curr_addr, lastaddr;
int code_bank_num, out_bank_num;
unsigned long tmr64;

// Functions prototypes.
void        getline(void);
void        parse_cmd(void);
void        prnhelp(void);
void        cmd_shell(void);
void        prnbanner(void);
void        prnerror(int errnum);
int         adv2nxttoken(int idx);
int         adv2nextspc(int idx);
int         exec_cmd(void);
void        listbuf(void);
void        pause_sec64(uint16_t delay);
int         isnull_hdr(const char *hdr);
void        initbuf(void);
void        setbuffers(void);
void        meminfo(void);
int         checkbuf(void);
int         isaddr_oor(uint16_t addr);
void        set_timeout(int secs);
int         is_timeout(void);
void        write_text2mem(uint16_t addr);
void        assert(uint16_t line, int cond, const char *msg);
void        add2output(const char *txt);
uint16_t    goto_line(unsigned int gtl);
void        delete_text(unsigned int from_line, unsigned int to_line);

////////////////////////////// CODE /////////////////////////////////////

/*
 * If the condition evaluates to zero (false), prints a message and aborts
 * the program.
 */
void assert(uint16_t line, int cond, const char *msg)
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
    else if (*prompt_buf == 'h') {
        cmd_code = CMD_HELP;
    }
    else if (1 == strlen(prompt_buf) && *prompt_buf == 'x') {
        cmd_code = CMD_EXIT;
    }
    else if (1 == strlen(prompt_buf) && *prompt_buf == 'e') {
        cmd_code = CMD_ERASE;
    }
    else if (*prompt_buf == 'b') {
        cmd_code = CMD_BANK;
    }
    else if (*prompt_buf == 'm') {
        cmd_code = CMD_INFO;
    }
    else
        cmd_code = CMD_UNKNOWN;
}

/* print help */
void prnhelp(void)
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
        case CMD_LIST:
            listbuf();
            break;
        case CMD_ERASE:
            initbuf();
            break;
        case CMD_BANK:
            setbuffers();
            break;
        case CMD_INFO:
            meminfo();
            break;
        default:
            break;
    }

    return ret;
}

/*
 *  Add text at the end of the text buffer.
 */
void add2output(const char *txt)
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
        if (0 == strncmp(eot_str, txt, strlen(eot_str))) {
            break;  // user entered command to end text entry
        }
        strcpy (CurrLine.text, txt);
        CurrLine.len = strlen(txt);
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
                if (0 == strncmp(eot_str, txt, strlen(eot_str))) {
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
 * List text buffer contents.
 * Command is expected in prompt_buf before calling this function:
 * "l <src | out> <all | from_line#>[-to_line# | -end] [n]"
 */
void listbuf(void)
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
 * Initialization and command shell loop.
 */
void cmd_shell(void)
{
    int cont = 1;

    while(cont)
    {
        puts("asm6502>");
        getline();
        parse_cmd();
        cont = exec_cmd();
    }
}

void prnbanner(void)
{
    pause_sec64(128);
    while (kbhit()) getc(); // flush RX buffer
    puts("\n\r");
    puts("Welcome to MOS 6502 Assembler ");
    puts(ver);
    puts("(C) Marek Karcz 2018. All rights reserved.\n\r");
    puts("Type 'h' for guide.\n\r\n\r");
}

/*
 * Initialize text buffer by putting null text header 1-st.
 */
void initbuf(void)
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
void setbuffers(void)
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
void meminfo(void)
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
    prnbanner();
    // Select memory bank 0
    __asm__("lda %v", bank_num);
    __asm__("jsr %w", MOS_BANKEDRAMSEL);
    meminfo();
    cmd_shell();
    return 0;
}
