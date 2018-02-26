/*
 * File:	clock.c
 * Purpose:	Implement program clock that prints current date / time in a loop.
 * Author:	Marek Karcz
 * Created:	2/20/2018
 *
 * Revision history:
 *
 * 2/20/2018
 * 	Initial revision.
 *
 * 2/25/2018
 *  Refactoring due to ROM library introduction.
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
#include "mkhbcos_serialio.h"
#include "mkhbcos_ansi.h"
#include "mkhbcos_ml.h"
#include "romlib.h"

#define IBUF1_SIZE      30
#define IBUF2_SIZE      30
#define IBUF3_SIZE      30
#define RADIX_DEC       10
#define RADIX_HEX       16
#define RADIX_BIN       2

const int ver_major = 1;
const int ver_minor = 1;
const int ver_build = 1;

char ibuf1[IBUF1_SIZE], ibuf2[IBUF2_SIZE], ibuf3[IBUF3_SIZE];

void clock_version(void);
void clock_show(void);
void clock_banner(void);
void pause_sec64(uint16_t delay);

void clock_version(void)
{
    itoa(ver_major, ibuf1, RADIX_DEC);
    strcat(ibuf1, ".");
    strcat(ibuf1, itoa(ver_minor, ibuf3, RADIX_DEC));
    strcat(ibuf1, ".");
    strcat(ibuf1, itoa(ver_build, ibuf3, RADIX_DEC));
    strcat(ibuf1, ".\n\r");
    puts(ibuf1);
}

/*
 * Read date/time from DS1685 IC and output to the console.
 */
void clock_show(void)
{
    EXCHRAM[0] = ROMLIBFUNC_SHOWDT;
    (*romlibfunc)();
}

void clock_banner(void)
{
    pause_sec64(128);
    while (kbhit()) getc(); // flush RX buffer
    puts("\n\r");
    puts("Screen Clock ");
    clock_version();
    puts("(C) Marek Karcz 2018. All rights reserved.\n\r\n\r");
    puts("Press any key while clock is running to exit.\n\r");
    puts("Press any key to continue ...");
    getc();
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

/////////////////////////// M A I N    F U N C T I O N //////////////////////
int main(void)
{
    clock_banner();
    ansi_cls();
    while(1) {
        clock_show();
        ansi_set_cursor(1,1);
        if(kbhit()) break;
        pause_sec64(61);
    }
    ansi_set_cursor(1,3);
    puts("\n\rBye!\n\r");
    while(kbhit()) getc();
    return 0;
}
