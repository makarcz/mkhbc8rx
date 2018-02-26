/*
 *
 * File: 	mkhbcos_ansi.c
 * Purpose:	ANSI terminal functions.
 * Author:	Marek Karcz
 * Created: 01/19/2012
 *
 * NOTE:
 *  GVIM: set tabstop=4 shiftwidth=4 expandtab
 *
 * Revision history:
 *
 * 1/24/2012
 *    Added terminating 0 to code sequences.
 *    Added reset all attributes and blink-on functions.
 *    Added set colors function.
 *    Added bold-on function.
 *    Added reverse-on function.
 * 
 * 2/21/2016
 *    Added cursor home sequence and call it in CLS.
 *
 * 7/26/2016
 *    ansi_set_cursor() and ansi_set_colors().
 */

#include <stdlib.h>
#include <string.h>
#include "mkhbcos_serialio.h"
#include "mkhbcos_ansi.h"

#define TBUF_MAX	20

unsigned char ansicode_scrldown [] 	= {ESC, 'D', 0};
unsigned char ansicode_cls [] 		= {ESC, '[', '2', 'J', 0};
unsigned char ansicode_home [] 		= {ESC, '[', '1', ';', '1', 'H', 0};
unsigned char ansicode_blink [] 	= {ESC, '[', '5', 'm', 0};
unsigned char ansicode_reset [] 	= {ESC, '[', '0', 'm', 0};
unsigned char ansicode_esc1 []		= {ESC, '[', 0};
unsigned char ansicode_bold []		= {ESC, '[', '1', 'm', 0};
unsigned char ansicode_reverse []	= {ESC, '[', '7', 'm', 0};

char termbuf[TBUF_MAX];

void ansi_resetallattr(void)
{
	puts(ansicode_reset);
}

void ansi_scrolldown(int n)
{
	int i;
	for (i = 0; i < n; i++)
		puts(ansicode_scrldown);
}

void ansi_cls(void)
{
	puts(ansicode_cls);
	puts(ansicode_home);
}

void ansi_crsr_home(void)
{
	puts(ansicode_home);
}

void ansi_blinkon(void)
{
	puts(ansicode_blink);
}

void ansi_boldon(void)
{
	puts(ansicode_bold);
}

void ansi_reverseon(void)
{
	puts(ansicode_reverse);
}

void ansi_set_cursor(int col, int row)
{
	memset(termbuf,0,TBUF_MAX);

	puts(ansicode_esc1);
	puts(itoa(row,termbuf,10));
	puts(";");
	memset(termbuf,0,TBUF_MAX);
	puts(itoa(col,termbuf,10));
	puts("f");
}

void ansi_set_colors(int bkg, int fg)
{
	int lcbg = ANSI_COL_BLACK;
	int lcfg = ANSI_COL_GREEN;
	
	memset(termbuf,0,TBUF_MAX);

	puts(ansicode_esc1);
	if (bkg >= 0 && bkg < ANSI_COL_ENDTABLE)
		lcbg = bkg + 40;
	if (fg >= 0 && fg < ANSI_COL_ENDTABLE)
		lcfg = fg + 30;
	puts(itoa(lcbg,termbuf,10));
	puts(";");
	memset(termbuf,0,TBUF_MAX);
	puts(itoa(lcfg,termbuf,10));
	puts("m");
}
