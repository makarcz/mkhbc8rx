
Project:
	MKHBC-8-Rx, homebrew 8-bit MOS-6502 based generic purpose computer.
	Hardware blueprints, OS / runtime library and applications.
	This project is still in development.

Author / Copyright:

   (C) Marek Karcz 2012-2018. All rights reserved.
   Contact: makarcz@yahoo.com

Derived work:

   This project uses some code and hardware designed by other authors.
   Credits:

      Scott Chidester. (Meadow Operating System)
      Paul Fellingham. (Prioritised Interrupts Controller)
      Chris Ward.      (RTC DS1685 circuit, functions, LCD interface)
      Peter Jennings.  (Microchess)
      Bill O'Neill.    (Tiny Basic port)
      T. Pittman       (TB Tiny Adventure Game)

   If I forgotten anybody or violated any copyright, please note that my intent
   was not malicious. Please contact me: makarcz@yahoo.com, I will gladly
   correct any problems.

Documentation and programming reference:

System programming / Operating System of MKHBC-8-Rx is divided into 2 parts:

   - Firmware, which resides in EPROM.
   - System programs, which are loaded to RAM via serial port.

Firmware consists of hexadecimal machine code monitor, hardware drivers code,
MOS-6502 mandatory vectors: Reset, NMI, IRQ and Kernel Jump Table.
Firmware is implemented in MOS 6502 assembly language.
Kernel Jump Table is a series of jump commands that redirect to code performing
certain system functions.
These jump commands are guaranteed to be always at the same memory locations,
so the system programs using them don't have to be rebuilt each time
the implementation of firmware is changed and some internal addresses has moved.
The jump table is always in the same place jumping from the same locations to
the same functions that they should perform, even if the functions themselves
change the location in the EPROM due to code refactoring or relocation of
binary code.
The new entries can be added to the Kernel Jump Table though, so it is
guaranteed to be backwards compatible.
Firmware code is integrated into the CC65 startup routine and together with the
entry program romlib.c, CC65 library and platform specific library code is put
in the library archive 'mkhbcrom.lib'. All this code compiles / links into 8 kB
image 'romlib.BIN' which is then burned into EPROM chip.

Theory of operation:

When computer is started, the reset circuit holds the reset line low for long
enough time for CPU to initialize. Next the CPU performs startup routine which
consists of reading the Reset vector from the EPROM memory (which must be at
fixed address) and executes the routine pointed by that vector.
The routine contains initialization code for OS and then goes into the eternal
loop which sends the output via serial port and expects input on the serial
port. By connecting text serial terminal device configured to be the same
speed as computer's UART speed, it is possible to interface with the computer
to send commands and receive output.

The command UI is very simple and consists of very rudimentary hexadecimal
machine code monitor which allows to read/dump values in computer's memory,
write/modify values in Random Access Memory and execute code at provided
address. Added functions in mkhbcrom library allow to initialize and copy
memory, perform canonical memory dump (hex + ascii), print date / time and
setup date / time if computer is equipped with RTC DS1685 chip.
This UI is rudimentary but sufficient for entering code into computer's RAM
and executing it. As mentioned - additional system library is integrated into
EPROM image. The startup code resides at address $E000.
Library uses $0400 bytes of RAM at address $0400 and $0200 of stack at the end
of RAM. Access to library functions is provided by entry point function at
address $E000.
Before calling / executing that function, function number (function code) must
be put in function code register at address $0A00, pointer (address) to the
arguments of the function (if any) goes in memory locations offset from
function code register by $01 (lo) and $02 (hi) and the pointer (address) to
the return values (if any) is set by the function upon return at offset memory
locations $03 (lo) and $04 (hi). The actual arguments and return values must
be present at these addresses.

This is new code at very early stage of development, so this information is
subject to change in the near future.

The system library function codes, arguments and return values are:

------------------------------------------------------------------------------
Code  Description                     Arguments       Return values
------------------------------------------------------------------------------
0     Print version and copyright
      information about library.      n/a             n/a
------------------------------------------------------------------------------
1     Print date and time.            n/a             Pointer to clock data
                                                      structure ds1685_clkdata
                                                      (see mkhbcos_ds1685.h).
------------------------------------------------------------------------------
2     Print a string.                 Address of the
                                      text.           n/a
