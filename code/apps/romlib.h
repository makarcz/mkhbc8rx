/*
 * File:	romlib.h
 * Purpose:	Declarations and definitions for MKHBCOS API located in romlib.
 * Author:	Marek Karcz
 * Created:	2/25/2018
 */
#ifndef ROMLIB_H
#define ROMLIB_H

enum RomLibFuncCodes {
    ROMLIBFUNC_INFO = 0,
    ROMLIBFUNC_SHOWDT,
    ROMLIBFUNC_PUTS,
    ROMLIBFUNC_GETS,
    ROMLIBFUNC_SETDT,
    ROMLIBFUNC_MEMCPY,
    ROMLIBFUNC_MEMDUMP,
    ROMLIBFUNC_MEMINIT,
//--------------------------
    ROMLIBFUNC_UNKNOWN
};

extern uint16_t _LIBARG_START__; //, __LIBARG_SIZE__;
extern uint16_t _IO0_START__;
extern uint16_t _IO7_START__, _IO7_SIZE__;
extern uint16_t _ROMLIB_START__;

#define EXCHRAM         ((unsigned char *)&_LIBARG_START__)
#define ARGPTR          (EXCHRAM+1)
#define RETPTR          (EXCHRAM+3)
#define START_IO        ((unsigned)&_IO0_START__)
#define SIZE_IO         ((unsigned)&_IO7_SIZE__)
#define END_IO          (((unsigned)&_IO7_START__) + SIZE_IO)
#define ROMLIBFUNC      ((unsigned char *)&_ROMLIB_START__)

/*
 * Pointer to be used to call ROM library function.
 * Usage example:
 *    EXCHRAM[0] = ROMLIBFUNC_SHOWDT;
 *    (*romlibfunc)();
 */
void (*romlibfunc)(void) = (void *)ROMLIBFUNC;

#endif
