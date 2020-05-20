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
 * 2/23/2018
 *  Corrected argument / return values passing in functions #1,2,3 and 4.
 *  Added function #7 for memory initialization.
 *
 * 2/24/2018
 *  Bug fixes. Refactoring.
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

extern uint16_t _LIBARG_START__; //, __LIBARG_SIZE__;
extern uint16_t _IO0_START__;
extern uint16_t _IO7_START__, _IO7_SIZE__;

#define TXTBUF_SIZE     80
#define IBUF1_SIZE      30
#define IBUF2_SIZE      30
#define IBUF3_SIZE      30
#define RADIX_DEC       10
#define RADIX_HEX       16
#define RADIX_BIN       2
#define EXCHRAM         ((unsigned char *)&_LIBARG_START__)
#define ARGPTR          (EXCHRAM+1)
#define RETPTR          (EXCHRAM+3)
#define START_IO        ((unsigned)&_IO0_START__)
#define SIZE_IO         ((unsigned)&_IO7_SIZE__)
#define END_IO          (((unsigned)&_IO7_START__) + SIZE_IO)

struct ds1685_clkdata	clkdata;

char txtbuf[TXTBUF_SIZE];
char ibuf1[IBUF1_SIZE], ibuf2[IBUF2_SIZE], ibuf3[IBUF3_SIZE];

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

void date_show(void);
void date_set(unsigned char setdt);
void memory_dump(uint16_t staddr, uint16_t enaddr);

/*
 * Read date/time from DS1685 IC and output to the console.
 */
void date_show(void)
{
    if (RTCDETECTED) {

        ds1685_rdclock (&clkdata);

        memset(ibuf1, 0, IBUF1_SIZE);
        memset(ibuf2, 0, IBUF2_SIZE);
        //memset(ibuf3, 0, IBUF3_SIZE);
        strcpy(ibuf1, "Date: ");

        if (clkdata.dayofweek > 0 && clkdata.dayofweek < 8)	{
           strcat(ibuf1, daysofweek[clkdata.dayofweek-1]);
        } else {

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
        POKEW(RETPTR, (uint16_t) &clkdata);
    } else {
        puts("RTC not detected.\n\r");
    }
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

    if (RTCDETECTED) {

        if (setdt)
        {
            puts("Century (19,20): ");
            gets(ibuf1);
            puts("\r\n");
            n = atoi(ibuf1);
            clkdata.century = (unsigned char) n;
            puts("Year (0-99): ");
            gets(ibuf1);
            puts("\r\n");
            n = atoi(ibuf1);
            clkdata.year = (unsigned char) n & 0x7f;
            puts("Month (1-12): ");
            gets(ibuf1);
            puts("\r\n");
            n = atoi(ibuf1);
            clkdata.month = (unsigned char) n & 0x0f;
            puts("Day (1-31): ");
            gets(ibuf1);
            puts("\r\n");
            n = atoi(ibuf1);
            clkdata.date = (unsigned char) n & 0x1f;
            puts("Day of week # (1-7, Sun=1): ");
            gets(ibuf1);
            puts("\r\n");
            n = atoi(ibuf1);
            clkdata.dayofweek = (unsigned char) n & 0x07;
        }
        puts("Hours (0-23): ");
        gets(ibuf1);
        puts("\r\n");
        n = atoi(ibuf1);
        clkdata.hours = (unsigned char) n & 0x1f;
        puts("Minutes (0-59): ");
        gets(ibuf1);
        puts("\r\n");
        n = atoi(ibuf1);
        clkdata.minutes = (unsigned char) n & 0x3f;
        puts("Seconds (0-59): ");
        gets(ibuf1);
        puts("\r\n");
        n = atoi(ibuf1);
        clkdata.seconds = (unsigned char) n & 0x3f;

        if (setdt)
            ds1685_setclock (&clkdata);
        else
            ds1685_settime	(&clkdata);
    } else {
        puts("RTC not detected.\n\r");
    }
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
        if (addr >= START_IO && addr < END_IO) {

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
    unsigned char func_code = EXCHRAM[0];    // function code (0-255)
    uint16_t argptr = PEEKW(ARGPTR); // EXCHRAM[2] * 256 + EXCHRAM[1];
    uint16_t argptrv1 = PEEKW(argptr);
    uint16_t argptrv2 = PEEKW(argptr + 2);
    uint16_t argptrv3 = PEEKW(argptr + 4);
    //uint16_t retptr = EXCHRAM[4] * 256 + EXCHRAM[3];

    switch(func_code) {
        case 0: // Function #0 : Print information about library.
            puts("MKHBCROM library 1.5.0.\n\r"
                 "(C) Marek Karcz 2018, 2020. All rights reserved.\n\r");
            break;
        case 1: // Function #1 : Print date / time.
            date_show();
            break;
        case 2: // Function #2 : Print string from address pointed by argptr
            puts((const char *) argptr);
            break;
        case 3: // Function #3 : Enter string from keyboard and copy to
                //               address pointer in retptr.
            gets((char *) argptr);
            POKEW(RETPTR, argptr);
            break;
        case 4: // Function #4 : Interactive function to set date / time.
            date_set(1);
            date_show();
            break;
        case 5: // Function #5 : Copy memory.
            memmove((void *) argptrv1,
                    (void *) argptrv2,
                    (size_t) argptrv3);
            break;
        case 6: // Function #6 : Canonical memory dump.
            memory_dump(argptrv1, argptrv2);
            break;
        case 7: // Function #7 : Initialize memory with value.
                //               argptr - pointer to start address
                //               argptr + 2 - pointer to end address
                //               argptr + 4 - pointer to value
            memset((void *) argptrv1,
                   (int)    argptrv3,
                   (size_t) (argptrv2 - argptrv1));
            break;
        default:
            puts("ERROR: Unknown function.\n\r");
            break;
    }

    return 0;
}
