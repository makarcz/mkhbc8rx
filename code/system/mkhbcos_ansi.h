/*
 * File: 	mkhbcos_ansi.h
 * Purpose:	Declarations and definitions for
 *          ANSI terminal API.
 * Author:	Marek Karcz
 * Created:	01/19/2012
 *
 * Revision history:
 *
 * 1/24/2012
 *
 *    Added reset all attributes and blink-on functions.
 *    Added set colors function.
 *    Added bold-on function.
 *    Added reverse-on function.
 *
 */

#ifndef MKHBCOS_ANSI
#define MKHBCOS_ANSI

enum ansi_colors
{
	ANSI_COL_BLACK	=	0,
	ANSI_COL_RED,
	ANSI_COL_GREEN,
	ANSI_COL_YELLOW,
	ANSI_COL_BLUE,
	ANSI_COL_MAGENTA,
	ANSI_COL_CYAN,
	ANSI_COL_WHITE,
	ANSI_COL_ENDTABLE
};

void ansi_resetallattr(void);
void ansi_scrolldown(int n);
void ansi_cls(void);
void ansi_crsr_home(void);
void ansi_blinkon(void);
void ansi_set_cursor(int col, int row);
void ansi_set_colors(int bkg, int fg);
void ansi_boldon(void);
void ansi_reverseon(void);

unsigned char ansicode_scrldown [];
unsigned char ansicode_cls [] ;
unsigned char ansicode_blink [];
unsigned char ansicode_reset [];
unsigned char ansicode_esc1 [];
unsigned char ansicode_bold [];
unsigned char ansicode_reverse [];

#endif