------------------------------------------------------------------------------
3     Get a string from input         Address of the  Address of the text
      (keyboard / UART).              text buffer.    buffer (duplicated).
------------------------------------------------------------------------------
4     Interactive function to set                     Pointer to clock data
      date / time.                    n/a             structure ds1685_clkdata
                                                      (see mkhbcos_ds1685.h).
------------------------------------------------------------------------------
5     Copy memory.                    DestAddr,       n/a
                                      SrcAddr,
                                      Size
------------------------------------------------------------------------------
6     Canonical memory dump.          StartAddr,      n/a
      (hex + ASCII)                   EndAddr
------------------------------------------------------------------------------
7     Initialize memory.              StartAddr,      n/a
                                      EndAddr,
                                      Value
------------------------------------------------------------------------------

Setting up all necessary registers before calling ROM library function is
a bit peculiar, but pretty automatic once you learn it.
Important thing to remember is that the value at offset locations $01, $02
from the function code register is a *pointer* to the arguments list, not an
actual argument.
The same goes for return values list pointer at offset $03, $04.
Assembly code example below comes from firmware.
Function to dump memory takes two arguments - start address and end address.
Programmer puts these addresses starting at FuncCodeArgs, just after return
values pointer, then initializes the arguments pointer with address
of FuncCodeArgs:
[...]
; Function codes for romlib and addresses of arg / ret exchange registers.
; The functions below are implemented in romlib.c and require protocol of
; setting up function code and argument pointer (and arguments themselves)
; before calling 'JSR RomLibFunc'.
;
FuncCodeDtTm    =   $01     ; show date / time
FuncCodeSetDtTm =   $04     ; set date / time
FuncCodeMemCpy  =   $05     ; copy memory
FuncCodeMemDump =   $06     ; canonical memory dump
FuncCodeMemInit =   $07     ; initialize memory
FuncCodeReg     =   __LIBARG_START__     ; put function code in this register
FuncCodeArgPtr  =   __LIBARG_START__+1   ; put ptr to arg list in this reg.
FuncCodeRetPtr  =   __LIBARG_START__+3   ; this is where return values ptr
                                         ; is left by the function (if any)
FuncCodeArgs    =   __LIBARG_START__+5   ; actual args list starts here

[...]

MOSReadMemCanonical:

    ; setup function code (canonical memory dump : hex + ascii)
    lda #FuncCodeMemDump
    sta FuncCodeReg
    ; setup arguments pointer to point at FuncCodeArgs
    lda #<FuncCodeArgs
    sta FuncCodeArgPtr
    lda #>FuncCodeArgs
    sta FuncCodeArgPtr+1
    ; copy list of arguments starting at address FuncCodeArgs
    lda ArrayPtr3
    sta FuncCodeArgs
    lda ArrayPtr3+1
    sta FuncCodeArgs+1
    lda ArrayPtr4
    sta FuncCodeArgs+2
    lda ArrayPtr4+1
    sta FuncCodeArgs+3
    ; call rom library function
    jsr RomLibFunc
    rts

Programming API / Kernal Jump Table:

CallDS1685Init

    Address: FFD2
    Input: RegB, RegA, RegXB, RegXA
    Returns: RegC in Acc
    Purpose: Initialize RTC chip.

