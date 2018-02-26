/*
 * File: 	mkhbcos_ds1685.h
 * Purpose:	Declarations and definitions for
 *          DS1685 RTC (Real Time Clock) chip API.
 * Author:	Marek Karcz
 * Created:	02/05/2012
 *
 * Revision history:
 *
 *
 * NOTE:
 *    GVIM: set tabstop=4 shiftwidth=4 expandtab
 */

#ifndef MKHBCOS_DS1685
#define MKHBCOS_DS1685

// DS RTC registers mask bits

// reg. A

#define DSC_REGA_UIP	0x80	// %10000000
#define DSC_REGA_DV2	0x40	// %01000000
#define DSC_REGA_DV1	0x20	// %00100000
#define DSC_REGA_DV0	0x10	// %00010000
#define DSC_REGA_RS3	0x08	// %00001000
#define DSC_REGA_RS2	0x04	// %00000100
#define DSC_REGA_RS1	0x02	// %00000010
#define DSC_REGA_RS0	0x01	// %00000001

// aliases

#define DSC_REGA_CTDWN	DSC_REGA_DV2
#define DSC_REGA_OSCEN	DSC_REGA_DV1
#define DSC_REGA_BSEL	DSC_REGA_DV0
#define DSC_REGA_BANK0	0xEF
#define DSC_REGA_BANK1	0x10

// reg. B

#define DSC_REGB_SET	0x80	// %10000000
#define DSC_REGB_PIE	0x40	// %01000000
#define DSC_REGB_AIE	0x20	// %00100000
#define DSC_REGB_UIE	0x10	// %00010000
#define DSC_REGB_SQWE	0x08	// %00001000
#define DSC_REGB_DM		0x04	// %00000100
#define DSC_REGB_24o12	0x02	// %00000010
#define DSC_REGB_DSE	0x01	// %00000001

// aliases

#define DSC_REGB_UNSET	0x7F	// %01111111


struct ds1685_clkdata
{
	unsigned char seconds;
	unsigned char minutes;
	unsigned char hours;
	unsigned char dayofweek;
	unsigned char date; // day
	unsigned char month;
	unsigned char year;
	unsigned char century;
};

unsigned char __fastcall__ ds1685_init (unsigned char regb,
                                        unsigned char rega,
                                        unsigned char regextb,
                                        unsigned char regexta);

struct 	ds1685_clkdata *ds1685_rdclock 	(struct ds1685_clkdata *buf);
void 	ds1685_setclock (struct ds1685_clkdata *buf);
void 	ds1685_settime 	(struct ds1685_clkdata *buf);
/* bank - 0 or 1, addr - $00 - $7f ($0e - $7f for Bank 0), data - 0..255 */
void	ds1685_storeram (unsigned char bank,
		                 unsigned char addr,
				         unsigned char data);			 
unsigned char __fastcall__ ds1685_readram(unsigned char bank,
						                  unsigned char addr);

#endif

