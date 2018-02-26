/*
 *
 * File:	enhshell.c
 * Purpose:	Code of shell with additional commands for MKHBCOS.
 *          This file is a part of MKHBCOS operating system programming API
 *          for MKHBC-8-Rx computer.
 * Author:	Marek Karcz
 * Created:	1/22/2012
 *
 * NOTE:
 *  GVIM: set tabstop=2 shiftwidth=2 expandtab
 *  2/11/2018 - I switched to ATOM editor using language-65asm 5.0.0 package.
 *              Soft TABS were setup with length=4. Therefore above settings
 *              provided for GVIM may not work well anymore.
 *
 * To Do:
 *
 * 	1) Read memory (hex + ASCII). - DONE
 * 	2) LCD clear screen command (lcdcls). - DONE
 * 	3) Redirection to MKHBCOS commands. - DONE
 * 	4) IRQ vector change command.
 * 	5) I/O vectors change commands.
 * 	6) Help. - IN PROGRESS.
 * 	7) Disassembler.
 * 	8) Online assembler.
 * 	9) The dec<->hex<->bin conversion tool. - DONE
 * 	10) Simple calculator.
 * 	11) Simple text editor.
 * 	12) Rudimentary batch scripting capability.
 *
 * Revision history:
 *
 * 1/22/2012
 * 	Created.
 *
 * 1/23/2012
 *  Added redirection to MOS execute.
 *
 * 1/24/2012
 *  Added command 'cls'.
 *
 * 1/28/2012
 *  Added enhanced read memory (rdmem).
 *
 * 1/31/2012
 * 	Changes to hexchar2int function.
 *
 * 2/5/2012
 * 	DS1685 (RTC - Real Time Clock) support.
 *
 * 2/9/2012
 *  Refactoring, bug fixes.
 *
 * 2/12/2012
 *  Cosmetic.
 *
 * 11/29/2015
 *  Added conditional code for LCD display.
 *
 * 2/3/2016
 *  Added sleep and rclk commands.
 *
 * 2/6/2016
 *  Blocked memory dump in I/O range.
 *
 * 2/21/2016
 *  enhsh_rclk: set cursor HOME instead of cls.
 *
 * 6/16/2017
 *  Functions to access RTC NV RAM.
 *  Prototypes of functions added.
 *
 * 2/1/2018
 *  Initialization of RTC commented out in the shell initialization routine
 *  because this code is now in EPROM, RTC is initialized when system boots
 *  or resets. Command 'rtci' to manually initialize RTC is still available
 *  in case RTC was messed up by software.
 *  Added command 'sctr' to read/print value of periodically updated counter.
 *
 * 2/3/2018
 *  Periodically updated counter type changed from unsigned short to unsigned
 *  long. This counter will roll over in more than 2 years.
 *  Banked RAM selection - command 'bank'.
 *  Decimal to hex/bin conversion.
 *  Copying RTC NV memory to RAM added. (optional argument to 'rnv' command.)
 *  Added optional arguments to 'sctr' command (timer monitoring in a loop)
 *
 * 2/5/2018
 *  Added KBHIT calls to rclk and sctr loops.
 *  Added kbhit() function.
 *  Function rclk now uses function kbhit(), while sctr uses macro KBHIT
 *  (for testing purposes, will switch back to macro as it is for sure
 *   faster and uses less memory than function).
 *  Also added RX buffer flush using kbhit() at the initialization part of the
 *  program in enhsh_banner() function.
 *
 *  Later I added kbhit() implementation in the library (mkhbcos_serial.s) and
 *  commented impl. in this source code. Used it in all functions for testing
 *  purposes. I will switch to macro impl. in future version, although the
 *  assembly code impl. in mkhbcos_serial.s should be pretty efficient being
 *  a __fastcall__ type function.
 *  Added ability to interrupt from keyboard (KBHIT) also to pause_sec() and
 *  lcd clock and memory dump functions.
 *  Now user can press SPACE during long memory dump to pause listing.
 *
 * 2/10/2018
 *  Added checking of the flags of devices presence before using RTC
 *  or RAM banking functions.
 *
 * 2/18/2018
 *  Refactoring, bug fixes and optimization.
 *  NOTE: It looks like calls to kbhit() function take less memory than
 *        use of macro KBHIT.
 *
 * 2/20/2018
 *  Reformatting code. Refactoring. Trimming.
 *  Function pause_sec() refactored to use timer variable updated in IRQ
 *  rather than polling RTC for time.
 *  Unfortunately the code produced by the compiler is longer after this
 *  even though the C-code is more compact after refactoring.
 *  NOTE: After another thought I decided to get rid of functions
 *        pause_sec() and enhsh_delay() as they are not really needed anymore
 *        since I dropped the screen clock and sleep commands from the
 *        program. So - gone.
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
 *     program doesn't return, it goes to firmware monitor. This is similar to
 *     the problem in enhmon.c program. I wonder if this is problem of big
 *     programs? This does not happen for all ROM library functions.
 *
 *  ..........................................................................
 */