CallDS1685ReadClock
    Address: FFD5
    Input: n/a
    Returns: Data is returned via hardware stack. Calling subroutine is
             responsible for allocating 8 bytes on stack before calling this
             function. Clock data are stored in following order below the
             subroutine return address: seconds, minutes, hours, dayofweek,
             date, month, year, century. Valid return data on stack only if
             Acc > 0 (Acc = # of bytes on stack). Calling subroutine still
             should de-allocate stack space by calling PLA x 8 after reading
             or discarding returned data.
    Purpose: Read RTC clock data.

CallDS1685SetClock
    Address: FFD8
    Input:   Parameters are passed via hardware stack: seconds, minutes,
             hours, day of week, day of month, month, year, century.
             Calling subroutine is responsible for allocating 8 bytes on stack
             and filling the space with valid input data before calling this
             function. Calling subroutine is also responsible for freeing
             stack space (PLA x 8).
    Returns: n/a
    Purpose: Set date/time in RTC chip.

CallDS1685SetTime
    Address: FFDB
    Input:   Parameters are passed via hardware stack: seconds, minutes, hour.
             Calling subroutine is responsible for allocating space on stack
             and filling it with valid input data before calling this function.
             Calling subroutine is also responsible for freeing stack space
             (PLA x 3).
    Returns: n/a
    Purpose: Set time of RTC chip.

CallDS1685StoreRam
    Address: FFDE
    Input:   BankNum, RamAddr, RamVal
    Returns: n/a
    Purpose: Store a value in non-volatile RTC memory bank.

CallDS1685ReadRam
    Address: FFE1
    Input:   BankNum, RamAddr
    Returns: value in Acc
    Purpose: Read value from non-volatile RTC memory bank.

CallReadMem
    Address: FFE4
    Input:   PromptLine (contains hexadecimal address range)
    Returns: n/a (output)
    Purpose: Machine code monitor function - read memory.

CallWriteMem
    Address: FFE7
    Input:   PromptLine (contains hexadecimal address and values)
    Returns: n/a (memory is modified)
    Purpose: Machine code monitor function - write memory.

CallExecute
    Address: FFEA
    Input:   PromptLine (contains hexadecimal address)
    Returns: n/a (code is executed)
    Purpose: Machine code monitor function - execute code in memory.

CallGetCh
    Address: FFED
    Input:   n/a
    Returns: Character code in Acc
    Purpose: Standard I/O function - get character.

CallPutCh
    Address: FFF0
    Input:   Character code in Acc.
    Returns: n/a (standard output)
    Purpose: Standard I/O function - put/print character.

CallGets
    Address: FFF3
    Input:   n/a (standard input)
    Returns: PromptLine, PromptLen
    Purpose: Standard I/O function - get string.

CallPuts
    Address: FFF6
    Input: StrPtr
    Returns: n/a (standard output)
    Purpose: Standard I/O function - put/print string.

CallBankRamSel
    Address: FFCF
    Input:   Banked RAM bank # in Acc. (0..7)
    Returns: n/a (selects RAM bank, updates shadow register in RAM)
    Purpose: Banked RAM bank selection.

CallKbHit
    Address: FFCC
    Input:   n/a
    Returns: Character in Acc or 0 if buffer empty.
    Purpose: Check if there is character in RX buffer (equivalent of check
             if key was pressed since this is UART I/O).

WARNING:
	Disable interrupts before calling any RTC function:
	SEI
	<call to RTC API>
	CLI

Registers, buffers, memory:

		RTC RAM shadow:
            RegB        = $F6
            RegA        = $F7
            RegXB       = $F8
            RegXA       = $F9
            RegC        = $FA
            Temp        = $FB
            BankNum     = $FC
            RamAddr     = $FD
            RamVal      = $FE

        UART Pointers

            UartRxInPt  = $F2       ; Rx head pointer, for chars placed in buf
            UartRxOutPt = $F3       ; Rx tail pointer, for chars taken from buf

        Uart Queues (after stack)
            UartTxQue   = $200   ; 256 byte output queue
            UartRxQue   = $300   ; 256 byte input queue

        MOS Prompt variables
            PromptLine  = $80    ; Prompt line (entered by user)
            PromptMax   = $50    ; An 80-character line is permitted
                                 ; ($80 to $CF)
            PromptLen   = $D0    ; Location of length variable

        MOS I/O Function variables
            StrPtr      = $E0           ; String pointer for I/O functions

        Other variables:
            Timer64Hz   = $E2  ; 4-byte (32-bit) counter incremented 64 times
                               ; per sec. $E2,$E3,$E4,$E5 (unsigned long,
                               ; little endian)

        RamBankNum  = $E6            ; Current Banked RAM bank#.
            DetectedDevices = $E7    ; Flags indicating devices detected by
                                     ; system during startup.

        Detected devices flags:
            DEVPRESENT_RTC          %10000000
            DEVPRESENT_NORTC        %01111111
            DEVPRESENT_EXTRAM       %01000000
            DEVPRESENT_NOEXTRAM     %10111111
            DEVPRESENT_BANKRAM      %00100000
            DEVPRESENT_NOBRAM       %11011111
            DEVPRESENT_UART         %00010000
            DEVPRESENT_NOUART       %11101111
            DEVNOEXTRAM             DEVPRESENT_NOEXTRAM & DEVPRESENT_NOBRAM

          Customizable jump vectors
          Program loaded and run in RAM can modify these vectors
          to drive custom I/O console hardware and attach/change
          handler to IRQ procedure. Interrupt flag should be
          set before changes are applied and cleared when ready.
          Custom IRQ handler routine should make a jump to standard
          handler at the end. Custom I/O function routine should
          end with RTS.

              StoreAcc  = $11       ; Temporary Accumulator store.
              IrqVect   = $0012     ; Customizable IRQ vector
              GetChVect = $0014     ; Custom GetCh function jump vector
              PutChVect = $0016     ; Custom PutCh function jump vector
              GetsVect  = $0018     ; Custom Gets function jump vector
              PutsVect  = $001a     ; Custom Puts function jump vector

          I/O space / address range:

            $C000 .. $C7FF, 8 pages (8 x 256 bytes):

            Internal (non-buffered) I/O bus:

            $C000 .. $C0FF - slot 0 (RAM bank switching register)
            $C100 .. $C1FF - slot 1 (RTC registers)
            $C200 .. $C2FF - slot 2 (Reserved for Prioritized IRQ Controller)
            $C300 .. $C3FF - slot 3 (Reserved for built in I/O parallel
                                     interface PIA or VIA)

            External (buffered/expansion) I/O bus:

            $C400 .. $C4FF - slot 4 (UART)
            $C500 .. $C5FF - slot 5
            $C600 .. $C6FF - slot 6
            $C700 .. $C7FF - slot 7

          RAM bank switching.

            NOTE: Because RAM bank switching hardware register is write only,
                  we cannot read from it to determine which bank is selected.
                  The purpose of bank# RAM register at $E6 is just that -
                  remembering last selected bank#. Note that just putting the
                  value in the bank number RAM register is not going to switch
                  the memory bank. The actual kernel function must be called
                  or programmer must write the same value into the $C000
                  register.

            Address:                    C000
            Value:                      $00 .. $07
            Banked memory:              $8000 .. $BFFF
            Bank number RAM register:   $E6

        Memory map:

        $0000 - $7FFF: Base RAM, 32 kB. $0000 - $03FF space is used by the
                       system.
        $6000 - $7FFF: Optional Video RAM, 8 kB. (takes away from Base RAM,
                       leaving 24 kB for general purpose)
        $8000 - $BFFF: Banked RAM, 16 kB space x 8 banks = 128 kB.
        $C000 - $C7FF: I/O space, 8 slots x 256 Bytes = 2 kB.
        $C800 - $FFFF: EPROM, 14 kB.


System programs:

	System programs currently consist of:

        floader.c - binary data stream loader, allows to load data into
                    computer's memory from serial port in binary mode.
        texted.c - line text editor, uses banked RAM for 8 independent 16 kB
                   text buffers (files) and features 3 kB clipboard for
                   copy / paste operations and search function.
        d2hexbin.c - conversion tool from decimal to hexadecimal / binary code.
        enhmon.c - enhanced monitor with functions to manipulate memory,
                   including RTC's non-volatile RAM and functions for number
                   conversions.
                   (dec to hex/bin, hex to dec/bin and bin to hex/dec.)
        clock.c - screen clock that runs in a loop.

    Programs written in C (CC65) or CA65 assembly for MKHBC-8-Rx
    computer / MKHBC OS use library written in C and assembly languages which
    implements standard C library (CC65), I/O console and RTC functions and
    are compiled into library archive mkhbcos.lib.

    Corresponding C header files are:

     mkhbcos_ansi.h - ANSI terminal API
     mkhbcos_ds1685.h - DS1685 RTC API
     mkhbcos_lcd.h - standard LCD 16x2 API
     mkhbcos_ml.h - C header with definitions of MKHBCOS API and internals
     mkhbcos_ml.inc - assembly header with definitions for MKHBCOS API and
                      internals
     mkhbcos_serialio.h - serial I/O API
     romlib.h - definitions for ROM library, system functions that can be
                accessed by setting up necessary registers and calling code at
                a single entry point address
