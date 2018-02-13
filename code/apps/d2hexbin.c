/*
 *
 * File:	d2hexbin.c
 * Purpose:	Decimal to hexadecimal / binary conversion tool.
 * Author:	Marek Karcz
 * Created:	2/11/2018
 *
 * Revision history:
 *
 * 2/11/2018
 * 	Initial revision.
 *
 * 2/12/2018
 *  Extended text buffers.
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
#include "mkhbcos_serialio.h"

#define IBUF1_SIZE      40
#define IBUF2_SIZE      60
#define IBUF3_SIZE      40
#define PROMPTBUF_SIZE  40
#define RADIX_DEC       10
#define RADIX_HEX       16
#define RADIX_BIN       2

const int ver_major = 1;
const int ver_minor = 0;
const int ver_build = 5;

char prompt_buf[PROMPTBUF_SIZE];
char ibuf1[IBUF1_SIZE], ibuf2[IBUF2_SIZE], ibuf3[IBUF3_SIZE];

void d2hb_getline(void);
void d2hb_version(void);
void d2hb_banner(void);
void d2hb_dec2hexbin(void);

/* get line of text from input */
void d2hb_getline(void)
{
	memset(prompt_buf,0,PROMPTBUF_SIZE);
	gets(prompt_buf);
	puts("\n\r");
}

void d2hb_version(void)
{
	strcpy(ibuf1, itoa(ver_major, ibuf3, RADIX_DEC));
	strcat(ibuf1, ".");
	strcat(ibuf1, itoa(ver_minor, ibuf3, RADIX_DEC));
	strcat(ibuf1, ".");
	strcat(ibuf1, itoa(ver_build, ibuf3, RADIX_DEC));
	strcat(ibuf1, "\n\r");
	puts(ibuf1);
}

void d2hb_banner(void)
{
  while (kbhit()) getc(); // flush RX buffer
  puts("\n\r");
	puts("Decimal to hexadecimal / binary conversion tool.\n\r");
  puts("Version: ");
  d2hb_version();
	puts("(C) Marek Karcz 2018. All rights reserved.\n\r\n\r");
}

/*
 * Decimal to hex/bin conversion.
 */
void d2hb_dec2hexbin()
{
  char *hexval = ibuf1;
  char *binval = ibuf2;

  strcpy(hexval, ultoa((unsigned long)atol(prompt_buf), ibuf3, RADIX_HEX));
  strcpy(binval, ultoa((unsigned long)atol(prompt_buf), ibuf3, RADIX_BIN));
  puts("Decimal: ");
  puts(prompt_buf);
  puts("\n\r");
  puts("Hex:     ");
  puts(hexval);
  puts("\n\r");
  puts("Binary:  ");
  puts(binval);
  puts("\n\r");
}

int main(void)
{
  char q = 'N';
  memset(prompt_buf,0,PROMPTBUF_SIZE);
	d2hb_banner();
  while (1) {
     puts("Enter decimal number: ");
     d2hb_getline();
	   d2hb_dec2hexbin();
     puts("Again? (Y/N)");
     q = getc();
     if (q != 'Y' && q != 'y') break;
     puts("\n\r");
  }
  puts("\n\r");
	return 0;
}
