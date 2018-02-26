/*
 *
 * File:	enhmon.c
 * Purpose:	Code of enhanced memory monitor for MKHBCOS.
 * Author:	Marek Karcz
 * Created:	2/16/2018
 *
 * Revision history:
 *
 * 2/16/2012
 * 	Created.
 *  NOTE: This is UNTESTED CODE.
 *
 * 2/17/2018
 *  Added new functions, corrected problems.
 *  Refactored function enhmon_initmem().
 *
 * 2/18/2018
 *  Refactored constant strings and enhmon_rnv().
 *
 * 2/19/2018
 *  Formatting source code.
 *  Refactoring.
 *
 * 2/20/2018
 *  Refactoring / optimization (applying CC65 coding hints).
 *  Validation if proper decimal value added in dec2hexbin conversion.
 *
 * 2/25/2018
 *  Refactoring related to ROM library introduction.
 *
 *  ..........................................................................
 *
 *  BUGS:
 *
 *  1) Something I didn't notice when calling ROM library function from
 *     programs 'date' or 'clock' - when the ROM library function is called,
 *     program doesn't return, it goes to firmware monitor. This doesn't
 *     happen for all functions. Program experienced this problem after
 *     calling different functions between subsequent code changes and builds.
 *     E.g.: at 1-st it used to be after canonical memory dump. Now the dump
 *     doesn't kick back to mos> prompt, but show date / time does.
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

#define IBUF1_SIZE      80
#define IBUF2_SIZE      80
#define IBUF3_SIZE      32
#define PROMPTBUF_SIZE  80
#define RADIX_DEC       10
#define RADIX_HEX       16
#define RADIX_BIN       2

enum cmdcodes
{
    CMD_NULL = 0,
    CMD_HELP,
    CMD_EXIT,
    CMD_READMEM,
    CMD_WRITEMEM,
    CMD_INITMEM,
    CMD_WNV,
    CMD_RNV,
    CMD_EXECUTE,
    CMD_RAMBANK,
    CMD_DEC2HEXBIN,
    CMD_HEX2DECBIN,
    CMD_BIN2HEXDEC,
    CMD_MEMCPY,
    CMD_PRNDT,
    CMD_SETDT,
	//-------------
	CMD_UNKNOWN
};

enum eErrors {
    ERROR_OK = 0,
    ERROR_NORTC,
    ERROR_NOEXTBRAM,
    ERROR_BANKNUM,
    ERROR_DECVALEXP,
    ERROR_CHECKARGS,
    ERROR_BADCMD,
    ERROR_HEXVALEXP,
    ERROR_BINVALEXP,
    ERROR_UNKNOWN
};

const int ver_major = 1;
const int ver_minor = 5;
const int ver_build = 2;

const char hexcodes[] = {'0', '1', '2', '3', '4', '5', '6', '7', '8', '9',
                         'a', 'b', 'c', 'd', 'e', 'f', 0};

const char *ga_errmsg[11] =
{
    "OK.",
    "No RTC chip detected.",
    "No BRAM detected.",
    "Bank# expected value: 0..7.",
    "Expected decimal argument.",
    "Check command arguments.",
    "Invalid command.",
    "Expected hexadecimal argument.",
    "Expected binary argument.",
    "Unknown."
};

const char *helptext[31] =
{
    "\n\r",
    "   r : read memory contents.\n\r",
    "       r <hexaddr>[-hexaddr] [+]\n\r",
    "         + - canonical (dump hex + ASCII)\n\r",
    "   w : write data to address.\n\r",
    "       w <hexaddr> <hexdata> [hexdata] ...\n\r",
    "   i : initialize memory with value.\n\r",
    "       i <hexaddr>-<hexaddr> hexdata\n\r",
    "   x : execute at address.\n\r",
    "       x <hexaddr>\n\r",
    "   m : copy memory of specified size.\n\r",
    "       m <hex_dst> <hex_src> <hex_size>\n\r",
    "   t : print date / time.\n\r",
    "   s : set date / time.\n\r",
    " wnv : write to non-volatile RTC RAM.\n\r",
    "       wnv <bank#> <hexaddr>\n\r",
    " rnv : dump non-volatile RTC RAM.\n\r",
    "       rnv <bank#> [<hexaddr>]\n\r",
    "   b : see or select BRAM bank#.\n\r",
    "       b [00..07]\n\r",
    "d2hb : decimal to hex/bin conversion.\n\r",
    "       d2hb <decnum>\n\r",
    "h2db : hexadecimal to dec/bin conversion.\n\r",
    "       h2db <hexval>\n\r",
    "b2hd : binary to hex/dec conversion.\n\r",
    "       b2hd <binval>\n\r",
    "exit : exit enhanced shell.\n\r",
    "help : print this message.\n\r",
    "\n\r",
    "@EOH"
};

int cmd_code = CMD_NULL;
char prompt_buf[PROMPTBUF_SIZE];
char ibuf1[IBUF1_SIZE], ibuf2[IBUF2_SIZE], ibuf3[IBUF3_SIZE];

void    enhmon_getline(void);
void    enhmon_parse(void);
void    enhmon_help(void);
void    enhmon_readmem(void);
void    enhmon_writemem(void);
void    enhmon_execmem(void);
void    enhmon_version(void);
int     hexchar2int(char c);
int     power(int base, int exp);
int     hex2int(const char *hexstr);
void    enhmon_initmem(void);
void    enhmon_mcp(void);
void    enhmon_rnv(void);
void    enhmon_wnv(void);
int     enhmon_exec(void);
void    enhmon_shell(void);
void    enhmon_banner(void);
//void    enhmon_rambanksel(void);
//void    enhmon_getrambank(void);
void    enhmon_bramsel(void);
void    enhmon_dec2hexbin(void);
void    enhmon_hex2decbin(void);
void    enhmon_bin2hexdec(void);
void    enhmon_prnerror(int errnum);
int     adv2nxttoken(int idx);
int     adv2nextspc(int idx);
void    prn_decbinhex(char *dec, char *bin, char *hex);
void    enhmon_prndt(void);
void    enhmon_setdt(void);

/* print error message */
void enhmon_prnerror(int errnum)
{
    puts("ERROR: ");
    puts(ga_errmsg[errnum]);
    puts("\r\n");
}

