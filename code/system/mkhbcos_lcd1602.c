
/*
 * File: 	mkhbcos_lcd1602.c
 * Purpose:	Implements put string for LCD 16x2 device.
 * Author:	Marek Karcz
 * Created:	1/19/2012
 *
 * Revision history:
 *
 * 	1/25/2012
 *
 * 		Added cursor control.
 */

#include "mkhbcos_lcd.h"

unsigned char lcdinitcursoron 	[] = {0x0E, 0};	// enable cursor
unsigned char lcdinitcursblon 	[] = {0x0F, 0};	// enable curson and turn on blinking
unsigned char lcdinitcursoroff 	[] = {0x0C, 0};	// disable cursor and blinking
unsigned char lcdinitblon		[] = {0x0D, 0};	// enable blinking of char on cursor pos.

/*
 * 
 * Print string on the LCD screen starting
 * at supplied address. If address is 0
 * characters are output to the current address.
 * 
 */
void lcd_puts(char *s, int adr)
{
	if (0 != adr)
		lcd_line(adr);
	while (*s)
	{
		lcd_putchar(*s++);
	}
}

void lcd_cursorctrl(int con, int blon)
{
	if (con && blon)
		lcd_init(lcdinitcursblon);
	else if (con)
		lcd_init(lcdinitcursoron);
	else if (blon)
		lcd_init(lcdinitblon);
	else
		lcd_init(lcdinitcursoroff);
}
