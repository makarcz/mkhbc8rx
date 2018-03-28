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
 *
 *  ..........................................................................
 *  TO DO:
 *  1) Introduce the 'magic' string at the beginning of the text memory buffer.
 *     This string if present will indicate that the buffer was initialized
 *     by this application and it is safe to perform any scans of the memory.
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

// Uncomment line below to compile extra debug code.
//#define DEBUG
#define IBUF1_SIZE      20
#define IBUF2_SIZE      20
#define IBUF3_SIZE      20
#define PROMPTBUF_SIZE  80
#define TEXTLINE_SIZE   80
#define RADIX_DEC       10
#define RADIX_HEX       16
#define RADIX_BIN       2

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
    ERROR_UNKNOWN
};

const int ver_major = 1;
const int ver_minor = 0;
const int ver_build = 6;

// next pointer in last line's header in text buffer should point to
// such content in memory
const char null_txt_hdr[] = {0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0x00};
// this string is entered by operator to end text adding / insertion
const char *eot_str = "@EOT";

const char *ga_errmsg[8] =
{
    "OK.",
    "Unknown command.",
    "Buffer full.",
    "Check arguments.",
    "Nothing to delete.",
    "No text in buffer.",
    "Unknown."
};

const char *helptext[34] =
{
    "\n\r",
    " b : select / show current file # (bank #).\n\r",
    "     b [00..07]\n\r",
    " l : List text buffer contents.\n\r",
    "     l all | [from-line#][-to-line#] [n]\n\r",
    "       n - show line numbers\n\r",
    "       all - list all content\n\r",
    " a : add text at the end of buffer.\n\r",
    "     NOTE: type '@EOT' to end.\n\r",
    " i : insert text.\n\r",
    "     i [line#]\n\r",
    " g : go to.\n\r",
    "     g <line#>\n\r",
    " d : delete text.\n\r",
    "     d [from-line#][-to-line#]\n\r",
    " s : select text.\n\r",
    "     s [from-line#][-to-line#]\n\r",
    " c : delete (cut) selection\n\r",
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
    "   Default line# is current line.\n\r",
    "   Arguments are <mandatory> OR [optional].\n\r",
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
unsigned int line_num, sel_begin, sel_end;
uint16_t curr_addr;
int bank_num;

void    texted_getline(void);
void    texted_parse(void);
void    texted_help(void);
void    texted_version(void);
void    texted_shell(void);
void    texted_banner(void);
void    texted_prnerror(int errnum);
int     adv2nxttoken(int idx);
int     adv2nextspc(int idx);
void    texted_prndt(void);
int     texted_exec(void);
void    texted_add(void);
void    texted_list(void);
void    texted_goto(void);
void    texted_delete(void);
void    pause_sec64(uint16_t delay);
int     texted_isnull(const char *hdr);
void    texted_initbuf(void);
void    texted_membank(void);
void    texted_info(void);

/*
 * Return non-zero if the text record header pointed by hdr is a null header.
 */
int texted_isnull(const char *hdr)
{
    int i = 0;
    int ret = 1;

    if (hdr == NULL || strlen(hdr) < strlen(null_txt_hdr)) {
        ret = 0;
    } else {
        while(*hdr != 0 && null_txt_hdr[i] != 0) {
            if (*hdr != null_txt_hdr[i]) {
                ret = 0;
                break;
            }
            hdr++;
            i++;
        }
    }

    return ret;
}

/*
 * Pause specified # of 1/64-ths of a second.
 * NOTE: Flush the Rx buffer before calling this function.
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

/* print error message */
void texted_prnerror(int errnum)
{
    puts("ERROR: ");
    puts(ga_errmsg[errnum]);
    puts("\r\n");
}

/* get line of text from input */
void texted_getline(void)
{
    memset(prompt_buf,0,PROMPTBUF_SIZE);
    gets(prompt_buf);
    puts("\n\r");
}

/* parse the input buffer, set command code */
void texted_parse(void)
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
            puts("   ");  // 3 spaces
            puts(helptext[i++]);
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
int texted_exec(void)
{
    int ret = 1;

    switch (cmd_code)
    {
        case CMD_NULL:
            break;
        case CMD_UNKNOWN:
            texted_prnerror(ERROR_BADCMD);
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
        default:
            break;
    }

    return ret;
}

void texted_add(void)
{
    uint16_t nxtaddr;

    // Position current address at the NULL line after text.
    curr_addr = START_BRAM;
    line_num = 0;
    while (1) {
        if (texted_isnull((const char *)curr_addr)) {
            break; // found null header
        }
        line_num = PEEKW(curr_addr) + 1;
        nxtaddr = PEEKW(curr_addr + 3);
        curr_addr = nxtaddr;
    }
    while (1) {
        // create and store null text header at current address
        strcpy((char *)curr_addr, null_txt_hdr);
        texted_getline();
        if (0 == strncmp(eot_str, prompt_buf, strlen(eot_str))) {
            break;
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
            // text buffer full, terminate text here
            texted_prnerror(ERROR_FULLBUF);
            break;
        }
        // update header and text at current address
        POKEW(curr_addr, line_num);
        POKE(curr_addr + 2, CurrLine.len);
        POKEW(curr_addr + 3, CurrLine.next_ptr);
        strcpy((char *)(curr_addr + 5), CurrLine.text);
        // proceed to next address
        line_num++;
        curr_addr = CurrLine.next_ptr;
    }
}

void texted_list(void)
{
    uint16_t from_line = line_num;
    uint16_t to_line = line_num;
    uint16_t addr = START_BRAM;
    uint16_t line = 0;
    int      show_num = 0;
    int      n0, n;

    if (texted_isnull((const char *)START_BRAM)) {
        texted_prnerror(ERROR_NOTEXT);
        return;
    }
    if (strlen(prompt_buf) > 1) {
        n0 = adv2nxttoken(2);
        n = adv2nextspc(n0);
        if (strncmp(prompt_buf + n0, "all", 3)) {
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
                            texted_prnerror(ERROR_BADARG);
                            return;
                        }
                        n = adv2nxttoken(n0);
                        adv2nextspc(n);
                        show_num = (prompt_buf[n] == 'n');
                    }
                }
            }
        } else {
#ifdef DEBUG
            puts("DBG(texted_list): List all content.\n\r");
#endif
            from_line = 0;
            while (1) {
                if (texted_isnull((const char *)addr)) {
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
#ifdef DEBUG
                itoa(to_line, ibuf1, RADIX_DEC);
                itoa(addr, ibuf2, RADIX_HEX);
                puts ("DBG(texted_list): to_line = ");
                puts (ibuf1);
                puts (", addr = ");
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
    puts ("a\n\r");
    while (1) {
        if (texted_isnull((const char *)addr)) {
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
        puts ("addr = ");
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
    }
    puts("@EOT\n\r");
}

void texted_goto(void)
{
    int n;
    unsigned int line_to, line;
    uint16_t addr = START_BRAM;
    uint16_t nxtaddr;

    if (texted_isnull((const char *)START_BRAM)) {
        texted_prnerror(ERROR_NOTEXT);
        return;
    }

    n = adv2nxttoken(2);
    adv2nextspc(n);
    line_to = atoi(prompt_buf + n);

    while (1) {
        line = PEEKW(addr);
#ifdef DEBUG
        itoa(line, ibuf1, RADIX_DEC);
        itoa(addr, ibuf2, RADIX_HEX);
        puts ("DBG: line = ");
        puts (ibuf1);
        puts (", ");
        puts ("addr = ");
        puts (ibuf2);
        puts ("\n\r");
#endif
        nxtaddr = PEEKW(addr + 3);
        if (line == line_to
            ||
            texted_isnull((const char *)nxtaddr)) {

            line_num = line;
            curr_addr = addr;
#ifdef DEBUG
            itoa(line_num, ibuf1, RADIX_DEC);
            itoa(addr, ibuf2, RADIX_HEX);
            puts ("DBG: line_num = ");
            puts (ibuf1);
            puts (", ");
            puts ("addr = ");
            puts (ibuf2);
            puts ("\n\r");
#endif
            break;
        }
        addr = nxtaddr;
    }
}

void texted_delete()
{
    uint16_t addr = START_BRAM;
    uint16_t nxtaddr, lastaddr, tmpaddr, endaddr;
    unsigned int from_line, to_line, line, last_line;
    int n1, n2;

    if (texted_isnull((const char *)START_BRAM)) {
        texted_prnerror(ERROR_NOTEXT);
        return;
    }
    // current line is default
    from_line = line_num;
    to_line = line_num;
    // Find last line (pointing to null_txt_hdr)
    while (1) {
        nxtaddr = PEEKW(addr + 3);
        if (texted_isnull((const char *)nxtaddr)) {
            break;  // found last line
        }
        addr = nxtaddr;
    }
    last_line = PEEKW(addr);
    // last address, just after the null_txt_hdr
    lastaddr = nxtaddr + strlen(null_txt_hdr);
    // parse command line
    if (strlen(prompt_buf) > 1) {
        n1 = adv2nxttoken(2);
        n2 = adv2nextspc(n1);
        from_line = atoi(prompt_buf + n1);
        n1 = adv2nxttoken(n2);
        n2 = adv2nextspc(n1);
        if (n2 > n1) {
            to_line = atoi(prompt_buf + n1);
        } else {
            to_line = last_line;
        }
    }
    if (from_line > to_line) {
        texted_prnerror (ERROR_BADARG);
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
    // the actual delete procedure
    // 1. Go to the from_line position.
    addr = START_BRAM;
    while (1) {
        line = PEEKW(addr);
        nxtaddr = PEEKW(addr + 3);
        if (line == from_line) {
            break;  // found it
        }
        if (texted_isnull((const char *)nxtaddr)) {
            // something is wrong, from_line is not found
            texted_prnerror (ERROR_BADARG);
            return;
        }
        addr = nxtaddr;
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
        if (texted_isnull((const char *)nxtaddr)) {
            break; // end of text
        }
        addr = nxtaddr;
    }
#ifdef DEBUG
    itoa(tmpaddr, ibuf1, RADIX_HEX);
    itoa(endaddr, ibuf2, RADIX_HEX);
    itoa(lastaddr, ibuf3, RADIX_HEX);
    puts ("DBG(texted_delete): tmpaddr (begin) = ");
    puts (ibuf1);
    puts (", ");
    puts ("endaddr = ");
    puts (ibuf2);
    puts (", ");
    puts ("lastaddr = ");
    puts (ibuf3);
    puts ("\n\r");
#endif
    if (lastaddr <= endaddr) {
        // nothing to  delete
        texted_prnerror(ERROR_NTD);
        return;
    }
    // 3. Delete lines from_line .. to_line by moving text up in the buffer.
    memmove((void *)tmpaddr, (void *)endaddr,
            lastaddr - endaddr + strlen(null_txt_hdr));
    // renumerate lines and change pointer to next line in each
    addr = tmpaddr;
    lastaddr = lastaddr - (endaddr - tmpaddr);  // last address is now closer
    while (1) {
        if (texted_isnull((const char *)addr)) {
            // reached the null header, leave it in place and break
#ifdef DEBUG
            itoa(addr, ibuf1, RADIX_HEX);
            puts ("DBG(texted_delete): Renumbering, found null header at ");
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
            puts ("DBG(texted_delete): WARNING!!! Renumbering, found null header at ");
            puts (ibuf1);
            puts (".\n\r");
            puts ("DBG(texted_delete): Addresses may be out of alignment.\n\r");
            puts ("DBG(texted_delete): Check your algorithm!\n\r");
#endif
            break;
        }
        addr = nxtaddr;             // do the next line
    }
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
        texted_getline();
        texted_parse();
        cont = texted_exec();
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
    texted_getline();
    if (*prompt_buf == 'y' || *prompt_buf == 'Y') {
        strcpy((char *)START_BRAM, null_txt_hdr);
    }
}

/*
 * Show / set current memory bank.
 */
void texted_membank(void)
{
    __asm__("jsr %w", MOS_BRAMSEL);
}

/*
 * Show memory use info.
 */
void texted_info(void)
{
    uint16_t addr = START_BRAM;
    uint16_t line_count = 0;
    uint16_t last_line = 0;
    uint16_t lastaddr;

    puts("Current bank: ");
    bank_num = *RAMBANKNUM;
    puts(utoa((unsigned int)bank_num, ibuf1, RADIX_DEC));
    puts("\n\r");
    // Find last line (pointing to null_txt_hdr)
    while (1) {
        if (texted_isnull((const char *)addr)) {
            break;  // found last line
        }
        last_line = PEEKW(addr);
        addr = PEEKW(addr + 3);
        line_count++;
    }
    // last address, just after the null_txt_hdr
    lastaddr = addr + strlen(null_txt_hdr);
    puts("Last line#: ");
    puts(utoa((unsigned int)last_line, ibuf1, RADIX_DEC));
    puts("\n\r");
    puts("Lines in buffer: ");
    puts(utoa((unsigned int)line_count, ibuf1, RADIX_DEC));
    puts("\n\r");
    puts("Next free memory address (hex): ");
    puts(utoa((unsigned int)lastaddr, ibuf1, RADIX_HEX));
    puts("\n\r");
}

int main(void)
{
    line_num = 0;
    sel_begin = 0;
    sel_end = 0;
    bank_num = 0;
    curr_addr = START_BRAM;

    texted_banner();
    // Select memory bank 0
    __asm__("lda %v", bank_num);
    __asm__("jsr %w", MOS_BANKEDRAMSEL);
    texted_initbuf();
    texted_info();
    #ifdef DEBUG
        itoa(curr_addr, ibuf1, RADIX_HEX);
        puts ("DBG: curr_addr = ");
        puts (ibuf1);
        puts ("\n\r");
    #endif
    texted_shell();
    return 0;
}
