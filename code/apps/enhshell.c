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
 *  ..........................................................................
 *
 *  BUGS:
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

#define IBUF1_SIZE      80
#define IBUF2_SIZE      80
#define IBUF3_SIZE      32
#define PROMPTBUF_SIZE  80
#define RADIX_DEC       10
#define RADIX_HEX       16
#define RADIX_BIN       2

 /*#define LCD_EN 1*/

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
#endif
	CMD_READMEM,
	CMD_RMEMENH,	  // enhanced read memory
	CMD_WRITEMEM,
	CMD_INITMEM,	  // initialize memory with value
	CMD_DATE,		    // read DS1685 RTC data
	CMD_SETCLOCK,	  // set DS1685 time
	CMD_SETDTTM,	  // set DS1685 date and time
	CMD_CLOCK,		  // LCD clock
	CMD_SLEEP,
	CMD_RCLK,
	CMD_WNV,
	CMD_RNV,
  CMD_RTCI,
	CMD_EXECUTE,
	CMD_VERSION,
  CMD_SHOWTMCT,   // show periodically updated (in interrupt) counter
  CMD_RAMBANK,    // select or get banked RAM bank#
  CMD_DEC2HEXBIN, // decimal to hex/bin conversion
	//-------------
	CMD_UNKNOWN
};

const int ver_major = 2;
const int ver_minor = 6;
const int ver_build = 9;

int cmd_code = CMD_NULL;
char prompt_buf[PROMPTBUF_SIZE];
char ibuf1[IBUF1_SIZE], ibuf2[IBUF2_SIZE], ibuf3[IBUF3_SIZE];
#if defined(LCD_EN)
const int lcdlinesel[] = {LCD_LINE_CURRENT, LCD_LINE_1, LCD_LINE_2};
#endif
const char hexcodes[] = {'0', '1', '2', '3', '4', '5', '6', '7', '8', '9',
					'a', 'b', 'c', 'd', 'e', 'f', 0};
const char daysofweek[8][4] =
{
	"Sun", "Mon", "Tue", "Wed", "Thu", "Fri", "Sat"
};

const char monthnames[13][4] =
{
	"Jan", "Feb", "Mar", "Apr", "May", "Jun", "Jul", "Aug", "Sep", "Oct", "Nov", "Dec"
};

enum eErrors {
  ERROR_OK = 0,
  ERROR_NORTC,
  ERROR_NOEXTBRAM,
  ERROR_BANKNUM,
  ERROR_DECVALEXP,
  ERROR_UNKNOWN
};

const char ga_errmsg[6][56] =
{
  "OK.",
  "No RTC chip detected.",
  "No extended RAM detected or extended RAM is not banked.",
  "Bank# expected value: 0..7.",
  "Expected decimal argument.",
  "Unknown."
};

#if defined(LCD_EN)
const char helptext[39][48] =
#else
const char helptext[35][48] =
#endif
{
	"\n\r",
	"    cls : clear console.\n\r",
#if defined(LCD_EN)
	"   lcdi : initialize LCD.\n\r",
	"   lcdc : clear LCD.\n\r",
	"   lcdp : print to LCD.\n\r",
#endif
	"      r : read memory contents.\n\r",
	"          r <hexaddr>[-hexaddr]\n\r",
	"      w : write data to address.\n\r",
	"          w <hexaddr> <hexdata> [hexdata] ...\n\r",
	"      i : initialize memory with value.\n\r",
	"          i <hexaddr> <hexaddr> [hexdata]\n\r",
	"      x : execute at address.\n\r",
	"          x <hexaddr>\n\r",
	"   date : display date/time.\n\r",
	"   time : set time.\n\r",
	"  setdt : set date/time.\n\r",
#if defined(LCD_EN)
	"  clock : run LCD clock (reset to exit).\n\r",
#endif
	"  sleep : pause for # of seconds\n\r",
	"          sleep <seconds>\n\r",
	"   rclk : run console clock for # of seconds\n\r",
	"          rclk <seconds>\n\r",
	"   dump : enhanced read memory/hex dump.\n\r",
	"          dump <hexaddr>[ hexaddr]\n\r",
	"   wnv  : write to non-volatile RTC RAM.\n\r",
	"          wnv <bank#> <hexaddr>\n\r",
	"   rnv  : dump non-volatile RTC RAM.\n\r",
	"          rnv <bank#> [<hexaddr>]\n\r",
  "   rtci : initialize RTC chip.\n\r",
  "   sctr : show 64 Hz counter value.\n\r",
  "          sctr [<num> <mon-interval>]\n\r",
  "   bank : see or select banked RAM bank.\n\r",
  "          bank [<bank#(0..7)>]\n\r",
  "   d2hb : decimal to hex/bin conversion.\n\r",
  "          d2hb <decnum>\n\r",
	"    ver : print version number.\n\r",
	"   exit : exit enhanced shell.\n\r",
	"   help : print this message.\n\r",
	"\n\r",
	"@EOH"
};

