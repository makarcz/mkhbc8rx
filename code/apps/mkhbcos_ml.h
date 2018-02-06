/*
 * File:	mkhbcos_ml.h
 * Purpose:	Declarations and definitions for MKHBCOS API
 *          (monitor functions).
 * Author:	Marek Karcz
 * Created:	1/23/2012
 *
 * NOTE:
 *  GVIM: set tabstop=2 shiftwidth=2 expandtab
 *
 * Revision history:
 * 
 * 2015-11-29
 *    Updated addresses for new version.
 *
 * 2018-01-20
 *    Updated addresses for new MKHBCOS version.
 *
 * 2018-02-01
 *    Added pointer to periodically updated counter.
 *
 * 2018-02-03
 *    Added remaining entries from MOS jump table.
 *    Type of TIMER64HZ changed from unsigned short to unsigned long.
 *
 * 2018-02-05
 *    Added jump address if MOS_KBHIT.
 *    Added pointers to UART RX queue.
 *    Added KBHIT macro
 *
 */

#ifndef MKHBCOS_ML
#define MKHBCOS_ML

// System variables:

#define TIMER64HZ   ((unsigned long *)0x00E2) // pointer to 4-byte counter
#define RAMBANKNUM  ((unsigned char *)0x00E6) // pointer to RAM bank# register
#define UARTRXINPT  ((unsigned char *)0x00F2) // ptr to beg. or UART RX queue
#define UARTRXOUTPT ((unsigned char *)0x00F3) // ptr to end of UART RX queue
#define KBHIT       (*UARTRXINPT != *UARTRXOUTPT) // KbHit as macro

/* These calls are now in Kernel Jump Table.
 * No more need to change them after firmware code is changed.
 * They stay at the same addresses - guaranteed.
 */
#define MOS_KBHIT         0xFFCC
#define MOS_BANKEDRAMSEL  0xFFCF
#define MOS_DS1685INIT    0xFFD2
#define MOS_DS1685RDCLK   0xFFD5
#define MOS_DS1685SETCLK  0xFFD8
#define MOS_DS1685SETTM   0xFFDB
#define MOS_DS1685WRRAM   0xFFDE
#define MOS_DS1685RDRAM   0xFFE1
#define MOS_PROCRMEM	    0xFFE4
#define MOS_PROCWMEM	    0xFFE7
#define MOS_PROCEXEC	    0xFFEA

#endif

