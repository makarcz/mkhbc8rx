/*
 * 
 * File: hello.c
 * 
 * Purpose: Hello World demo program to run under MKHBCOS
 *          (M.O.S. derivative).
 *          Program demonstrates I/O, LCD and ANSI terminal
 *          API of mkhbcos.lib library.
 *          
 * Author: Marek Karcz
 *
 * NOTE:
 *
 * GVIM
 * set tabstop=4 shiftwidth=4 expandtab
 * 
 */

#include <stdlib.h>
#include <stdint.h>
#include <string.h>
#include "mkhbcos_lcd.h"
#include "mkhbcos_serialio.h"
#include "mkhbcos_ansi.h"

char hello[] = "Hello World!\n\r";
char buf[256];
char b2[20];
char s1[20]; //,s2[20],s3[20];
uint16_t n = UINT16_MAX;

void my_puts (char *s)
{
	mos_puts(s);
}

void test_speed_lcd(uint16_t n)
{
	lcd_clear();
	while(n>0)
	{
	    ansi_set_cursor(0,21);
		strcpy(s1, utoa(n,s1,2)); 	// binary base
		puts(s1); putchar(':');
		lcd_puts(s1, LCD_LINE_1);
		strcpy(s1, utoa(n,s1,16));	// hexadecimal base
		puts(s1); putchar(':');
		lcd_puts(s1, LCD_LINE_2);
		strcpy(s1, utoa(n,s1,10));	// decimal base
		puts(s1); puts("     ");
		lcd_puts(":",LCD_LINE_CURRENT);
		lcd_puts(s1, LCD_LINE_CURRENT);
		lcd_puts(" ",LCD_LINE_CURRENT);
		n--;
	}
	ansi_set_cursor(0,21);
	puts("                                \r\n");
}

void my_delay(int n)
{
	int i,j;

	for (i=0; i<n; i++)
	{
		for(j=0; j<100; j++)
			__asm__("nop");
	}
}

void my_reset_scr_attr(void)
{
	ansi_resetallattr();
	ansi_set_colors(ANSI_COL_BLACK, ANSI_COL_GREEN);
}

int main (void)
{
	uint16_t iter = UINT16_MAX;
	int i, n;
	
	my_reset_scr_attr();
	ansi_set_colors(ANSI_COL_BLACK, ANSI_COL_YELLOW);
	ansi_cls();
	puts("Demo will commence in...");
	for(i=35,n=10; n>=0; n--, i-=2)
	{
		ansi_set_cursor(i,2);
		puts(itoa(n,s1,10));
		my_delay(100);
	}
	ansi_set_colors(ANSI_COL_BLACK, ANSI_COL_GREEN);
	ansi_boldon();
	ansi_cls();
	ansi_set_cursor(0,10);
	puts("Serial I/O, LCD and ANSI API demo.\n\r");
	puts("Copyright by Marek Karcz 2012.\n\r");
	puts("All rights reserved.\n\r");
	puts("\n\r");
	ansi_blinkon();
	my_puts(hello);
	my_reset_scr_attr();	
	LCD_INIT;
	lcd_cursorctrl(1,1);	// turn on blinking cursor on LCD
	lcd_puts("   MKHBC-8-R1   ", LCD_LINE_1);
	lcd_puts("  Hello World! ", LCD_LINE_2);
	mos_puts("It sure feels good to code in 'C' for g'old MOS 6502!\n\r");
	ansi_boldon();
	puts("Enter text:");
	my_reset_scr_attr();
	gets(buf);
	putchar('\n');
	putchar('\r');
	puts("You entered:");
	ansi_reverseon();
	puts(buf);
	putchar('\n');
	putchar('\r');
	my_reset_scr_attr();
	ansi_boldon();
	puts("Enter # of iterations [0..65535]:");
	my_reset_scr_attr();
	gets(b2);
	iter = atoi(b2);
	putchar('\n');
	putchar('\r');
	puts("You entered:");
	ansi_reverseon();
	puts(b2);
	putchar('\n');
	putchar('\r');
	my_reset_scr_attr();
	ansi_blinkon();
	puts("Please wait...\n\r");
	my_reset_scr_attr();
	ansi_boldon();
	lcd_cursorctrl(0,0);	// turn off cursor on LCD
	test_speed_lcd(iter);
	puts("Demo finished.");
	putchar('\n');
	putchar('\r');
	lcd_clear();
	lcd_cursorctrl(1,1);	// turn on blinking cursor on LCD
	lcd_puts("   MKHBC-8-R1   ", LCD_LINE_1);
	lcd_puts(buf, LCD_LINE_2);

	return 0;
}