struct ds1685_clkdata	clkdata;

void enhsh_pause(uint16_t delay);
void pause_sec(uint16_t delay);
void enhsh_getline(void);
void enhsh_parse(void);
void enhsh_lcdp(void);
void enhsh_help(void);
void enhsh_readmem(void);
void enhsh_writemem(void);
void enhsh_execmem(void);
void enhsh_version(void);
void enhsh_cls(void);
void enhsh_lcdinit(void);
int hexchar2int(char c);
int power(int base, int exp);
int hex2int(const char *hexstr);
//int kbhit(void);
void enhsh_initmem(void);
void enhsh_rmemenh(void);
void enhsh_date(unsigned char rdclk);
void enhsh_sleep();
void enhsh_time(unsigned char setdt);
void enhsh_clock(void);
void enhsh_rclk(void);
void enhsh_rnv(void);
void enhsh_wnv(void);
void enhsh_rtci(void);
int enhsh_exec(void);
void enh_shell(void);
void enhsh_banner(void);
void enhsh_showtmct(void);
void enhsh_rambanksel(void);
void enhsh_getrambank(void);
void enhsh_dec2hexbin(void);
void enhsh_prnerror(int errnum);

/* print error message */
void enhsh_prnerror(int errnum)
{
  puts("ERROR: ");
  puts(ga_errmsg[errnum]);
  puts("\r\n");
}

/* pause */
void enhsh_pause(uint16_t delay)
{
	int i = 0;

	for(i=0; i < delay; i++);
}

/* pause specified # of seconds */
void pause_sec(uint16_t delay)
{
	unsigned int currsec = 0, nsec = delay;

  if (RTCDETECTED) {
  	ds1685_rdclock (&clkdata);
  	while (nsec > 0) {

  		currsec = clkdata.seconds;
  		/* wait 1 second */
  	  while (currsec == clkdata.seconds) {

  	 	  enhsh_pause(200);
  	 	  ds1685_rdclock (&clkdata);
  	  }
  	  nsec--;
      if (KBHIT) break; // must be able to interrupt from keyboard
                        // remember to flush RX buffer vefore calling this
                        // function
  	}
  }
}