/* get line of text from input */
void enhmon_getline(void)
{
    memset(prompt_buf,0,PROMPTBUF_SIZE);
    gets(prompt_buf);
    puts("\n\r");
}

/* parse the input buffer, set command code */
void enhmon_parse(void)
{
    if (0 == strlen(prompt_buf))
    {
        cmd_code = CMD_NULL;
    }
    else if (0 == strncmp(prompt_buf,"exit",4))
    {
        cmd_code = CMD_EXIT;
    }
    else if (0 == strncmp(prompt_buf,"r ",2))
    {
        cmd_code = CMD_READMEM;
    }
    else if (0 == strncmp(prompt_buf, "m ", 2))
    {
        cmd_code = CMD_MEMCPY;
    }
    else if (0 == strncmp(prompt_buf,"w ",2))
    {
        cmd_code = CMD_WRITEMEM;
    }
    else if (0 == strncmp(prompt_buf,"i ",2))
    {
        cmd_code = CMD_INITMEM;
    }
    else if (0 == strncmp(prompt_buf,"x ",2))
    {
        cmd_code = CMD_EXECUTE;
    }
    else if (0 == strncmp(prompt_buf,"help",4))
    {
        cmd_code = CMD_HELP;
    }
    else if (0 == strncmp(prompt_buf,"wnv ",4))
    {
        cmd_code = CMD_WNV;
    }
    else if (0 == strncmp(prompt_buf,"rnv ",4))
    {
        cmd_code = CMD_RNV;
    }
/*****
    else if (0 == strncmp(prompt_buf,"bank",4))
    {
        cmd_code = CMD_RAMBANK;
    }
*****/
    else if (0 == strncmp(prompt_buf,"b ",2)
             || prompt_buf[0] == 'b'
            )
    {
        cmd_code = CMD_RAMBANK;
    }
    else if (0 == strncmp(prompt_buf,"d2hb ",5))
    {
        cmd_code = CMD_DEC2HEXBIN;
    }
    else if (0 == strncmp(prompt_buf,"h2db ",5))
    {
        cmd_code = CMD_HEX2DECBIN;
    }
    else if (0 == strncmp(prompt_buf,"b2hd ",5))
    {
        cmd_code = CMD_BIN2HEXDEC;
    }
    else if (prompt_buf[0] == 't')
    {
        cmd_code = CMD_PRNDT;
    }
    else if (prompt_buf[0] == 's')
    {
        cmd_code = CMD_SETDT;
    }
    else
        cmd_code = CMD_UNKNOWN;
}

/* print help */
void enhmon_help(void)
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

void enhmon_readmem(void)
{
    __asm__("jsr %w", MOS_PROCRMEM);
}

