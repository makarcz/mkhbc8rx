/*
 * File: 	testansi.c
 * Purpose:	Testing ANSI terminal functions.
 * Author:	Marek Karcz
 * Created: 07/26/2016
 *
 * Revision history:
 *
 * 7/26/2016
 *    Testing ansi_set_cursor() and ansi_set_colors().
 */
#include <stdlib.h>
#include <stdint.h>
#include <string.h>
#include <peekpoke.h>
//#include "mkhbcos_lcd.h"
#include "mkhbcos_serialio.h"
#include "mkhbcos_ansi.h"
#include "mkhbcos_ml.h"
#include "mkhbcos_ds1685.h"

char mybuf[20]; 
struct ds1685_clkdata	clkdata;

/* pause specified # of seconds */
void my_pause(uint16_t delay)
{
	int i = 0;
	unsigned int currsec = 0, nsec = delay;

	ds1685_rdclock (&clkdata);
	while (nsec > 0) {

		currsec = clkdata.seconds;
		/* wait 1 second */
	  while (currsec == clkdata.seconds) {

	 	  for(i=0; i < 200; i++);
	 	  ds1685_rdclock (&clkdata);
	  }	
	  nsec--;		
	}
}

int main(void)
{
	int i;

	ds1685_init(DSC_REGB_SQWE | DSC_REGB_DM | DSC_REGB_24o12, 
				/* Square Wave output enable, binary mode, 24h format */
				DSC_REGA_OSCEN | 0x0A, /* oscillator ON, 64 Hz on SQWE */
				0xA0, /* AUX battery enable, 12.5pF crystal, 
						 E32K=0 - SQWE pin frequency set by RS3..RS0 */
				0x08 /* PWR pin to high-impedance state */);

	memset(mybuf,0,20);
	ansi_set_colors(ANSI_COL_BLACK, ANSI_COL_GREEN);
	ansi_cls();

	for (i=1; i<11; i++) {
		ansi_set_cursor(1, 1);
		itoa(i,mybuf,10);
		puts(mybuf);
		my_pause(2);
	}	

	puts("\n\r");	

	for (i=1; i<11; i++) {
		ansi_set_cursor(i, i);
		puts("Hello!");
	}

	ansi_set_colors(ANSI_COL_BLACK, ANSI_COL_WHITE);
	puts("\n\r");

	return 0;
}