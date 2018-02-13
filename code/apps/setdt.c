/*
 *
 * File:	setdt.c
 * Purpose:	Code of program to set date / time.
 * Author:	Marek Karcz
 * Created:	2/12/2018
 *
 * Revision history:
 *
 * 2/12/2018
 * 	Created.
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
#include "mkhbcos_serialio.h"
#include "mkhbcos_ml.h"
#include "mkhbcos_ds1685.h"

#define IBUF1_SIZE      30
#define IBUF2_SIZE      30
#define IBUF3_SIZE      30
#define RADIX_DEC       10
#define RADIX_HEX       16
#define RADIX_BIN       2

const int ver_major = 1;
const int ver_minor = 0;
const int ver_build = 2;

char ibuf1[IBUF1_SIZE], ibuf2[IBUF2_SIZE], ibuf3[IBUF3_SIZE];

const char daysofweek[8][4] =
{
	"Sun", "Mon", "Tue", "Wed", "Thu", "Fri", "Sat"
};

const char monthnames[13][4] =
{
	"Jan", "Feb", "Mar", "Apr", "May", "Jun", "Jul", "Aug", "Sep", "Oct", "Nov", "Dec"
};

struct ds1685_clkdata	clkdata;

void setdt_version(void);
void setdt_show(void);
void setdt_set(unsigned char setdt);
void setdt_banner(void);

void setdt_version(void)
{
	strcpy(ibuf1, itoa(ver_major, ibuf3, RADIX_DEC));
	strcat(ibuf1, ".");
	strcat(ibuf1, itoa(ver_minor, ibuf3, RADIX_DEC));
	strcat(ibuf1, ".");
	strcat(ibuf1, itoa(ver_build, ibuf3, RADIX_DEC));
	strcat(ibuf1, "\n\r");
	puts(ibuf1);
}

/*
 * Read date/time from DS1685 IC and output to the console.
 */
void setdt_show(void)
{
  if (!RTCDETECTED) {
      puts("ERROR: RTC not detected.\n\r");
      return;
  }

	ds1685_rdclock (&clkdata);

	memset(ibuf1, 0, IBUF1_SIZE);
	memset(ibuf2, 0, IBUF2_SIZE);
	memset(ibuf3, 0, IBUF3_SIZE);
	strcpy(ibuf1, "Date: ");

	if (clkdata.dayofweek > 0 && clkdata.dayofweek < 8)	{

     strcat(ibuf1, daysofweek[clkdata.dayofweek-1]);
	}	else {

     strcat(ibuf1, "???");
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

	strcat(ibuf1, "\r\n");
	puts(ibuf1);

	strcpy(ibuf2, "Time: ");
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

	strcat(ibuf2, "\r\n");
	puts(ibuf2);
}

/*
 *
 * Prompt and set date/time in DS1685 RTC chip.
 * Input: setdt
 * 0 - do not prompt for and set date.
 * 1 - prompt for and set date and time.
 *
 */
void setdt_set(unsigned char setdt)
{
	uint16_t n = 0;

  if (!RTCDETECTED) {
      puts("ERROR: RTC not detected.\n\r");
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

	if (setdt)
		ds1685_setclock (&clkdata);
	else
		ds1685_settime	(&clkdata);
}

void setdt_banner(void)
{
  while (kbhit()) getc(); // flush RX buffer
  puts("\n\r");
	puts("Set date / time.\n\r");
	puts("(C) Marek Karcz 2018. All rights reserved.\n\r");
	puts("Version: ");
  setdt_version();
  puts("\n\r");
}

int main(void)
{
  char q = 's';
  unsigned char opt = 0xFF;
	setdt_banner();
  puts("Set date and time (d), just time (t) OR only show time (s)?: ");
  q = getc();
  switch (q) {
    case 'd':
    case 'D': opt = 1;
              break;
    case 't':
    case 'T': opt = 0;
              break;
    case 's':
    case 'S': opt = 0xFF;
              break;
    default:  opt = 0xFE;
              break;
  }
  puts("\n\r");
  if (opt == 0xFE) {
    puts("ERROR: Invalid selection.\n\r");
  }
  else if (opt != 0xFF) {
	   setdt_set(opt);
  }
  setdt_show();

	return 0;
}