void enhmon_writemem(void)
{
    __asm__("jsr %w", MOS_PROCWMEM);
}

void enhmon_execmem(void)
{
    __asm__("jsr %w", MOS_PROCEXEC);
}

void enhmon_initmem(void)
{
    __asm__("jsr %w", MOS_MEMINIT);
}

void enhmon_mcp(void)
{
    __asm__("jsr %w", MOS_MEMCPY);
}

void enhmon_bramsel(void)
{
    __asm__("jsr %w", MOS_BRAMSEL);
}

void enhmon_prndt(void)
{
    __asm__("jsr %w", MOS_PRNDT);
}

void enhmon_setdt(void)
{
    __asm__("jsr %w", MOS_SETDT);
}

void enhmon_version(void)
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
 *
 * Convert single hexadecimal ascii representation
 * to an integer number.
 *
 */
int hexchar2int(char c)
{
    int i = 0;

    if (c >= '0' && c <= '9')
        i = c - '0';
    else if (c >= 'A' && c <= 'F')
        i = c - 'A' + 10;
    else if (c >= 'a' && c <= 'f')
        i = c - 'a' + 10;

    return i;
}

/*
 *
 * Calculate integer power.
 *
 */
int power(int base, int exp)
{
    int i = 0;
    int ret = base;

    if (exp == 0)
    	ret = 1;
    else if (exp == 1)
    	ret = base;
    else
    {
        for (i=1; i<exp; i++)
        {
            ret = ret * base;
        }
    }

    return ret;
}

/*
 *
 * Convert ascii representation of hexadecimal
 * number to integer.
 *
 */
