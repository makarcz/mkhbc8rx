/*
 * File:	date.c
 * Purpose:	Implement program date that shows current date / time.
 * Author:	Marek Karcz
 * Created:	2/12/2018
 *
 * Revision history:
 *
 * 2/12/2018
 * 	Initial revision.
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
//#include <peekpoke.h>
//#include "mkhbcos_lcd.h"
#include "mkhbcos_serialio.h"
//#include "mkhbcos_ansi.h"
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
const int ver_build = 0;

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

void date_version(void);
void date_show(void);
void date_banner(void);

void date_version(void)
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
void date_show(void)
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

void date_banner(void)
{
  while (kbhit()) getc(); // flush RX buffer
  puts("\n\r");
	puts("Show date / time.\n\r");
	puts("(C) Marek Karcz 2018. All rights reserved.\n\r");
  puts("Version: ");
  date_version();
  puts("\n\r");
}

int main(void)
{
	date_banner();
	date_show();
	return 0;
}