#include <stdlib.h>
#include <stdint.h>
#include <string.h>
#include <peekpoke.h>
#include "mkhbcos_lcd.h"
#include "mkhbcos_serialio.h"
#include "mkhbcos_ansi.h"
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

//#define LCD_EN 1

enum cmdcodes
{
    CMD_NULL = 0,
    CMD_HELP,
    CMD_EXIT,
    CMD_CLS,
#if defined(LCD_EN)
    CMD_LCDINIT,
    CMD_LCDPRINT,
    CMD_LCDCLEAR,
    CMD_CLOCK,		  // LCD clock
#endif
    CMD_READMEM,
    CMD_WRITEMEM,
    CMD_INITMEM,	  // initialize memory with value
    CMD_DATE,		    // read DS1685 RTC data
    CMD_SETDTTM,	  // set DS1685 date and time
    CMD_WNV,
    CMD_RNV,
    CMD_EXECUTE,
    CMD_SHOWTMCT,   // show periodically updated (in interrupt) counter
    CMD_RAMBANK,    // select or get banked RAM bank#
    CMD_CONV,       // dec/hex/bin conversion
    CMD_MEMCPY,
    //-------------
    CMD_UNKNOWN
};

const int ver_major = 3;
const int ver_minor = 2;
const int ver_build = 1;

#if defined(LCD_EN)
const int lcdlinesel[] = {LCD_LINE_CURRENT, LCD_LINE_1, LCD_LINE_2};
#endif

const char hexcodes[] = {'0', '1', '2', '3', '4', '5', '6', '7', '8', '9',
                         'a', 'b', 'c', 'd', 'e', 'f', 0};

enum eErrors {
    ERROR_OK = 0,
    ERROR_NORTC,
    ERROR_NOEXTBRAM,
    ERROR_BANKNUM,
    ERROR_CHECKARGS,
    ERROR_BADCMD,
    ERROR_UNKNOWN
};

const char *ga_errmsg[7] =
{
    "OK.",
    "No RTC chip detected.",
    "No BRAM detected.",
    "Bank# expected value: 0..7.",
    "Check command arguments.",
    "Invalid command.",
    "Unknown."
};

#if defined(LCD_EN)
const char *helptext[33] =
#else
const char *helptext[29] =
#endif
{
    "\n\r",
    "  cls : clear console.\n\r",
#if defined(LCD_EN)
    " lcdi : initialize LCD.\n\r",
    " lcdc : clear LCD.\n\r",
    " lcdp : print to LCD.\n\r",
#endif
    "    r : read memory contents.\n\r",
    "        r <hexaddr>[-hexaddr] [+]\n\r",
    "        + - canonical (hex + ASCII)\n\r",
    "    w : write data to address.\n\r",
    "        w <hexaddr> <hexdata> [hexdata] ...\n\r",
    "    i : initialize memory with value.\n\r",
    "        i <hexaddr> <hexaddr> [hexdata]\n\r",
    "    x : execute at address.\n\r",
    "        x <hexaddr>\n\r",
    "    m : copy memory of specified size.\n\r",
    "        mcp <hex_dst> <hex_src> <hex_size>\n\r",
    "    t : display date/time.\n\r",
    "    s : set date/time.\n\r",
#if defined(LCD_EN)
    "clock : run LCD clock (reset to exit).\n\r",
#endif
    "  wnv : write to non-volatile RTC RAM.\n\r",
    "        wnv <bank#> <hexaddr>\n\r",
    "  rnv : dump non-volatile RTC RAM.\n\r",
    "        rnv <bank#> [<hexaddr>]\n\r",
    "  ctr : show 64 Hz counter value.\n\r",
    "    b : see or select banked RAM bank.\n\r",
    "        b [00..07]\n\r",
    " conv : dec/hex/bin conversion.\n\r",
    "        conv <type> <arg>\n\r",
    "        type: d2hb, h2db, b2hd\n\r",
    " exit : exit enhanced shell.\n\r",
    "\n\r",
    "@EOH"
};

