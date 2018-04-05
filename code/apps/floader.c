/*
 * File:	floader.c
 * Purpose:	Source code of a binary data stream loader.
 * Author:	Marek Karcz
 * Created:	4/4/2018
 *
 * Revision history:
 *
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
//#include "mkhbcos_ansi.h"
#include "mkhbcos_ml.h"
#include "romlib.h"

#define IBUF1_SIZE      20
#define RADIX_DEC       10
#define RADIX_HEX       16
#define RADIX_BIN       2
#define LOAD_MEM        0x0B00

char ibuf1[IBUF1_SIZE];
unsigned long tmr64;
uint16_t start_addr, addr;
//unsigned char b;
int cont;

/////////////////////////// M A I N    F U N C T I O N //////////////////////
int main(void)
{
    cont = 1;
    //puts("Binary data loader 1.0.0 (C) Marek Karcz'2018.\n\r");
    puts("Address to load (dec): ");
    gets(ibuf1);
    start_addr = (uint16_t) atoi(ibuf1);
    addr = start_addr;
    puts("\n\rWaiting for data...\n\r");
    while(!KBHIT);
    puts("Loading...\n\r");
    while(cont) {
        //b = getc();
        puts(utoa(addr, ibuf1, RADIX_HEX));
        puts("\r");
        POKE(addr++, getc());
        //putchar('.');
        //addr++;
        if (RTCDETECTED) {
            tmr64 = *TIMER64HZ + 128;    // 2 seconds timeout
            while(!KBHIT) {
                if (tmr64 < *TIMER64HZ) {
                    cont = 0;
                    break;
                }
            }
        }
    }
    puts("\n\r");
    puts(utoa(addr-start_addr, ibuf1, RADIX_DEC));
    puts(" bytes loaded to $");
    puts(utoa(start_addr, ibuf1, RADIX_HEX));
    puts(" - $");
    puts(utoa(addr, ibuf1, RADIX_HEX));
    puts("\n\r");

    return 0;
}