int hex2int(const char *hexstr)
{
    int ret = 0;
    int i = 0;
    char c = '0';
    int n = 0;
    int l = 0;

    l = strlen(hexstr);

    for (i=l-1; i>=0; i--)
    {
        c = hexstr[i];
        n = hexchar2int(c);
        ret = ret + n*power(16,l-i-1);
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

/*
 * Handle command: rnv Bank#
 * Dump contents of non-volatile RTC RAM from specified Bank# (0 or 1).
 * Bank 0 : 114 bytes are dumped ($0E - $7F) from RTC NV RAM.
 * Bank 1 : 128 bytes are dumped ($00 - $7F) from RTC NV RAM.
 * If optional hexaddr argument is provided, the NV memory will be
 * dumped in RAM at specified address.
 */
void enhmon_rnv(void)
{
    int i, len;
    unsigned char b     = 0x00;
    unsigned char bank  = 0;
    unsigned char offs  = 0;
    unsigned char offs2 = 0;
    uint16_t addr       = 0x000e;
    uint16_t ramaddr    = 0x0000;
    int optarg = 0; // optional argument provided 0 - no / 1 - yes

    if (!RTCDETECTED) {
        enhmon_prnerror(ERROR_NORTC);
        return;
    }

    len = strlen(prompt_buf);   // length of prompt before cut off at 1=st arg.
    offs = adv2nxttoken(3);     // find beginning of 1-st arg. (bank #)
    offs2 = adv2nextspc(offs);  // find the end of 1-st arg.
    bank = atoi(prompt_buf + offs);
    if (bank) addr=0;
    /*
     * optional argument, hexaddr
     * if full len of prompt greater than prompt cut at end of 1-st arg.
     */
    if (len > strlen(prompt_buf)) {
        optarg = 1;
        // find the start index of 2-nd argument (hexaddr)
        offs2 = adv2nxttoken(offs2);
        adv2nextspc(offs2);
        ramaddr = hex2int(prompt_buf+offs2);
        puts("RAM copy address: ");
        puts(itoa(ramaddr, ibuf3, RADIX_HEX));
        puts("\n\r");
    }
    puts("RTC NV Bank #: ");
    puts(itoa(bank, ibuf3, RADIX_DEC));
    puts("\n\r");
    for ( ; addr<0x0080; addr+=16)
    {
        itoa(addr,ibuf1,RADIX_HEX);
        if (strlen(ibuf1) < 2)
        {
            for (i=2-strlen(ibuf1); i>0; i--) {
                putchar('0');
            }
        }
        puts(ibuf1);
        puts(" : ");
        for (offs=0; offs<16 && (addr+offs)<0x0080; offs++)
        {
            b = ds1685_readram(bank, (addr+offs)&0x00ff);
            if (optarg) {
                POKE(ramaddr++, b);
            }
            if (b < 16) putchar('0');
            puts(itoa(b,ibuf3,RADIX_HEX));
            putchar(' ');
        }
        puts(" : ");
        for (offs=0; offs<16 && (addr+offs)<0x0080; offs++)
        {
            b = ds1685_readram(bank, (addr+offs)&0x00ff);
            if (b > 31 && b < 127)
                putchar(b);
            else
                putchar('?');
        }
        puts("\n\r");
        if ((addr+16) > 0x007f) break;
    }
}

/*
 * Handle command: wnv Bank# <hexaddr>
 * Write contents of RAM at <hexaddr> to non-volatile RTC RAM
 * at specified Bank# (0 or 1).
 * If Bank=0, 114 bytes are written starting at <hexaddr> to NV RAM.
 * If Bank=1, 128 bytes are written starting at <hexaddr> to NV RAM.
 *
 */
void enhmon_wnv(void)
{
    int i,n;
    unsigned char b = 0x00;
    unsigned char bank = 0;
    uint16_t addr = 0x0000;
    uint16_t nvaddr = 0x000e;

    if (!RTCDETECTED) {
        enhmon_prnerror(ERROR_NORTC);
        return;
    }

    i=3;
    /* find index of bank value start */
    i = adv2nxttoken(3);
    n = i;
    /* find index of end of bank value */
    i = adv2nextspc(i);
    /* find index of hexaddr value start */
    i = adv2nxttoken(i);
    addr = hex2int(prompt_buf+i);
    bank = atoi(prompt_buf+n);
    puts("RAM address: ");
    puts(prompt_buf+i);
    puts("\n\r");
    puts("RTC NV Bank #: ");
    puts(prompt_buf+n);
    puts("\n\r");
    if (bank) nvaddr = 0;
    for ( ; nvaddr<0x0080; addr++, nvaddr++)
    {
        b = PEEK(addr);
        ds1685_storeram(bank, nvaddr&0x00ff, b);
    }
}

/*
 * Execute command.
 */
int enhmon_exec(void)
{
    int ret = 1;

    switch (cmd_code)
    {
        case CMD_NULL:
            break;
        case CMD_UNKNOWN:
            enhmon_prnerror(ERROR_BADCMD);
            break;
        case CMD_EXIT:
            puts("Bye!\n\r");
            ret = 0;
            break;
        case CMD_READMEM:
            enhmon_readmem();
            break;
        case CMD_MEMCPY:
            enhmon_mcp();
            break;
        case CMD_WRITEMEM:
            enhmon_writemem();
            break;
        case CMD_INITMEM:
            enhmon_initmem();
            break;
        case CMD_EXECUTE:
            enhmon_execmem();
            break;
        case CMD_HELP:
            enhmon_help();
            break;
        case CMD_RNV:
            enhmon_rnv();
            break;
        case CMD_WNV:
            enhmon_wnv();
            break;
        case CMD_RAMBANK:
            //enhmon_rambanksel();
            enhmon_bramsel();
            break;
        case CMD_DEC2HEXBIN:
            enhmon_dec2hexbin();
            break;
        case CMD_HEX2DECBIN:
            enhmon_hex2decbin();
            break;
        case CMD_BIN2HEXDEC:
            enhmon_bin2hexdec();
            break;
        case CMD_PRNDT:
            enhmon_prndt();
            break;
        case CMD_SETDT:
            enhmon_setdt();
            break;
        default:
            break;
    }

    return ret;
}

/*
 * Initialization and command shell loop.
 */
void enhmon_shell(void)
{
    int cont = 1;

    while(cont)
    {
        puts("enhmon>");
        enhmon_getline();
        enhmon_parse();
        cont = enhmon_exec();
    }
}

void enhmon_banner(void)
{
    while (kbhit()) getc(); // flush RX buffer
    memset(prompt_buf,0,PROMPTBUF_SIZE);
    puts("\n\r");
    puts("Welcome to Enhanced Monitor ");
    enhmon_version();
    puts("(C) Marek Karcz 2012-2018. All rights reserved.\n\r");
    puts("Type 'help' for guide.\n\r\n\r");
}

//int g_bnum;
/*
 * Select banked RAM bank.
 */
/********************
void enhmon_rambanksel(void)
{
    g_bnum = -1;

    if (!EXTRAMDETECTED || !EXTRAMBANKED) {
        enhmon_prnerror(ERROR_NOEXTBRAM);
        return;
    }

    if (strlen(prompt_buf) > 5) {

        g_bnum = atoi(prompt_buf+5);

        if (g_bnum >= 0 && g_bnum < 8) {
            __asm__("lda %v", g_bnum);
            __asm__("jsr %w", MOS_BANKEDRAMSEL);
        } else {
            enhmon_prnerror(ERROR_BANKNUM);
        }
    }

    enhmon_getrambank();
}
****************/

/*
 * Get current banked RAM bank #.
 */
/************************
void enhmon_getrambank(void)
{
    unsigned char rambank = 0;

    if (!EXTRAMDETECTED || !EXTRAMBANKED) {
        enhmon_prnerror(ERROR_NOEXTBRAM);
        return;
    }

    rambank = *RAMBANKNUM;

    puts("Current RAM bank#: ");
    puts(utoa((unsigned int)rambank, ibuf3, RADIX_DEC));
    puts("\n\r");
}
****************/

/*
 * Print 3 strings to output in separate lines with labels
 * Dec:, Hex: and Bin:.
 */
void prn_decbinhex(char *dec, char *bin, char *hex)
{
    puts("Dec: ");
    puts(dec);
    puts("\n\r");
    puts("Hex: ");
    puts(hex);
    puts("\n\r");
    puts("Bin: ");
    puts(bin);
    puts("\n\r");
}

/*
 * Decimal to hex/bin conversion.
 */
void enhmon_dec2hexbin()
{
    int i, j;
    char *hexval = ibuf1;
    char *binval = ibuf2;

    if (strlen(prompt_buf) > 5) {
        i = adv2nxttoken(5);
        adv2nextspc(i);
        j = i;
        // validate if proper decimal value
        while (prompt_buf[j] != 0) {
            if (prompt_buf[j] < 48 || prompt_buf[j] > 57) {
                enhmon_prnerror(ERROR_DECVALEXP);
                return;
            }
            j++;
        }
        ultoa((unsigned long)atol(prompt_buf + i), hexval, RADIX_HEX);
        ultoa((unsigned long)atol(prompt_buf + i), binval, RADIX_BIN);
        prn_decbinhex(prompt_buf + i, binval, hexval);
    } else {
        enhmon_prnerror(ERROR_DECVALEXP);
    }
}

/*
 * Hexadecimal to decimal and binary conversion.
 */
void enhmon_hex2decbin(void)
{
      int i, j;
      char *decval = ibuf1;
      char *binval = ibuf2;
      int dv = 0;

      if (strlen(prompt_buf) > 5) {
          i = adv2nxttoken(5);
          adv2nextspc(i);
          j = i;
          // validate if proper hexadecimal value
          while (prompt_buf[j] != 0) {
              if (prompt_buf[j] < 48
                  || (prompt_buf[j] > 57 && prompt_buf[j] < 65)
                  || (prompt_buf[j] > 70 && prompt_buf[j] < 97)
                  || prompt_buf[j] > 102
                 ) {
                  enhmon_prnerror(ERROR_HEXVALEXP);
                  return;
              }
              j++;
          }
          dv = hex2int(prompt_buf + i);
          utoa((unsigned int)dv, decval, RADIX_DEC);
          utoa((unsigned int)dv, binval, RADIX_BIN);
          prn_decbinhex(decval, binval, prompt_buf + i);
      } else {
          enhmon_prnerror(ERROR_HEXVALEXP);
      }
}

/*
 * Binary to hexadecimal and decimal conversion.
 */
void enhmon_bin2hexdec(void)
{
    int i, j, k;
    char *hexval = ibuf1;
    char *decval = ibuf2;
    int pos = 0;
    int dv = 0;

    if (strlen(prompt_buf) > 5) {
        i = adv2nxttoken(5);
        adv2nextspc(i);
        pos = strlen(prompt_buf + i) - 1;
        j = i;
        // convert binary to decimal
        while (prompt_buf[j] != 0) {
            // validate if proper binary digit
            if (prompt_buf[j] != '1'  && prompt_buf[j] != '0') {
                enhmon_prnerror(ERROR_BINVALEXP);
                return;
            }
            k = ((prompt_buf[j] == '1') ? 1 : 0);
            dv += k * power(2, pos);
            pos--;
            j++;
        }
        utoa((unsigned int)dv, hexval, RADIX_HEX);
        utoa((unsigned int)dv, decval, RADIX_DEC);
        prn_decbinhex(decval, prompt_buf + i, hexval);
    } else {
        enhmon_prnerror(ERROR_BINVALEXP);
    }
}

int main(void)
{
    enhmon_banner();
    enhmon_shell();
    return 0;
}
