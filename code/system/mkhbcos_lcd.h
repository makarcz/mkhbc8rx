/*
 * File: mkhbcos_lcd.h
 *
 * Purpose: Declarations and definitions for LCD 16x2 API.
 *
 * Author: Marek Karcz
 *
 * Created date: 1/19/2012
 *
 * Revision history:
 * 	
 * 	1/25/2012
 * 		Added cursor control.
 *
 */

#ifndef MKHBCOS_LCD
#define MKHBCOS_LCD

void __fastcall__ lcd_init(unsigned char seq[]);
void __fastcall__ lcd_putchar(char c);
void __fastcall__ lcd_clear(void);
void __fastcall__ lcd_line(unsigned char c);
void lcd_puts(char *s, int adr);
void lcd_cursorctrl(int con, int blon);

unsigned char lcdinitseq		[]; // initialization sequence
unsigned char lcdinitcursoron 	[];	// enable cursor
unsigned char lcdinitcursblon 	[];	// enable curson anr turn on blinking
unsigned char lcdinitcursoroff 	[];	// disable cursor and blinking
unsigned char lcdinitblon		[];	// enable blinking of char on cursor pos.

#define LCD_INIT lcd_init(lcdinitseq)
#define LCD_LINE_1			0x80	//128
#define LCD_LINE_2			0xC0	//192
#define LCD_LINE_CURRENT	0x00	// append at end

#endif
