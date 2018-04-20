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
    CMD_ERASE,
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
    ERROR_FULLBUF,
    ERROR_BADBANK,
    ERROR_BANKSEQUAL,
    //------------------
    ERROR_UNKNOWN
};

// Error messages.
const char *ga_errmsg[12] =
{
    "OK.",
    "Unknown command.",
    "Check arguments.",
    "No text in buffer.",
    "Buffer was never initialized.",
    "Address out of range.",
    "Timeout during text buffer search operation.",
    "Buffer full.",
    "Invalid RAM bank#. (expected: 0..7)",
    "Code and output buffers must be in different banks.",
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

enum eAddrModes {
    IMM = 0,    // #aa
    ABS,        // aaaa
    ZPG,        // aa
    IMP,
    INDABS,     // (aaaa)
    ABSINX,     // aaaa,X
    ABSINY,     // aaaa,Y
    ZPGINX,     // aa,X
    ZPGINY,     // aa,Y
    INXIND,     // (aa,X)
    INDINY,     // (aa),Y
    REL,        // aaaa
    ACC         // A
};

struct instr {
    char mnem[4];   // assembly mnemonic of the instruction
    unsigned char opcode[14];   // op-codes for each addressing mode
};

const struct instr OpCodes[4] = {
    {"adc", {0x69, 0x6D, 0x65, 0x00, 0x00, 0x7D, 0x79, 0x75, 0x00, 0x61, 0x71, 0x00, 0x00, 0x00}},
    {"and", {0x29, 0x2D, 0x25, 0x00, 0x00, 0x3D, 0x39, 0x35, 0x00, 0x21, 0x31, 0x00, 0x00, 0x00}},
    {"asl", {0x00, 0x0E, 0x06, 0x00, 0x00, 0x1E, 0x00, 0x16, 0x00, 0x00, 0x00, 0x00, 0x0A, 0x00}},
    {"", {0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0}}
};

// globals
int  cmd_code = CMD_NULL;
char prompt_buf[PROMPTBUF_SIZE];
char ibuf1[IBUF1_SIZE], ibuf2[IBUF2_SIZE], ibuf3[IBUF3_SIZE];
char bankinit_flags[8];
unsigned int line_num, line_count, last_line;
uint16_t curr_addr, lastaddr;
int code_bank_num, out_bank_num, bank_num;
unsigned long tmr64;

// Functions prototypes.
void        get_line(void);
void        parse_cmd(void);
void        prn_help(void);
void        cmd_shell(void);
void        prn_banner(void);
void        prn_error(int errnum);
int         adv2nxt_token(int idx);
int         adv2next_spc(int idx);
int         exec_cmd(void);
void        list_buf(void);
void        pause_sec64(uint16_t delay);
int         isnull_hdr(const char *hdr);
void        init_buf(void);
void        set_buffers(void);
void        mem_info(void);
int         check_buf(int bank);
int         isaddr_oor(uint16_t addr);
void        set_timeout(int secs);
int         is_timeout(void);
void        write_text2mem(uint16_t addr);
void        assert(uint16_t line, int cond, const char *msg);
void        add2output(const char *txt);
uint16_t    goto_line(unsigned int gtl);
void        delete_text(unsigned int from_line, unsigned int to_line);
uint16_t    renumber(uint16_t addr, int offs);

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
        prn_error(ERROR_ADDROOR);
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
int adv2nxt_token(int idx)
{
    while (prompt_buf[idx] == 32) {
        idx++;
    }

    return idx;
}

/*
 * Advance index to next space.
 */
int adv2next_spc(int idx)
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
void prn_error(int errnum)
{
    puts("\n\rERROR: ");
    puts(ga_errmsg[errnum]);
    puts("\r\n");
}

/* get line of text from input */
void get_line(void)
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
void prn_help(void)
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
            prn_error(ERROR_BADCMD);
            break;
        case CMD_EXIT:
            puts("Bye!\n\r");
            ret = 0;
            break;
        case CMD_HELP:
            prn_help();
            break;
        case CMD_LIST:
            list_buf();
            break;
        case CMD_ERASE:
            init_buf();
            break;
        case CMD_BANK:
            set_buffers();
            break;
        case CMD_INFO:
            mem_info();
            break;
        default:
            break;
    }

    return ret;
}

