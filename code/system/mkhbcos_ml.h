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
 * 2018-02-10
 *    Added DEVICESDET register and masing flags for it.
 *    Added macros for RTC detected, extended RAM detected and Banked RAM
 *    detected.
 *
 * 2018-02-19
 *    Added comment for KBHIT macro.
 *    Added Constants section and IO address range definitions.
 *
 * 3/8/2018
 *    Added entries in kernel jump table.
 *
 */

#ifndef MKHBCOS_ML
#define MKHBCOS_ML

// System variables:

#define TIMER64HZ   ((unsigned long *)0x00E2) // pointer to 4-byte counter
#define RAMBANKNUM  ((unsigned char *)0x00E6) // pointer to RAM bank# register
#define DEVICESDET  ((unsigned char *)0x00E7) // pointer to detected devices
                                              // flags
#define UARTRXINPT  ((unsigned char *)0x00F2) // ptr to beg. of UART RX queue
#define UARTRXOUTPT ((unsigned char *)0x00F3) // ptr to end of UART RX queue

// masking flags and their complements

#define DEVPRESENT_RTC      0x80
#define DEVPRESENT_NORTC    0x7F
#define DEVPRESENT_EXTRAM   0x40
#define DEVPRESENT_NOEXTRAM 0xBF
#define DEVPRESENT_BANKRAM  0x20
#define DEVPRESENT_NOBRAM   0xDF
#define DEVPRESENT_UART     0x10
#define DEVPRESENT_NOUART   0xEF

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
#define MOS_PROCRMEM      0xFFE4
#define MOS_PROCWMEM      0xFFE7
#define MOS_PROCEXEC      0xFFEA
#define MOS_GETC          0xFFED
#define MOS_PUTC          0xFFF0
#define MOS_GETS          0xFFF3
#define MOS_PUTS          0xFFF6
#define MOS_MEMINIT       0xFFC9
#define MOS_MEMCPY        0xFFC6
#define MOS_BRAMSEL       0xFFC3
#define MOS_PRNDT         0xFFC0
#define MOS_SETDT         0xFFBD
#define MOS_RTCENPIE      0xFFB7
#define MOS_RTCDISPIE     0xFFBA

/*
 * The addresses below (if any) need to be moved to Kernel Jump Table.
 * Before they are, they need to be changed each time firmware code in
 * system/mkhbcos_fmware.s is altered in such a way that relocates the code.
 * NOTE: These are monitor commands, they require properly formed command
 *       in the PromptLine entered by user at address $80.
 */

// Macros

#define KBHIT       (*UARTRXINPT != *UARTRXOUTPT) // KbHit as macro
/* NOTE:
 * It is recommended to use library function kbhit(), if memory use is of
 * concern. When compiled, the call to function is more compact than above
 * macro expression. The performance may be better with the macro though.
 * (performance claim not verified)
 * 4/5/2018 - This has been discovered yesterday, a bug? a racing
 *            effect? : In program floader.c, when I used kbhit() function,
 *            kbhit() would return incorrect value and program would fail.
 *            I replaced calls to kbhit() with the above macro and the problem
 *            is gone. Something I need to investigate.
 *            STATUS:
 *            I think I know what the problem is. The macro simply compares
 *            pointers to the beginning and end of the input queue.
 *            Returns a boolean result.
 *            However kbhit(), after comparing the pointers, if they're not
 *            equal (meaning queue not empty), returns the value of the
 *            character in the queue.
 *            This will work for character input, however program 'floader'
 *            loads a binary data stream. Therefore zero values are expected
 *            in input. Case closed.
 *            Two solutions:
 *            1) Use KBHIT macro in case of binary input where null values are
 *               expected.
 *              OR (more proper)
 *            2) Modify function kbhit() in the firmware to return a result of
 *               boolean comparison and not a value of the character.
 *               Going with this will require review of all the source code
 *               and correcting all the places where value returned by kbhit()
 *               may be used as an input character value and replace these
 *               cases with getting the actual character with getc() function
 *               if kbhit() returns a non-zero result.
 *
 */
#define RTCDETECTED (*DEVICESDET & DEVPRESENT_RTC) // true if RTC chip was
                                                   // detected by MOS
#define EXTRAMDETECTED (*DEVICESDET & DEVPRESENT_EXTRAM) // true if extended
                                                         // RAM was detected
                                                         // by MOS
#define EXTRAMBANKED   (*DEVICESDET & DEVPRESENT_BANKRAM) // true if extended
                                                          // RAM is banked

#endif

// Constants

// IO address range (inclusive):
#define IO_START    0xC000
#define IO_END      0xC7FF
