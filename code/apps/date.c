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
 * 2/25/2018
 *  Refactoring due to ROM library implementation.
 *  NOTE: Command to show date / time is now a built-in command in firmware.
 *        Therefore this program is obsolete and will be soon removed.
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
    EXCHRAM[0] = ROMLIBFUNC_SHOWDT;
    (*romlibfunc)();
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