/* get line of text from input */
void enhsh_getline(void)
{
	memset(prompt_buf,0,PROMPTBUF_SIZE);
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
	else if (0 == strncmp(prompt_buf,"dump ",5))
	{
		cmd_code = CMD_RMEMENH;
	}
	else if (0 == strncmp(prompt_buf,"w ",2))
	{
		cmd_code = CMD_WRITEMEM;
	}
	else if (0 == strncmp(prompt_buf,"i ",2))
	{
		cmd_code = CMD_INITMEM;
	}
	else if (0 == strncmp(prompt_buf,"date",4))
	{
		cmd_code = CMD_DATE;
	}
	else if (0 == strncmp(prompt_buf,"time",4))
	{
		cmd_code = CMD_SETCLOCK;
	}
	else if (0 == strncmp(prompt_buf,"setdt",5))
	{
		cmd_code = CMD_SETDTTM;
	}
#if defined(LCD_EN)
	else if (0 == strncmp(prompt_buf,"clock",5))
	{
		cmd_code = CMD_CLOCK;
	}
#endif
	else if (0 == strncmp(prompt_buf,"sleep ",6))
	{
		cmd_code = CMD_SLEEP;
	}
	else if (0 == strncmp(prompt_buf,"rclk ",5))
	{
		cmd_code = CMD_RCLK;
	}
	else if (0 == strncmp(prompt_buf,"x ",2))
	{
		cmd_code = CMD_EXECUTE;
	}
	else if (0 == strncmp(prompt_buf,"ver",3))
	{
		cmd_code = CMD_VERSION;
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
	else if (0 == strncmp(prompt_buf,"rtci",4))
	{
		cmd_code = CMD_RTCI;
	}
  else if (0 == strncmp(prompt_buf,"sctr",4))
  {
    cmd_code = CMD_SHOWTMCT;
  }
  else if (0 == strncmp(prompt_buf,"bank",4))
  {
    cmd_code = CMD_RAMBANK;
  }
  else if (0 == strncmp(prompt_buf,"d2hb",4))
  {
    cmd_code = CMD_DEC2HEXBIN;
  }
	else
		cmd_code = CMD_UNKNOWN;
}

/* get text from console and print on LCD */
void enhsh_lcdp(void)
{
#if defined(LCD_EN)
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
	//else
	//	puts("Unable to complete request.\n\r");
#else
	puts("LCD is not enabled.\n\r");
#endif
}

/* print help */
void enhsh_help(void)
{
	unsigned char cont = 1, i = 0;

	while(cont)
	{
		if (0 == strncmp(helptext[i],"@EOH",4))
			cont = 0;
		else
			puts(helptext[i++]);
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

void enhsh_version(void)
{
	//strcpy(ibuf1, "MKHBCOS Enhanced Shell, version: ");
	strcpy(ibuf1, itoa(ver_major, ibuf3, RADIX_DEC));
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

void enhsh_lcdinit(void)
{
#if defined(LCD_EN)
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
#else
	puts("LCD is not enabled.\n\r");
#endif
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
 * Initialize memory - command handler.
 * i startaddr endaddr val
 * E.g.: i 8000 bfff 01
 *
 */
void enhsh_initmem(void)
{
	uint16_t staddr = 0x0000;
	uint16_t enaddr = 0x0000;
	uint16_t addr = 0x0000;
	int i, j, k;
	unsigned char b;

	i=2; j=k=0; b=0;
	while (*(prompt_buf+i) != 0) {
		if (*(prompt_buf+i) == 32 || *(prompt_buf+i) == '-') {
			*(prompt_buf+i) = 0;
			i++;
			if (j == 0) j = i;
			else {
				k = i;
				break;
			}
		}
		i++;
	}
	staddr = hex2int(prompt_buf+2);
	if (j > 0) enaddr = hex2int(prompt_buf+j);
	if (k > 0) b = hex2int(prompt_buf+k);
	if (enaddr == 0x0000) {
		enaddr = staddr + 0x0100;
	}

	for (addr=staddr; addr<enaddr; addr++)
	{
		POKE(addr,b);
	}
}

/*
 *
 * Read memory range and output hexadecimal
 * values of memory locations as well as
 * ascii representation if applicable.
 * This function will avoid reading the I/O mapped
 * memory range to avoid undesired side effects from
 * I/O devices or memory banking register (which is
 * also sensitive to reading operations).
 *
 */
void enhsh_rmemenh(void)
{
	uint16_t staddr = 0x0000;
	uint16_t enaddr = 0x0000;
	uint16_t addr = 0x0000;
	unsigned char b = 0x00;
	int i;

    /*
	puts("Start addr. (4-digit hex):");
	gets(ibuf1);
	puts("\n\rEnd addr.   (4-digit hex):");
	gets(ibuf2);
	puts("\n\r");

	staddr = hex2int(ibuf1);
	enaddr = hex2int(ibuf2);
	*/

	i=5;
	while (*(prompt_buf+i) != 0) {
		if (*(prompt_buf+i) == 32 || *(prompt_buf+i) == '-') {
			*(prompt_buf+i) = 0;
			i++;
        	enaddr = hex2int(prompt_buf+i);
        	break;
		}
		i++;
	}
	staddr = hex2int(prompt_buf+5);
	if (enaddr == 0x0000) {
		enaddr = staddr + 0x0100;
	}

  while (KBHIT) getc(); // flush RX buffer
	for (addr=staddr; addr<enaddr; addr+=16)
	{
		utoa(addr,ibuf1,RADIX_HEX);
		if (strlen(ibuf1) < 4)
		{
			for (i=4-strlen(ibuf1); i>0; i--)
				putchar('0');
		}
		puts(ibuf1);
		puts(" : ");
		if (addr > 0xBFFF && addr < 0xC800) {
			puts ("dump blocked in I/O mapped range\n\r");
			continue;
		}
		for (i=0; i<16; i++)
		{
			b = PEEK(addr+i);
			if (b < 16)
				putchar('0');
			puts(itoa(b,ibuf3,RADIX_HEX));
			putchar(' ');
		}
		puts(" : ");
		for (i=0; i<16; i++)
		{
			b = PEEK(addr+i);
			if (b > 31 && b < 127)
				putchar(b);
			else
				putchar('?');
		}
		puts("\n\r");
		if (0xffff - enaddr <= 0x0f && addr < staddr)
			break;
    if (KBHIT) {  // interrupt from keyboard
      if (32 == getc()) { // if SPACE,
        while (!KBHIT) /* wait for any keypress to continue */ ;
        getc(); // consume the key
      } else {  // if not SPACE, break (exit) the loop
        break;
      }
    }
	}
}

/*
 *
 * Read date/time from DS1685 IC and output to the console
 * and/or convert to string buffers ibuf1, ibuf2 for display.
 * Input: rdclk
 * 0 - do not read the actual data from DS1685 chip,
 *     just output the values in global clkdata variable.
 * 1 - Read data from DS1685 chip and output to console.
 * 2 - Read data from DS1685 chip and convert to strings ibuf1, ibuf2.
 *     (the purpose is to prepare data for LCD display).
 * 3 - Read data from DS1685 chip, do not output, do not convert to
 *     string buffers, just store in clkdata.
 *
 */
void enhsh_date(unsigned char rdclk)
{
  if (!RTCDETECTED) {
      enhsh_prnerror(ERROR_NORTC);
      return;
  }

	if(rdclk)
		ds1685_rdclock (&clkdata);

	if (rdclk == 3)
		return;

	memset(ibuf1, 0, IBUF1_SIZE);
	memset(ibuf2, 0, IBUF2_SIZE);
	memset(ibuf3, 0, IBUF3_SIZE);
	if (rdclk < 2) strcpy(ibuf1, "Date: ");
	if (clkdata.dayofweek > 0 && clkdata.dayofweek < 8)
	{
		if (rdclk < 2)
			strcat(ibuf1, daysofweek[clkdata.dayofweek-1]);
		else
			strcpy(ibuf1, daysofweek[clkdata.dayofweek-1]);
	}
	else
	{
		if (rdclk < 2)
			strcat(ibuf1, "???");
		else
			strcpy(ibuf1, "???");
	}
	strcat(ibuf1, ", ");
	strcat(ibuf1, itoa(clkdata.date, ibuf3, RADIX_DEC));
	strcat(ibuf1, " ");
	if (clkdata.month > 0 && clkdata.month < 13)
		strcat(ibuf1, monthnames[clkdata.month-1]);
	else
		strcat(ibuf1, "???");
	strcat(ibuf1, " ");
	if (clkdata.century < 100)
		strcat(ibuf1, itoa(clkdata.century, ibuf3, RADIX_DEC));
	else
		strcat(ibuf1, "??");
	if(clkdata.year < 100)
	{
		if (clkdata.year < 10)
			strcat(ibuf1, "0");
		strcat(ibuf1, itoa(clkdata.year, ibuf3, RADIX_DEC));
	}
	else
		strcat(ibuf1, "??");
	if (rdclk < 2)
	{
		strcat(ibuf1, "\r\n");
		puts(ibuf1);
	}

	if (rdclk < 2) strcpy(ibuf2, "Time: ");
	else strcpy(ibuf2, "  ");
	if (clkdata.hours < 10)
		strcat(ibuf2, "0");
	strcat(ibuf2, itoa(clkdata.hours, ibuf3, RADIX_DEC));
	strcat(ibuf2, " : ");
	if (clkdata.minutes < 10)
		strcat(ibuf2, "0");
	strcat(ibuf2, itoa(clkdata.minutes, ibuf3, RADIX_DEC));
	strcat(ibuf2, " : ");
	if (clkdata.seconds < 10)
		strcat(ibuf2, "0");
	strcat(ibuf2, itoa(clkdata.seconds, ibuf3, RADIX_DEC));
	if (rdclk < 2)
	{
		strcat(ibuf2, "\r\n");
		puts(ibuf2);
	}
}

/*
 *
 * Handler of command: sleep N
 * Sleep for number of seconds provided in command line.
 *
 */
void enhsh_sleep()
{
	unsigned int sleptsec = 0, cmdarg = 0;
	unsigned char currsec = 0;
	cmdarg = atoi(prompt_buf+6);
  while (KBHIT) getc(); // flush RX buffer
	pause_sec(cmdarg);
}

/*
 *
 * Prompt and set date/time in DS1685 RTC chip.
 * Input: setdt
 * 0 - do not prompt for and set date.
 * 1 - prompt for and set date and time.
 *
 */
void enhsh_time(unsigned char setdt)
{
	uint16_t n = 0;

  if (!RTCDETECTED) {
      enhsh_prnerror(ERROR_NORTC);
      return;
  }

	if (setdt)
	{
		puts("Enter the century (19,20): ");
		gets(ibuf1);
		puts("\r\n");
		n = atoi(ibuf1);
		clkdata.century = (unsigned char) n;
		puts("Enter the year (0-99): ");
		gets(ibuf1);
		puts("\r\n");
		n = atoi(ibuf1);
		clkdata.year = (unsigned char) n & 0x7f;
		puts("Enter the month (1-12): ");
		gets(ibuf1);
		puts("\r\n");
		n = atoi(ibuf1);
		clkdata.month = (unsigned char) n & 0x0f;
		puts("Enter the date (1-31): ");
		gets(ibuf1);
		puts("\r\n");
		n = atoi(ibuf1);
		clkdata.date = (unsigned char) n & 0x1f;
		puts("Enter the day (1-7, Sun=1..Sat=7): ");
		gets(ibuf1);
		puts("\r\n");
		n = atoi(ibuf1);
		clkdata.dayofweek = (unsigned char) n & 0x07;
	}
	puts("Enter the hours (0-23): ");
	gets(ibuf1);
	puts("\r\n");
	n = atoi(ibuf1);
	clkdata.hours = (unsigned char) n & 0x1f;
	puts("Enter the minutes (0-59): ");
	gets(ibuf1);
	puts("\r\n");
	n = atoi(ibuf1);
	clkdata.minutes = (unsigned char) n & 0x3f;
	puts("Enter the seconds (0-59): ");
	gets(ibuf1);
	puts("\r\n");
	n = atoi(ibuf1);
	clkdata.seconds = (unsigned char) n & 0x3f;

	//puts("Setting DS1685 RTC...\r\n");
	//enhsh_date(0);

	if (setdt)
		ds1685_setclock (&clkdata);
	else
		ds1685_settime	(&clkdata);
}

/*
 *
 * Output date/time to LCD.
 *
 */
void enhsh_clock(void)
{
#if defined(LCD_EN)
	unsigned char date;

	lcd_clear();
	enhsh_date(2);
	lcd_puts(ibuf1, lcdlinesel[1]); // output date part
	date = clkdata.date;
  while (KBHIT) getc(); // flush RX buffer
	while(1)
	{
		enhsh_date(2);
		// update date part only if date changes
		if (clkdata.date != date)
		{
			lcd_puts(ibuf1, lcdlinesel[1]);
			date = clkdata.date;
		}
		lcd_puts(ibuf2, lcdlinesel[2]); // output time part
		pause_sec(1);
    if (KBHIT) {  // interrupt from keyboard
      getc();
      break;
    }
	}
#else
	puts("LCD disabled.\n\r");
#endif
}

/*
 *
 * Handle command: rclk N
 * Run date/time on console for N seconds, then quit.
 *
 */
void enhsh_rclk(void)
{
   unsigned int cmdarg = 0, secs = 0;

   if (!RTCDETECTED) {
     enhsh_prnerror(ERROR_NORTC);
     return;
   }

   cmdarg = atoi(prompt_buf+5);
   /*enhsh_cls();*/
   ansi_cls();
   //while (KBHIT) getc();  // flush rx buffer
   while (kbhit()) getc();  // flush rx buffer
   while(secs < cmdarg)
   {
   	  /*ansi_crsr_home();*/
      enhsh_date(1);
      pause_sec(1);
      secs++;
   	  ansi_set_cursor(1,1);
      //if (KBHIT) break;
      if (kbhit()) {
        getc();
        break;
      }
   }
   ansi_set_cursor(1,3);
}

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
  unsigned char b     = 0x00;
  unsigned char bank  = 0;
  unsigned char offs  = 0;
  unsigned char offs2 = 0;
  uint16_t addr       = 0x000e;
  uint16_t ramaddr    = 0x0000;
  int i, len;

  if (!RTCDETECTED) {
      enhsh_prnerror(ERROR_NORTC);
      return;
  }

  len = strlen(prompt_buf); // length of prompt before cut off at 1=st arg.
  // find beginning of 1-st arg. (bank #)
	offs=3;
	while (prompt_buf[offs] == 32) {
    offs++;
	}
  // find the end of 1-st arg.
  offs2 = offs;
	while (prompt_buf[offs2] != 32) {
    offs2++;
	}
  prompt_buf[offs2++] = 0;
	bank = atoi(prompt_buf+offs);
  if (bank) addr=0;
  // optional argument, hexaddr
  // if full len of prompt greater than prompt cut at end of 1-st arg.
  if (len > strlen(prompt_buf)) {
    // find the start index of 2-nd argument (hexaddr)
	  while (prompt_buf[offs2] == 32) {
      offs2++;
	  }
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
			for (i=2-strlen(ibuf1); i>0; i--)
				putchar('0');
		}
		puts(ibuf1);
		puts(" : ");
		for (offs=0; offs<16 && (addr+offs)<0x0080; offs++)
		{
			b = ds1685_readram(bank, (addr+offs)&0x00ff);
      if (ramaddr >= 0x4000) { // don't write over self
        POKE(ramaddr++, b);
      }
			if (b < 16)
				putchar('0');
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
void enhsh_wnv(void)
{
  unsigned char b = 0x00;
  unsigned char bank = 0;
  uint16_t addr = 0x0000;
  uint16_t nvaddr = 0x000e;
  int i,n;

  if (!RTCDETECTED) {
      enhsh_prnerror(ERROR_NORTC);
      return;
  }

	i=3;
  /* find index of bank value start */
	while (prompt_buf[i] == 32) {
    i++;
	}
  n=i;
  /* find index of end of bank value */
	while (prompt_buf[i] != 0 && prompt_buf[i] != 32) {
    i++;
	}
  prompt_buf[i++] = 0;
  /* find index of hexaddr value start */
	while (prompt_buf[i] == 32) {
    i++;
	}
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
 * Initialize RTC chip.
 */
void enhsh_rtci(void)
{
  if (!RTCDETECTED) {
      enhsh_prnerror(ERROR_NORTC);
      return;
  }

	ds1685_init(DSC_REGB_SQWE | DSC_REGB_DM | DSC_REGB_24o12 | DSC_REGB_PIE,
				/* Square Wave output enable, binary mode, 24h format, periodic interrupt */
				DSC_REGA_OSCEN | 0x0A, /* oscillator ON, 64 Hz on SQWE */
				0xB0, /* AUX battery enable, 12.5pF crystal,
						 E32K=0 - SQWE pin frequency set by RS3..RS0,
             CS=1 - 12.5pF crystal,
             RCE=1 - RAM clear pin /RCLR enabled */
				0x08 /* PWR pin to high-impedance state */);
}

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
			puts("Invalid command.\n\r");
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
		case CMD_RMEMENH:
			enhsh_rmemenh();
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
			enhsh_date(1);
			break;
		case CMD_SETCLOCK:
			enhsh_time(0);
			break;
		case CMD_SETDTTM:
			enhsh_time(1);
			break;
#if defined(LCD_EN)
		case CMD_CLOCK:
			enhsh_clock();
			break;
#endif
		case CMD_SLEEP:
		    enhsh_sleep();
		    break;
		case CMD_RCLK:
		    enhsh_rclk();
		    break;
		case CMD_VERSION:
			enhsh_version();
			break;
		case CMD_HELP:
			enhsh_help();
			break;
		case CMD_RNV:
			enhsh_rnv();
			break;
		case CMD_WNV:
			enhsh_wnv();
			break;
    case CMD_RTCI:
      enhsh_rtci();
      break;
    case CMD_SHOWTMCT:
      enhsh_showtmct();
      break;
    case CMD_RAMBANK:
      enhsh_rambanksel();
      break;
    case CMD_DEC2HEXBIN:
      enhsh_dec2hexbin();
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

	/**** this is now in EPROM called when system boots
	ds1685_init(DSC_REGB_SQWE | DSC_REGB_DM | DSC_REGB_24o12 | DSC_REGB_PIE,
				// Square Wave output enable, binary mode, 24h format, periodic int.
				DSC_REGA_OSCEN | 0x0A, // oscillator ON, 64 Hz on SQWE
				0xB0, // AUX battery enable, 12.5pF crystal,
						  // E32K=0 - SQWE pin frequency set by RS3..RS0,
              // CS=1 - 12.5pF crystal,
              // RCE=1 - RAM clear pin /RCLR enabled
				0x08  // PWR pin to high-impedance state
        );
	***/
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
	//enhsh_cls();
  enhsh_pause(2);
  while (kbhit()) getc(); // flush RX buffer
	memset(prompt_buf,0,PROMPTBUF_SIZE);
  //ansi_cls();
  puts("\n\r\n\r");
	puts("Welcome to MKHBC-8-R2 M.O.S. Enhanced Shell.\n\r");
	puts("(C) Marek Karcz 2012-2018. All rights reserved.\n\r");
	puts("  'help' for guide.\n\r");
	puts("  'exit' to quit.\n\r\n\r");
#if defined(LCD_EN)
	LCD_INIT;
	lcd_puts("   MKHBC-8-R2   ", LCD_LINE_1);
	lcd_puts(" 6502 @ 1.8 Mhz ", LCD_LINE_2);
#else
	puts("LCD disabled.\n\r");
#endif
}

/*
 * Show the value of counter updated in interrupt 64 times / sec.
 * Display decimal and hexadecimal format.
 * If optional arguments were provided in command line: num, delay
 * then the value of the counter will be shown num times in a loop
 * with delay # of seconds between each loop run.
 */
void enhsh_showtmct(void)
{
  unsigned long tmr64;
  int offs = 5;
  int offs2 = 6;
  int num = 1;
  int delay = 0;
  int i = 0;

  if (!RTCDETECTED) {
      enhsh_prnerror(ERROR_NORTC);
      return;
  }

  if (strlen(prompt_buf) > 4) {
	  while (prompt_buf[offs] == 32) {
      offs++;
	  }
    offs2 = offs;
    while (prompt_buf[offs2] != 32) {
      offs2++;
    }
    prompt_buf[offs2++] = 0;
    while (prompt_buf[offs2] == 32) {
      offs2++;
    }
    num = atoi(prompt_buf+offs);
    delay = atoi(prompt_buf+offs2);
  }
  puts("Loop count: ");
  puts(itoa(num, ibuf3, RADIX_DEC));
  puts(", ");
  puts("Delay: ");
  puts(itoa(delay, ibuf3, RADIX_DEC));
  puts(" seconds.\n\r");
  //while (KBHIT) getc(); // flush rx buffer
  while (kbhit()) getc(); // flush rx buffer
  for (i = 0; i < num; i++) {
    tmr64 = *TIMER64HZ;
    strcpy(ibuf1, ultoa(tmr64, ibuf3, RADIX_DEC));
    strcat(ibuf1, " $");
    strcat(ibuf1, ultoa(tmr64, ibuf3, RADIX_HEX));
    strcat(ibuf1, "\n\r");
    puts(ibuf1);
    //if (KBHIT) break;
    if (kbhit()) {
      getc();
      break;
    }
    if (delay > 0) {
      pause_sec(delay);
    }
  }
}

int g_bnum;
/*
 * Select banked RAM bank.
 */
void enhsh_rambanksel(void)
{
  g_bnum = -1;

  if (!EXTRAMDETECTED || !EXTRAMBANKED) {
    enhsh_prnerror(ERROR_NOEXTBRAM);
    return;
  }

  if (strlen(prompt_buf) > 5) {

    g_bnum = atoi(prompt_buf+5);

    if (g_bnum >= 0 && g_bnum < 8) {
      __asm__("lda %v", g_bnum);
      __asm__("jsr %w", MOS_BANKEDRAMSEL);
    } else {
      enhsh_prnerror(ERROR_BANKNUM);
    }
  }

  enhsh_getrambank();
}

/*
 * Get current banked RAM bank #.
 */
void enhsh_getrambank(void)
{
  unsigned char rambank = 0;

  if (!EXTRAMDETECTED || !EXTRAMBANKED) {
    enhsh_prnerror(ERROR_NOEXTBRAM);
    return;
  }

  rambank = *RAMBANKNUM;

  puts("Current RAM bank#: ");
  puts(utoa((unsigned int)rambank, ibuf3, RADIX_DEC));
  puts("\n\r");
}

/*
 * Decimal to hex/bin conversion.
 */
void enhsh_dec2hexbin()
{
  char *hexval = ibuf1;
  char *binval = ibuf2;

  if (strlen(prompt_buf) > 5) {
    strcpy(hexval, ultoa((unsigned long)atol(prompt_buf+5), ibuf3, RADIX_HEX));
    strcpy(binval, ultoa((unsigned long)atol(prompt_buf+5), ibuf3, RADIX_BIN));
    puts("Decimal: ");
    puts(prompt_buf+5);
    puts("\n\r");
    puts("Hex:     ");
    puts(hexval);
    puts("\n\r");
    puts("Binary:  ");
    puts(binval);
    puts("\n\r");
  } else {
    enhsh_prnerror(ERROR_DECVALEXP);
  }
}

int main(void)
{
	enhsh_banner();
	enh_shell();
	return 0;
}