/*
 *  Add single line of text at the end of the text buffer.
 */
void add2output(const char *txt)
{
    uint16_t nxtaddr;

    curr_addr = START_BRAM;
    line_num = 0;
    if (0 == check_buf(out_bank_num)) {
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
    // create and store null text header at current address
    strcpy((char *)curr_addr, null_txt_hdr);
    strcpy (CurrLine.text, txt);
    CurrLine.len = strlen(txt);
    CurrLine.next_ptr = curr_addr + CurrLine.len + 6;
    if (CurrLine.next_ptr > (END_BRAM - 6)) {
        // Text buffer full.
        prn_error(ERROR_FULLBUF);
    } else {
        // update header and text at current address
        CurrLine.num = line_num;
        write_text2mem(curr_addr);
        line_num++;
        curr_addr = CurrLine.next_ptr;
        if (isaddr_oor(curr_addr)) {
            return;
        }
    }
    check_buf(out_bank_num); // update globals after text buffer was altered
}

/*
 * List text buffer contents.
 * Command is expected in prompt_buf before calling this function:
 * "l <src | out> <all | from_line#>[-to_line# | -end] [n]"
 */
void list_buf(void)
{
    int      n0, n;
    uint16_t from_line = line_num;
    uint16_t to_line = line_num;
    uint16_t addr = START_BRAM;
    uint16_t line = 0;
    int      show_num = 0;

    if (strlen(prompt_buf) > 1) {

        n0 = adv2nxt_token(2);
        n = adv2next_spc(n0);

        if (n <= n0) {
            prn_error(ERROR_BADARG);
            puts ("Expected: src | out\n\r");
            return;
        }

        if (0 == strncmp(prompt_buf + n0, "src", 3)) {

            bank_num = code_bank_num;

        } else if (0 == strncmp(prompt_buf + n0, "out", 3)) {

            bank_num = out_bank_num;

        } else {

            prn_error(ERROR_BADARG);
            puts ("Expected: src | out\n\r");
            return;

        }

        // now that we know which buffer / bank#, check the sanity
        if (0 == check_buf(bank_num)) {
            return;
        }

        n = adv2nxt_token(n);
        n0 = adv2next_spc(n);

        if (n0 <= n) {
            prn_error(ERROR_BADARG);
            puts ("Expected: range argument(s)\n\r");
            return;
        }

        if (0 == strncmp(prompt_buf + n, "all", 3)) {

            from_line = 0;
            to_line = last_line;

        } else {

            from_line = atoi(prompt_buf + n);
            n = adv2nxt_token(n0);
            n0 = adv2next_spc(n);
            if (n0 > n) {
                if (0 == strncmp(prompt_buf + n, "end", 3)) {
                    to_line = last_line;
                } else if (*(prompt_buf + n) == 'n') {
                    show_num = 1;
                } else {
                    to_line = atoi(prompt_buf + n);
                    n = adv2nxt_token(n0);
                    n0 = adv2next_spc(n);
                    if (n0 > n) {
                        if (*(prompt_buf + n) == 'n') {
                            show_num = 1;
                        } else {
                            prn_error(ERROR_BADARG);
                            puts("Expected: n\n\r");
                            return;
                        }
                    }
                }
            }
        }
    }
    // set corresponding buffer's bank number
    __asm__("lda %v", bank_num);
    __asm__("jsr %w", MOS_BANKEDRAMSEL);
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
 * Delete text from buffer.
 */
void delete_text(unsigned int from_line, unsigned int to_line)
{
    uint16_t tmpaddr, endaddr;
    uint16_t addr = START_BRAM;

    if (to_line < from_line) {
        prn_error (ERROR_BADARG);
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
    assert(__LINE__, (endaddr <= lastaddr), ibuf1);
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
        get_line();
        parse_cmd();
        cont = exec_cmd();
    }
}

void prn_banner(void)
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
void init_buf(void)
{
    puts("Initialize output buffer? (Y/N) ");
    get_line();
    if (*prompt_buf == 'y' || *prompt_buf == 'Y') {
        bank_num = out_bank_num;
        __asm__("lda %v", bank_num);
        __asm__("jsr %w", MOS_BANKEDRAMSEL);
        strcpy((char *)START_BRAM, null_txt_hdr);
        line_num = 0;
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
void set_buffers(void)
{
    int n0, n1;

    n0 = adv2nxt_token(2);
    n1 = adv2next_spc(n0);
    if (n1 > n0) {
        code_bank_num = atoi(prompt_buf + n0);
        n0 = adv2nxt_token(n1);
        n1 = adv2next_spc(n0);
        if (n1 > n0) {
            out_bank_num = atoi(prompt_buf + n0);
        }
    } else {
        prn_error(ERROR_BADARG);
        puts("Expected: src_code_bank#\n\r");
    }
    bank_num = code_bank_num;
    mem_info();  // set bank_num, validate buffer and display info
}

/*
 * Set bank_num, validate buffer and display text buffer and memory use info.
 */
void mem_info(void)
{
    check_buf(code_bank_num);
    puts(divider);
    puts("Source code bank#........: ");
    puts(itoa(code_bank_num, ibuf1, RADIX_DEC));
    puts("\n\rNext free memory address.: $");
    puts(utoa((unsigned int)lastaddr, ibuf1, RADIX_HEX));
    puts("\n\rLast line#...............: ");
    puts(utoa((unsigned int)last_line, ibuf1, RADIX_DEC));
    puts("\n\rLines in buffer..........: ");
    puts(utoa((unsigned int)line_count, ibuf1, RADIX_DEC));
    puts("\n\r");
    puts(divider);
    check_buf(out_bank_num);
    puts(divider);
    puts("Output bank#.............: ");
    puts(itoa(out_bank_num, ibuf1, RADIX_DEC));
    puts("\n\rNext free memory address.: $");
    puts(utoa((unsigned int)lastaddr, ibuf1, RADIX_HEX));
    puts("\n\rLast line#...............: ");
    puts(utoa((unsigned int)last_line, ibuf1, RADIX_DEC));
    puts("\n\rLines in buffer..........: ");
    puts(utoa((unsigned int)line_count, ibuf1, RADIX_DEC));
    puts("\n\r");
    puts(divider);
}

/*
 * Sanity check.
 * Check the text buffer located in bank# = bank and set global variables.
 * Return non-zero if buffer has text or was ever initialized.
 * Return 0 if buffer has non-initialized (random) data or the code and
 * output buffers are in the same banks or bank# is out of range.
 */
int check_buf(int bank)
{
    uint16_t addr = START_BRAM;
    uint16_t lcount = MAX_LINES;
    int ret = 0;

    if (code_bank_num == out_bank_num) {
        prn_error(ERROR_BANKSEQUAL);
        return ret;
    }
    if (bank < 0 || bank > 7) {
        prn_error(ERROR_BADBANK);
        return ret;
    }
    if (0 == bankinit_flags[bank]) {
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
        bankinit_flags[bank] = 1;   // set the flag, buffer is sane
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
            if (line_num > last_line) {

                line_num = last_line;
                curr_addr = addr;
                puts("WARNING: Current line was reset to last line #");
                puts(utoa(line_num, ibuf1, RADIX_DEC));
                puts("\n\r");
            }
        } else {
            prn_error(ERROR_TIMEOUT);
            puts("Text buffer may be corrupted.");
            init_buf();
            ret = isnull_hdr((const char *)START_BRAM);
        }
    } else {
        prn_error(ERROR_BUFNOTINIT);
        init_buf();
        ret = isnull_hdr((const char *)START_BRAM);
    }

    return ret;
}

int main(void)
{
    int i;

    // initialize global variables
    line_num = 0;
    code_bank_num = 0;
    out_bank_num = 1;
    bank_num = 0;
    curr_addr = START_BRAM;
    last_line = 0;
    line_count = 0;
    lastaddr = START_BRAM;

    // initialize text buffer init flags
    for (i=0; i<8; i++) {
        bankinit_flags[i] = 0;
    }
    prn_banner();
    // Select memory bank 0
    __asm__("lda %v", bank_num);
    __asm__("jsr %w", MOS_BANKEDRAMSEL);
    mem_info();
    cmd_shell();
    return 0;
}