int     cmd_code = CMD_NULL;
char    prompt_buf[PROMPTBUF_SIZE];
char    ibuf1[IBUF1_SIZE], ibuf2[IBUF2_SIZE], ibuf3[IBUF3_SIZE];

struct  ds1685_clkdata	clkdata;

//void    enhsh_pause(uint16_t delay);
//void    pause_sec(uint16_t delay);
void    enhsh_getline(void);
void    enhsh_parse(void);
void    enhsh_help(void);
void    enhsh_readmem(void);
void    enhsh_writemem(void);
void    enhsh_execmem(void);
void    enhsh_version(void);
void    enhsh_cls(void);
#if defined(LCD_EN)
void    enhsh_lcdp(void);
void    enhsh_lcdinit(void);
void    enhsh_clock(void);
#endif
int     hexchar2int(char c);
int     power(int base, int exp);
int     hex2int(const char *hexstr);
void    enhsh_initmem(void);
void    enhsh_rnv(void);
void    enhsh_wnv(void);
int     enhsh_exec(void);
void    enh_shell(void);
void    enhsh_banner(void);
void    enhsh_showtmct(void);
//void    enhsh_rambanksel(void);
//void    enhsh_getrambank(void);
void    enhsh_conv(void);
void    enhsh_mcp(void);
void    enhsh_prnerror(int errnum);
int     adv2nxttoken(int idx);
int     adv2nextspc(int idx);
void    enhsh_bramsel(void);
void    enhsh_prndt(void);
void    enhsh_setdt(void);

/*
 * Advance index to next token.
 */
int adv2nxttoken(int idx)
{
    while (*(prompt_buf + idx) == 32) {
        idx++;
    }

    return idx;
}

/*
 * Advance index to next space.
 */
int adv2nextspc(int idx)
{
    while (*(prompt_buf + idx) != 0) {

        if (*(prompt_buf + idx) == 32
            || *(prompt_buf + idx) == '-') {

            *(prompt_buf + idx) = 0;
            idx++;
            break;
        }
        idx++;
	 }

   return idx;
}

/* print error message */
void enhsh_prnerror(int errnum)
{
    puts("ERROR: ");
    puts(ga_errmsg[errnum]);
    puts("\r\n");
}

/* pause */
/************
void enhsh_pause(uint16_t delay)
{
    int i = 0;

    for(; i < delay; i++);
}
***********/

/*
 * Pause specified # of seconds.
 * NOTE: Flush the Rx buffer before calling this function.
 */
//void pause_sec(uint16_t delay)
/************************
{
    // current timer + number of seconds * 64 will be
    // the number we wait for
    unsigned long t64hz = *TIMER64HZ + delay * 64;

    if (RTCDETECTED) {
        while (*TIMER64HZ < t64hz) {
            enhsh_pause(200);   // wait some cycles
            if (kbhit()) break; // possible to interrupt from keyboard
        }
    }
    *******************/
    // Code above actually compiles to bigger image than the code below.
    /***************
    unsigned int currsec = 0, nsec = delay;

    if (RTCDETECTED) {
        ds1685_rdclock (&clkdata);
        while (nsec > 0) {

            currsec = clkdata.seconds;
            // wait 1 second
            while (currsec == clkdata.seconds) {

                enhsh_pause(200);
                ds1685_rdclock (&clkdata);
            }
            nsec--;
            if (kbhit()) break; // must be able to interrupt from keyboard
                                // remember to flush RX buffer vefore calling
                                // this function
        }
    }
    ******************/
//}

/* get line of text from input */
void enhsh_getline(void)
{
    memset(prompt_buf, 0, PROMPTBUF_SIZE);
    gets(prompt_buf);
    puts("\n\r");
}

