/*
 * File: 	mkhbcos_serialio.h
 * Purpose:	declarations and definitions for serial I/O API.
 * Author:	Marek Karcz 
 * Created:	1/19/2012
 *
 * Revision history:
 *
 * 1/19/2012:
 * 	Initial revision.
 *
 * 2/5/2018
 *  Added kbhit().
 *
 */

#ifndef MKHBCOS_SERIALIO
#define MKHBCOS_SERIALIO

void 					mos_puts(const char *s);
int 	__fastcall__ 	puts(const char *s);
int		__fastcall__  	putchar(int c);
char*	__fastcall__	gets(char *s);
int		__fastcall__	getchar(void);
int		__fastcall__	getc(void);
int		__fastcall__	fgetc(void);
int     __fastcall__    kbhit(void);

#define	ESC	0x1B

#endif
