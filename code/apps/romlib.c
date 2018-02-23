/*
 *
 * File: romlib.c
 * Purpose: Entry point for mkhbcrom.lib library functions.
 * Author: (C) Marek Karcz 2018. All rights reserved.
 *
 * Theory of operation:
 *
 *  A chunk of monolithic code in ROM with useful functionality and system
 *  functions. Access to library functions is provided by a single entry point
 *  function (main). Library function is selected by providing it's code
 *  in offset 0 of RAM designated exchange area (EXCHRAM). Arguments are
 *  passed in following the function code memory addresses.
 *
 *   *EXCHRAM - function code
 *   *(EXCHRAM+1), *(EXCHRAM+2) - pointer to arguments
 *   *(EXCHRAM+2), *(EXCHRAM+3) - pointer to return values
 *
 * 2/22/2018
 *  Added copy memory and canonical memory dump.
 *
 */

#include <stdlib.h>
#include <stdint.h>
#include <string.h>
#include <peekpoke.h>
#include "mkhbcos_ml.h"
#include "mkhbcos_serialio.h"
#include "mkhbcos_ansi.h"
#include "mkhbcos_ds1685.h"

#define TXTBUF_SIZE     80
#define IBUF1_SIZE      30
#define IBUF2_SIZE      30
#define IBUF3_SIZE      30
#define RADIX_DEC       10
#define RADIX_HEX       16
#define RADIX_BIN       2
#define EXCHRAM         ((unsigned char *)0x0C00)
#define EXCHSIZE        0xFF

const char *daysofweek[8] =
{
    "Sun", "Mon", "Tue", "Wed", "Thu", "Fri", "Sat"
};

const char *monthnames[13] =
{
    "Jan", "Feb", "Mar",
    "Apr", "May", "Jun",
    "Jul", "Aug", "Sep",
    "Oct", "Nov", "Dec"
};

struct ds1685_clkdata	clkdata;

char txtbuf[TXTBUF_SIZE];
char ibuf1[IBUF1_SIZE], ibuf2[IBUF2_SIZE], ibuf3[IBUF3_SIZE];


void date_show(void);
void date_set(unsigned char setdt);
void memory_dump(uint16_t staddr, uint16_t enaddr);

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

    *(uint16_t *)(EXCHRAM+3) = (uint16_t)&clkdata;
}

/*
 *
 * Prompt and set date/time in DS1685 RTC chip.
 * Input: setdt
 * 0 - do not prompt for and set date.
 * 1 - prompt for and set date and time.
 *
 */
void date_set(unsigned char setdt)
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

/*
 * Canonical memory dump.
 */
void memory_dump(uint16_t staddr, uint16_t enaddr)
{
    int i;
    uint16_t addr = 0x0000;
    unsigned char b = 0x00;

    if (enaddr == 0x0000) {
        enaddr = staddr + 0x0100;
    }

    while (kbhit()) getc(); // flush RX buffer
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
        if (addr >= IO_START && addr <= IO_END) {
            puts ("I/O mapped range\n\r");
            continue;
        }
        for (i=0; i<16; i++)
        {
            b = PEEK(addr+i);
            if (b < 16) putchar('0');
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
        // interrupt from keyboard
        if (kbhit()) {
            if (32 == getc()) { // if SPACE,
                while (!kbhit())  /* wait for any keypress to continue */ ;
                getc();         // consume the key
            } else {            // if not SPACE, break (exit) the loop
                break;
            }
        }
    }
}

int main (void)
{
    char func_code = EXCHRAM[0];    // function code (0-255)
    uint16_t argptr = EXCHRAM[2] * 256 + EXCHRAM[1];
    uint16_t retptr = EXCHRAM[4] * 256 + EXCHRAM[3];

    switch(func_code) {
        case 0: // Function #0 : Print information about library.
            puts("MKHBCROM library 1.2.0.\n\r");
            puts("(C) Marek Karcz 2018. All rights reserved.\n\r");
            break;
        case 1: // Function #1 : Print date / time.
            date_show();
            break;
        case 2: // Function #2 : Print string from address set in
                // EXCHRAM + 1, EXCHRAM + 2.
            puts((const char *)argptr);
            break;
        case 3: // Function #3 : Enter string from keyboard and copy to
                //               address set in EXCHRAM+1, EXCHRAM+2
            gets((char *)argptr);
            *(uint16_t *)(EXCHRAM+3) = argptr;
            break;
        case 4: // Function #4 : Interactive function to set date / time.
            date_set(1);
            date_show();
            break;
        case 5: // Function #5 : Copy memory.
            memmove((void *)PEEKW(argptr),
                    (void *)PEEKW(argptr + 2),
                    PEEKW(argptr+2) - PEEKW(argptr));
            break;
        case 6: // Function #6 : Canonical memory dump.
            memory_dump(PEEKW(argptr), PEEKW(argptr + 2));
            break;
        default:
            puts("ERROR (MKHBCROM Library): Unknown function.\n\r");
            break;
    }

    return 0;
}