/* parse the input buffer, set command code */
void enhsh_parse(void)
{
    if (0 == strlen(prompt_buf))
    {
        cmd_code = CMD_NULL;
    }
    else if (0 == strncmp(prompt_buf,"exit",4))
    {
        cmd_code = CMD_EXIT;
    }
    else if (0 == strncmp(prompt_buf,"cls",3))
    {
        cmd_code = CMD_CLS;
    }
#if defined(LCD_EN)
    else if (0 == strncmp(prompt_buf,"lcdi",4))
    {
        cmd_code = CMD_LCDINIT;
    }
    else if (0 == strncmp(prompt_buf,"lcdp",4))
    {
        cmd_code = CMD_LCDPRINT;
    }
    else if (0 == strncmp(prompt_buf,"lcdc",4))
    {
        cmd_code = CMD_LCDCLEAR;
    }
#endif
    else if (0 == strncmp(prompt_buf,"r ",2))
    {
        cmd_code = CMD_READMEM;
    }
    else if (0 == strncmp(prompt_buf,"w ",2))
    {
        cmd_code = CMD_WRITEMEM;
    }
    else if (0 == strncmp(prompt_buf,"i ",2))
    {
        cmd_code = CMD_INITMEM;
    }
    else if (prompt_buf[0] == 't')
    {
        cmd_code = CMD_DATE;
    }
    else if (prompt_buf[0] == 's')
    {
        cmd_code = CMD_SETDTTM;
    }
#if defined(LCD_EN)
    else if (0 == strncmp(prompt_buf,"clock",5))
    {
        cmd_code = CMD_CLOCK;
    }
#endif
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
    else if (0 == strncmp(prompt_buf, "ctr", 3))
    {
        cmd_code = CMD_SHOWTMCT;
    }
    else if (prompt_buf[0] == 'b')
    {
        cmd_code = CMD_RAMBANK;
    }
    else if (0 == strncmp(prompt_buf,"conv",4))
    {
        cmd_code = CMD_CONV;
    }
    else if (0 == strncmp(prompt_buf, "m ", 2))
    {
        cmd_code = CMD_MEMCPY;
    }
    else
        cmd_code = CMD_UNKNOWN;
}

/* get text from console and print on LCD */
#if defined(LCD_EN)
void enhsh_lcdp(void)
{
    int l;
    char c;

    puts("Line (0 - current, 1 - line #1, 2 - line #2): ");
    c = getchar();
    ibuf1[0] = c; ibuf1[1] = 0;
    puts("\n\r");
    puts("Text: "); gets(ibuf2);
    puts("\n\r");
    l = atoi(ibuf1);
    if (l >= 0 && l < 3 && 0 < strlen(ibuf2))
        lcd_puts(ibuf2, lcdlinesel[l]);
}
#endif

/* print help */
void enhsh_help(void)
{
    unsigned char cont = 1, i = 0;

    while(cont)
    {
        if (0 == strncmp(helptext[i],"@EOH",4))
            cont = 0;
        else {
            puts("  "); // 2 spaces
            puts(helptext[i++]);
        }
    }
}

void enhsh_readmem(void)
{
    __asm__("jsr %w", MOS_PROCRMEM);
}

void enhsh_writemem(void)
{
    __asm__("jsr %w", MOS_PROCWMEM);
}

void enhsh_execmem(void)
{
    __asm__("jsr %w", MOS_PROCEXEC);
}

void enhsh_initmem(void)
{
    __asm__("jsr %w", MOS_MEMINIT);
}

void enhsh_mcp(void)
{
    __asm__("jsr %w", MOS_MEMCPY);
}

void enhsh_bramsel(void)
{
    __asm__("jsr %w", MOS_BRAMSEL);
}

void enhsh_prndt(void)
{
    __asm__("jsr %w", MOS_PRNDT);
}

void enhsh_setdt(void)
{
    __asm__("jsr %w", MOS_SETDT);
}

void enhsh_version(void)
{
    itoa(ver_major, ibuf1, RADIX_DEC);
    strcat(ibuf1, ".");
    strcat(ibuf1, itoa(ver_minor, ibuf3, RADIX_DEC));
    strcat(ibuf1, ".");
    strcat(ibuf1, itoa(ver_build, ibuf3, RADIX_DEC));
    strcat(ibuf1, "\n\r");
    puts(ibuf1);
}

void enhsh_cls(void)
{
    ansi_cls();
    puts("\r");
}

#if defined(LCD_EN)
void enhsh_lcdinit(void)
{
    char ecans = 'n';
    char ebans = 'n';

    LCD_INIT;
    puts("Enable cursor (y/n)?");
    ecans = getchar();
    puts("\n\r");
    puts("Enable blinking (y/n)?");
    ebans = getchar();
    puts("\n\r");
    lcd_cursorctrl(((ecans=='y')?1:0), ((ebans=='y')?1:0));
}
#endif

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
    int i = 1;
    int ret = base;

    if (exp == 0)
        ret = 1;
    else if (exp == 1)
        ret = base;
    else
    {
        for (; i<exp; i++)
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

//int g_KbHit;
/*
 * Check if character waits in buffer.
 * Return it if so, otherwise return 0.
 */
/*****************
int kbhit(void)
{
  int ret = 0;

  __asm__("jsr %w", MOS_KBHIT);
  __asm__("sta %v", g_KbHit);

  ret = g_KbHit;

  return ret;
}
******************/

/*
 *
 * Output date/time to LCD.
 *
 */
#if defined(LCD_EN)
void enhsh_clock(void)
{
    unsigned char date;

    lcd_clear();
    enhsh_date(2);
    lcd_puts(ibuf1, lcdlinesel[1]); // output date part
    date = clkdata.date;
    while (kbhit()) getc(); // flush RX buffer
    while(1) {

        enhsh_date(2);
        // update date part only if date changes
        if (clkdata.date != date) {

            lcd_puts(ibuf1, lcdlinesel[1]);
            date = clkdata.date;
        }
        lcd_puts(ibuf2, lcdlinesel[2]); // output time part
        pause_sec(1);
        if (kbhit()) {  // interrupt from keyboard
            getc();
            break;
        }
    }
}
#endif

/*
 * Handle command: rnv Bank#
 * Dump contents of non-volatile RTC RAM from specified Bank# (0 or 1).
 * Bank 0 : 114 bytes are dumped ($0E - $7F) from RTC NV RAM.
 * Bank 1 : 128 bytes are dumped ($00 - $7F) from RTC NV RAM.
 * If optional hexaddr argument is provided, the NV memory will be
 * dumped in RAM at specified address.
 */
void enhsh_rnv(void)
{
    int i, len;
    unsigned char b     = 0x00;
    unsigned char bank  = 0;
    unsigned char offs  = 0;
    unsigned char offs2 = 0;
    uint16_t addr       = 0x000e;
    uint16_t ramaddr    = 0x0000;
    int optarg = 0; // optional argument provided (0 - no / 1 - yes)

    if (!RTCDETECTED) {
        enhsh_prnerror(ERROR_NORTC);
        return;
    }

    len = strlen(prompt_buf);   // length of prompt before cut off at 1=st arg.
    offs = adv2nxttoken(3);     // find beginning of 1-st arg. (bank #)
    offs2 = adv2nextspc(offs);  // find the end of 1-st arg.
    bank = atoi(prompt_buf+offs);
    if (bank) addr=0;
    /*
        optional argument, hexaddr
        if full len of prompt greater than prompt cut at end of 1-st arg.
    */
    if (len > strlen(prompt_buf)) {

        optarg = 1;
        // find the start index of 2-nd argument (hexaddr)
        offs2 = adv2nxttoken(offs2);
        ramaddr = hex2int(prompt_buf+offs2);
        puts("RAM copy address: ");
        puts(itoa(ramaddr, ibuf3, RADIX_HEX));
        puts("\n\r");
    }
    puts("RTC NV Bank #: ");
    puts(itoa(bank, ibuf3, RADIX_DEC));
    puts("\n\r");
    for ( ; addr<0x0080; addr+=16) {

        itoa(addr,ibuf1,RADIX_HEX);
        if (strlen(ibuf1) < 2) {

            for (i=2-strlen(ibuf1); i>0; i--)
                putchar('0');
        }
        puts(ibuf1);
        puts(" : ");
        for (offs=0; offs<16 && (addr+offs)<0x0080; offs++) {

            b = ds1685_readram(bank, (addr+offs)&0x00ff);
            if (optarg) {
                POKE(ramaddr++, b);
            }
            if (b < 16) putchar('0');
            puts(itoa(b,ibuf3,RADIX_HEX));
            putchar(' ');
        }
        puts(" : ");
        for (offs=0; offs<16 && (addr+offs)<0x0080; offs++) {

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
void enhsh_wnv(void)
{
    int i,n;
    unsigned char b = 0x00;
    unsigned char bank = 0;
    uint16_t addr = 0x0000;
    uint16_t nvaddr = 0x000e;

    if (!RTCDETECTED) {
        enhsh_prnerror(ERROR_NORTC);
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
    for ( ; nvaddr<0x0080; addr++, nvaddr++) {

        b = PEEK(addr);
        ds1685_storeram(bank, nvaddr&0x00ff, b);
    }
}

/*
 * Initialize RTC chip.
 */
 /*
void enhsh_rtci(void)
{
    if (!RTCDETECTED) {
        enhsh_prnerror(ERROR_NORTC);
        return;
    }

    ds1685_init(DSC_REGB_SQWE | DSC_REGB_DM | DSC_REGB_24o12 | DSC_REGB_PIE,
    // Square Wave output enable, binary mode, 24h format,
    // periodic interrupt
                DSC_REGA_OSCEN | 0x0A, // oscillator ON, 64 Hz on SQWE
                0xB0, // AUX battery enable, 12.5pF crystal,
                      // E32K=0 - SQWE pin frequency set by RS3..RS0,
                      // CS=1 - 12.5pF crystal,
                      // RCE=1 - RAM clear pin /RCLR enabled
                0x08  // PWR pin to high-impedance state
            );
}
*/

/*
 *
 * Execute command.
 *
 */
int enhsh_exec(void)
{
    int ret = 1;

    switch (cmd_code)
    {
        case CMD_NULL:
            break;
        case CMD_UNKNOWN:
            enhsh_prnerror(ERROR_BADCMD);
            break;
        case CMD_CLS:
            enhsh_cls();
            break;
        case CMD_EXIT:
            puts("Bye!\n\r");
            ret = 0;
            break;
#if defined(LCD_EN)
        case CMD_LCDINIT:
            enhsh_lcdinit();
            break;
        case CMD_LCDPRINT:
            enhsh_lcdp();
            break;
        case CMD_LCDCLEAR:
            lcd_clear();
            break;
#endif
        case CMD_READMEM:
            enhsh_readmem();
            break;
        case CMD_WRITEMEM:
            enhsh_writemem();
            break;
        case CMD_INITMEM:
            enhsh_initmem();
            break;
        case CMD_EXECUTE:
            enhsh_execmem();
            break;
        case CMD_DATE:
            //enhsh_date(1);
            enhsh_prndt();
            break;
        case CMD_SETDTTM:
            //enhsh_time(1);
            enhsh_setdt();
            break;
#if defined(LCD_EN)
        case CMD_CLOCK:
            enhsh_clock();
            break;
#endif
        case CMD_HELP:
            enhsh_help();
            break;
        case CMD_RNV:
            enhsh_rnv();
            break;
        case CMD_WNV:
            enhsh_wnv();
            break;
        case CMD_SHOWTMCT:
            enhsh_showtmct();
            break;
        case CMD_RAMBANK:
            //enhsh_rambanksel();
            enhsh_bramsel();
            break;
        case CMD_CONV:
            enhsh_conv();
            break;
        case CMD_MEMCPY:
            enhsh_mcp();
            break;
        default:
            break;
    }

    return ret;
}

/*
 *
 * Initialization and command shell loop.
 *
 */
void enh_shell(void)
{
    int cont = 1;

    while(cont)
    {
        puts("mkhbc>");
        enhsh_getline();
        enhsh_parse();
        cont = enhsh_exec();
    }
}

void enhsh_banner(void)
{
    //pause_sec(2);
    while (kbhit()) getc(); // flush RX buffer
    memset(prompt_buf, 0, PROMPTBUF_SIZE);
    puts("\n\r\n\r");
    puts("Enhanced Monitor / Shell ");
    enhsh_version();
    puts("(C) Marek Karcz 2012-2018. All rights reserved.\n\r");
    puts("  'help' for guide.\n\r\n\r");
#if defined(LCD_EN)
    LCD_INIT;
    lcd_puts("   MKHBC-8-R2   ", LCD_LINE_1);
    lcd_puts(" 6502 @ 1.8 Mhz ", LCD_LINE_2);
#endif
}

/*
 * Show the value of counter updated in interrupt 64 times / sec.
 * Display decimal and hexadecimal format.
 */
void enhsh_showtmct(void)
{
    unsigned long tmr64 = *TIMER64HZ;

    //tmr64 = *TIMER64HZ;
    strcpy(ibuf1, ultoa(tmr64, ibuf3, RADIX_DEC));
    strcat(ibuf1, " $");
    strcat(ibuf1, ultoa(tmr64, ibuf3, RADIX_HEX));
    strcat(ibuf1, "\n\r");
    puts(ibuf1);
}

//int g_bnum;
/*
 * Select banked RAM bank.
 */
/*********************
void enhsh_rambanksel(void)
{
    int tok1;
    g_bnum = -1;

    if (!EXTRAMDETECTED || !EXTRAMBANKED) {
        enhsh_prnerror(ERROR_NOEXTBRAM);
        return;
    }

    if (strlen(prompt_buf) > 5) {

        tok1 = adv2nxttoken(5);
        adv2nextspc(tok1);
        g_bnum = atoi(prompt_buf + tok1);

        if (g_bnum >= 0 && g_bnum < 8) {

            __asm__("lda %v", g_bnum);
            __asm__("jsr %w", MOS_BANKEDRAMSEL);

        } else {

            enhsh_prnerror(ERROR_BANKNUM);
        }
    }

    enhsh_getrambank();
}
**************/
/*
 * Get current banked RAM bank #.
 */
/**********************
void enhsh_getrambank(void)
{
    unsigned char rambank = 0;

    rambank = *RAMBANKNUM;
    puts("Current BRAM bank#: ");
    puts(utoa((unsigned int)rambank, ibuf3, RADIX_DEC));
    puts("\n\r");
}
*****************/

/*
 * Dec/hex/bin conversion.
 * NOTE: Due to code size constraints that I wanted to adhere to,
 *       validation of proper input value is not implemented.
 */
void enhsh_conv()
{
    int tok1, tok2, j, k, pos;
    int dv = 0;
    char *hexval = ibuf1;
    char *binval = ibuf2;
    char *decval = ibuf3;

    if (strlen(prompt_buf) > 5) {

        tok1 = adv2nxttoken(5);     // conversion type
        tok2 = adv2nextspc(tok1);
        tok2 = adv2nxttoken(tok2);  // argument
        adv2nextspc(tok2);
        if (0 == strncmp(prompt_buf + tok1, "d2hb", 4)) {

            ultoa((unsigned long)atol(prompt_buf + tok2), hexval, RADIX_HEX);
            ultoa((unsigned long)atol(prompt_buf + tok2), binval, RADIX_BIN);
            strcpy(decval, prompt_buf + tok2);

        } else if (0 == strncmp(prompt_buf + tok1, "h2db", 4)) {
            // validate if proper hexadecimal value
            j = tok2;
            while (*(prompt_buf + j) != 0) {
                if (*(prompt_buf + j) < 48
                    || (*(prompt_buf + j) > 57 && *(prompt_buf + j) < 65)
                    || (*(prompt_buf + j) > 70 && *(prompt_buf + j) < 97)
                    || *(prompt_buf + j) > 102
                  ) {
                    enhsh_prnerror(ERROR_CHECKARGS);
                    return;
                }
                j++;
            }
            dv = hex2int(prompt_buf + tok2);
            utoa((unsigned int)dv, decval, RADIX_DEC);
            utoa((unsigned int)dv, binval, RADIX_BIN);
            strcpy(hexval, prompt_buf + tok2);

        } else if (0 == strncmp(prompt_buf + tok1, "b2hd", 4)) {

            j = tok2;
            pos = strlen(prompt_buf + tok2) - 1;
            // convert binary to decimal
            while (*(prompt_buf + j) != 0) {
                // validate if proper binary digit
                if (*(prompt_buf + j) != '1'  && *(prompt_buf + j) != '0') {
                    enhsh_prnerror(ERROR_CHECKARGS);
                    return;
                }
                k = ((*(prompt_buf + j) == '1') ? 1 : 0);
                dv += k * power(2, pos);
                pos--;
                j++;
            }
            utoa((unsigned int)dv, hexval, RADIX_HEX);
            utoa((unsigned int)dv, decval, RADIX_DEC);
            strcpy(binval, prompt_buf + tok2);
        }
        else {
          enhsh_prnerror(ERROR_CHECKARGS);
        }
        puts("Dec: ");
        puts(decval);
        puts("\n\r");
        puts("Hex: ");
        puts(hexval);
        puts("\n\r");
        puts("Bin: ");
        puts(binval);
        puts("\n\r");
    } else {
        enhsh_prnerror(ERROR_CHECKARGS);
    }
}

int main(void)
{
    enhsh_banner();
    enh_shell();
    return 0;
}
