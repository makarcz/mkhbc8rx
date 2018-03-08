; ---------------------------------------------------------------------------
;
; File:		mkhbcos_fmware.s
; Author:	Marek Karcz
;
; Purpose:
;
;	 Startup code / firmware for MKHBC-8-R2 hobby computer.
;    Compiles under CC65.
;    Includes full code of MKHBCOS, which is an operating system / memory
;    monitor based on M.O.S. - Meadow Operating System, original work by
;    Scott Chidester for his SBC.
;
; Revision history:
;
; 2018-02-21:
;	Initial revision.
;
; 2018-02-22:
;   Added new commands: print date / time, set date / time, canonical
;   memory dump and copy memory.
;
; 2018-02-23
;   Added 'b' command: select / show BRAM bank#.
;   Added 'i' command: memory initialize.
;   Deleted 'd' command (Hello World demo).
;   Trimmed some messages and help text.
;
; 2018-02-24
;   Bug fixes, adjustments of welcome and help messages.
;   Refactoring.
;
; 2/27/2018
;   Minor code formatting.
;
; 3/7/2018
;   Changed location of temporary Accumulator store and custom vectors.
;   Deleted custom vectors for get string and print string functions.
;   (Don't really need them, get character / print character are sufficient.)
;   Header mkhbcos_ml.inc is now included.
;
; 3/8/2018
;   Added new entries to kernel jump table.
;
; ---------------------------------------------------------------------------

.export   _init, _exit
.import   _main

.export   __STARTUP__ : absolute = 1        ; Mark as startup
.import   __RAM_START__,    __RAM_SIZE__    ; Linker generated
.import   __LIBARG_START__, __LIBARG_SIZE__

.import    copydata, zerobss, initlib, donelib

;.include  "zeropage.inc"
.include  "mkhbcos_ml.inc"

; ---------------------------------------------------------------------------
; Place the startup code in a special segment

.segment  "STARTUP"

; This is the entry point of mkhbcrom library.
; Enter the function code and arguments into the RAM exchange area.
; Look in the map file for beginning of STARTUP segment.
; Provide that address to 'x' command (remember to use lower case letters).

RomLibFunc:
_init:

; ---------------------------------------------------------------------------
; Set cc65 argument stack pointer
; Must run, otherwise stack pointer will point to $ffff.

      LDA     #<(__RAM_START__ + __RAM_SIZE__)
      STA     sp
      LDA     #>(__RAM_START__ + __RAM_SIZE__)
      STA     sp+1

; ---------------------------------------------------------------------------
; Initialize memory storage

      JSR     zerobss              ; Clear BSS segment
      JSR     copydata             ; Initialize DATA segment
      JSR     initlib              ; Run constructors

; ---------------------------------------------------------------------------
; Call main()

      JSR     _main

; ---------------------------------------------------------------------------
; Back from main (this is also the _exit entry):  force a software break

_exit:    JSR     donelib              ; Run destructors
          rts

; Here starts the firmware source code.
; Uncomment statement below to add compilation of extra debugging messages.
;Debug       = 1

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

; MKHBC-8-R1 OS Version number
VerMaj      = 1
VerMin      = 8
VerMnt      = 0

; 6502 CPU

; 62256 Static Random Access Memory (SRAM)

RamBase     = $0000
RamEnd      = RamBase+$7FFF 	;with no VRAM option

IO0 =   __IO0_START__   ;IOSTART   ; $C000 - $C0FF
IO1 =   __IO1_START__   ;IO0+256   ; $C100 - $C1FF
IO2 =   __IO2_START__   ;IO1+256   ; $C200 - $C2FF
IO3 =   __IO3_START__   ;IO2+256   ; $C300 - $C3FF
IO4 =   __IO4_START__   ;IO3+256   ; $C400 - $C4FF
IO5 =   __IO5_START__   ;IO4+256   ; $C500 - $C5FF
IO6 =   __IO6_START__   ;IO5+256   ; $C600 - $C6FF
IO7 =   __IO7_START__   ;IO6+256   ; $C700 - $C7FF

; 6850 Asynchronous Communications Interface Adapter (ACIA)

; Register addresses
UartBase    = IO4           ; NOTE: having this configured automatically would
                            ;       be ideal (plug&play), since the serial
                            ;       interface goes in the expansion slot, but
                            ;       for now it is hardcoded and mandatory to
                            ;       have UART chip at IO4 slot.
UartCt      = UartBase+$0   ; Control register
UartSt      = UartBase+$0   ; Status register
UartTx      = UartBase+$1   ; Transmit buffer register
UartRx      = UartBase+$1   ; Receive buffer register

; UART control register bit fields
UART_RXI_EN = %10000000     ; Receive interrupt enable
UART_TXI_EN = %00100000     ; Transmit interrupt enable, /rts low
UART_TXI_DS = %10011111     ; Transmit interrupt disable, /rts low (AND)
UART_N_8_1  = %00010100     ; No parity, 8 bit data, 1 stop (see docs)
UART_DIV_16 = %00000001     ; Divide tx & rx clock by 16, sample middle
UART_RESET  = %00000011     ; Master reset

; UART status register bit fields
UART_RDRF   = %00000001     ; Receive Data Register Full
UART_TDRE   = %00000010     ; Transmit Data Register Empty
UART_DCD    = %00000100     ; Data Carrier Detect line
UART_CTS    = %00001000     ; Clear To Send line
UART_ER_F   = %00010000     ; Error- Framing
UART_ER_O   = %00100000     ; Error- Overrun
UART_ER_P   = %01000000     ; Error- Parity
UART_IRQ    = %10000000     ; Interrupt Request

; Uart Queues (after stack)
UartTxQue   = $200          ; 256 byte output queue
UartRxQue   = $300          ; 256 byte input queue

; MOS Prompt variables
PromptLine  = mos_PromptLine ; Prompt line (entered by user)
PromptMax   = $50            ; An 80-character line is permitted ($80 to $CF)
PromptLen   = mos_PromptLen  ; Location of length variable

; MOS internal/general variables
Error       = $D1           ; Stores error code from some subroutines
ArrayPtr1   = $D2           ; Two-byte generic array pointer 1
ArrayPtr2   = $D4           ; Two-byte generic array pointer 2
ArrayPtr3   = $D6           ; Two-byte generic array pointer 3
ArrayPtr4   = $D8           ; Two-byte generic array pointer 4
Cnt1        = $DA           ; Generic counter 1
Cnt2        = $DB           ; Generic counter 2

; MOS ISR variables
PCPtr       = $DC           ; Two-byte pointer reserved for the ISR
StackDumpV  = $DE           ; Flag for a valid stack dump (only allows one)
StackDump   = $DF           ; Storage for stack dump pointer on interrupt

; MOS I/O Function variables
StrPtr      = mos_StrPtr    ; String pointer for I/O functions (2 bytes)

; MOS Debug variables

; MOS Uart variables (NOTE: some are defined in mkhbcos_ml.inc)
UartTxInPt  = $F0           ; Tx head pointer, for chars placed in buf
UartTxOutPt = $F1           ; Tx tail pointer, for chars taken from buf
;UartRxInPt  = $F2           ; Rx head pointer, for chars placed in buf
;UartRxOutPt = $F3           ; Rx tail pointer, for chars taken from buf
UartCtRam   = $F4           ; Control register in RAM
UartStRam   = $F5           ; Status register in RAM

; MOS DS1685 (RTC) registers and variables
; (NOTE: most are defined in header mkhbcos_ml.inc)

; reg. C
DSC_REGC_IQRF   =   %10000000
DSC_REGC_PF     =   %01000000
DSC_REGC_AF     =   %00100000
DSC_REGC_UF     =   %00010000

; MOS Banked memory bank switching register and shadow reg. in RAM.
; Write only, value $00 .. $07 (bank#).
; Selects RAM Bank in address range $8000 .. $BFFF.

RamBankSwitch   = IO0
;RamBankNum      = $E6  ; defined in mkhbcos_ml.inc

; MOS Customizable jump vectors
; Program can modify these vectors
; to drive custom I/O console hardware and attach/change
; handler to IRQ procedure. Interrupt flag should be
; set before changes are applied and cleared when ready.
; Custom IRQ handler routine should end with a jump to standard
; handler. Custom I/O function routine should end with RTS.

StoreAcc    =   $FF         ; Temporary Accumulator store.
IrqVect     =   $00E8       ; Customizable IRQ vector
GetChVect   =   $00EA       ; Custom GetCh function jump vector
PutChVect   =   $00EC       ; Custom PutCh function jump vector

; MOS device detection flags are defined in mkhbcos_ml.inc

DetectedDevices     =   DetectedDev

; Header
TxtHeader:
	.BYTE	"MKHBC-8-R2, MOS 6502 system "
.ifdef Debug
    .BYTE   $30+VerMaj,'.',$30+VerMin,'.',$30+VerMnt,".DBG"
.else
    .BYTE   $30+VerMaj,'.',$30+VerMin,'.',$30+VerMnt
.endif
    .BYTE   ", motherboard variant.",                           $0d, $0a
    .BYTE   "(C) 2012-2018 Marek Karcz.",                       $0d, $0a
    .BYTE   "6502 SBC Meadow Operating System 1.02",            $0D, $0A
    .BYTE   "(C) 1993-2002 Scott Chidester.",                   $0d, $0a
    .BYTE   "All rights reserved.",                             $0d, $0a
    .BYTE   $0d, $0a
    .BYTE   "Type 'h' for help."

TxtDoubleLineFeed:
    .BYTE   $0D,$0A,$0D,$0A,0

    ; This causes an attempt to "type" the binary file in MS-DOS to stop here.
    .BYTE   $1A

    ; Command Line Prompt
TxtPrompt:
    .BYTE   "mos>",0

    ; Help
TxtHelp:
    .BYTE   $0D,$0A
    .BYTE   " w <adr> <dat> [dat] ... Write data to address",$0D,$0A
    .BYTE   " r <adr>[-<adr>] [+]     Read address range "
    .BYTE   "(+ - ascii dump)",$0D,$0A
    .BYTE   " m <dst> <src> <size>    Copy memory",$0D,$0A
    .BYTE   " i <adr>-<adr> <dat>     Initialize memory",$0D,$0A
    .BYTE   " b [00..07]              Show / select memory bank.",$0D,$0A
    .BYTE   " x <adr>                 Execute at address",$0D,$0A,$0D,$0A
    .BYTE   " c   Continue from NMI event",$0D,$0A
    .BYTE   " t   Print date / time",$0D,$0A
    .BYTE   " s   Set date / time",$0D,$0A,$0D,$0A
    .BYTE   "All values hex, adr, dst, src, size are 4 characters and dat"
    .BYTE   $0D,$0A
    .BYTE   "is 2, separated by exactly one space; parameters are <required>"
    .BYTE   $0D,$0A
    .BYTE   "or [optional]. Use lower case.", $0D,$0A,$0D,$0A,0

TxtNoStack:
    .BYTE   "Can''t continue; need a valid stack frame from NMI.",$0D,$0A,0

TxtNoCmd:
TxtNoFmt:
    .BYTE   "Command ERROR. Enter h for help.",$0D,$0A,0

TxtNmiEvent:
    .BYTE   $0D,$0A,"NMI event.",$0D,$0A,0

TxtRTCDevice:
    .BYTE   "RTC DS-1685 ",0

TxtExtRAM:
    .BYTE   "BRAM $8000-$BFFF ",0

TxtFound:
    .BYTE   "found.",$0D,$0A,0

TxtNotFound:
    .BYTE   "not found.",$0D,$0A,0

TxtBanked:
    .BYTE   "is banked.",$0D,$0A,0

TxtNotBanked:
    .BYTE   "is not banked.",$0D,$0A,0

.ifdef Debug

TxtDetectBRAM:
    .BYTE   "Detecting extended and banked RAM ...",$0D,$0A,0

TxtDetectRTC:
    .BYTE   "Detecting RTC chip ...",$0D,$0A,0

TxtInitComplete:
    .BYTE   "Initialization completed. Enabling interrupts ...",$0D,$0A,0

TxtDeviceDetected:
    .BYTE   "Device detected.",$0D,$0A,0

TxtDeviceNotDetected:
    .BYTE   "Device not detected.",$0D,$0A,0

TxtDetectUart:
    .BYTE   "Detecting UART chip ...",$0D,$0A,0

.endif

TxtRegReport:
    .BYTE   "PC:",0," SR:",0," A:",0," X:",0," Y:",0," SP:",0

TxtPFlags:
    .BYTE   "nv-bdizc"

;-------------------------------------------------------------------------------
; MOS Command prompt intrinsic command tables
;-------------------------------------------------------------------------------

MOSCmdTxt:
    .BYTE   'h'
    .BYTE   'w'
    .BYTE   'r'
    .BYTE   'x'
    .BYTE   'c'
    .BYTE   't'
    .BYTE   's'
    .BYTE   'm'
    .BYTE   'b'
    .BYTE   'i'

MOSCmdLoc:
    .WORD   MOSHelp
    .WORD   MOSWriteMem
    .WORD   MOSReadMem
    .WORD   MOSExecute
    .WORD   MOSContinue
    .WORD   MOSPrintDtTm
    .WORD   MOSSetDtTm
    .WORD   MOSMemCpy
    .WORD   MOSRamBank
    .WORD   MOSMemInit

; Number of commands
MOSCmdNum   =   $0a

NMIJUMP:

	jmp	NMIPROC

RESJUMP:

	jmp RESPROC

IRQJUMP:

	jmp (IrqVect)
	;jmp IRQPROC

;-----------------------------------------------------------------------------
;-----------------------------------------------------------------------------
; Non Maskable Interrupt Vector Entry Point and Interrupt Service Routines
; (ISR)
;-----------------------------------------------------------------------------
;-----------------------------------------------------------------------------

NMIPROC:

    ; This routine (caused by the NMI button press) will initialize the UART
    ; and print out all registers, then transfer control to the monitor prompt.

    cld                     ; Clear decimal

; Push 6502 registers
    pha
    txa
    pha
    tya
    pha

; Reset the 6850 and buffers
    jsr Init6850
    jsr InitUARTISR

; Go to the NMI handler
    jmp NmiHandler

;-------------------------------------------------------------------------------
; NMI handler.  This routine is a jump-point (not subroutine) called in response
; to an NMI event.  Theoretically the system is now in an ISR state, but MOS
; will cancel that state so the monitor prompt can be run with the IRQ-driven
; UART routines again. The interrupted program's status is on the stack at a
; determinable location, so the monitor can recover it.  Otherwise the user can
; move on to other things, and the six- byte stack frame may eventually be
; overwritten since the stack is circular. So this routine just displays a
; message, dumps registers, and proceeds to the monitor prompt.
;-------------------------------------------------------------------------------

NmiHandler:
    tsx                     ; Save stack position for dump and continue
    stx StackDump
    lda #1                  ; Set valid stack dump flag
    sta StackDumpV

    ; Clear interrupt state
    jsr NmiCancel

    ; Display NMI message
    lda #<TxtNmiEvent
    sta StrPtr
    lda #>TxtNmiEvent
    sta StrPtr+1
    jsr Puts

    ; Do register report and goto prompt
    jsr RegReport
    jmp MOSPromptLoop

    ; NMI causes an NMI condition that can only be reset by RTI and not simply
    ; by clearing the I flag (I think).  That's why this needs to be a
    ; subroutine.
NmiCancel:
    tsx                     ; First increment PC to return to addr+1 on RTI
    inx
    inc $100,x
    bne NmiCancelNoRoll
    inx                     ; Handle rollover of PC-low
    inc $100,x
NmiCancelNoRoll:
    lda #0                  ; Clear flags
    pha                     ; Push flags so they'll be pulled by RTI
	rti

;-------------------------------------------------------------------------------
;-------------------------------------------------------------------------------
; Interrupt Request Vector Entry Point and Interrupt Service Routines (ISR)
;-------------------------------------------------------------------------------
;-------------------------------------------------------------------------------

IRQPROC:
    cld                     ; Clear decimal

; Check if software (BRK) is the source of interrupt.

	sta StoreAcc			; store acc. temporarily
	pla						; load status register
	pha						; restore onto stack
	and #$10				; isolate B flag (1 - BRK is the source)
	beq	SrcIrqLine			; result is 0, IRQ line is the source

; Serve BRK (software) interrupt.

	lda StoreAcc			; restore acc. to be preserved on stack
	jmp NMIPROC				; return control to M.O.S.

; IRQ - hardware interrupt service routine
SrcIrqLine:

	lda StoreAcc			; restore acc. to be preserved on stack

; Push 6502 registers
    pha
    txa
    pha
    tya
    pha

; check the source of the interrupt
; NOTE: This is a variant with no hardware IRQ controller.
;       If HW IRQ controller is implemented, the routine
;       checking for IRQ source will change (simplify) significantly
; NOTE: Currently UART 6850 and RTC DS-1685 are the only IRQ sources.

    lda #DEVPRESENT_RTC
    bit DetectedDevices
    beq IRQRTCNotPresent    ; RTC chip was not detected
    lda #$0C                ; Get status from DS1685 (RTC)
    jsr RdRTC               ; Read RegC to clear IRQ flags and for later check
    sta RegC                ; store in RAM
IRQRTCNotPresent:
    lda #DEVPRESENT_UART
    bit DetectedDevices
    beq IrqChkNxt01         ; UART chip not detected
    lda UartSt              ; Get status from 6850
    sta UartStRam           ; Store in MOS variable in RAM
    and #UART_IRQ           ; check the IRQ bit in status reg.
    beq IrqChkNxt01         ; bit not set, check next source
    jmp Irq6850             ; bit set, jump to service 6850 interrupt

IrqChkNxt01:

; here goes the code checking status bits from other possible
; sources of interrupts, e.g.: you can follow the code template below
; for implementation of such checking

;   lda <irq-source-status-register>
;   sta <irq-source-status-ram-storage> ; optional
;   and #<irq-source-irq-bit-flag>
;   beq IrqChkNxt02         ; bit not set, check next
;   jmp <irq-source-service-routine-addr>   ; bit set, jump to service
;
;IrqChkNxt02:
;
;   [... and so on ...]

    lda #DEVPRESENT_RTC
    bit DetectedDevices
    beq IrqChkNxt02         ; RTC chip is not present
    ; check RTC (DS1685) status (stored in RegC)
    lda RegC
    and #DSC_REGC_IQRF      ; check interrupt request flag
    beq IrqChkNxt02         ; not set, check next source
    lda RegC
    and #DSC_REGC_PF        ; check periodic interrupt flag
    beq IrqChkNxt02         ; not set, check next source
    ; DS1685 interrupt service routine
    ; For now it is just incrementing a counter.
    inc Timer64Hz
    bne IrqChkNxt02
    inc Timer64Hz+1
    bne IrqChkNxt02
    inc Timer64Hz+2
    bne IrqChkNxt02
    inc Timer64Hz+3

IrqChkNxt02:

; end of checking, if none of the devices were IRQ source, go to IrqDone
; NOTE: also, after each service routine, function should jump to IrqDone

    jmp IrqDone

; 6850 Interrupt Service Routine
Irq6850:

    lda #UART_RDRF
    bit UartStRam           ; Check receiver full flag
    beq CheckXmt            ; Branch to next check if flag not set
    jsr UartReceive         ; Receive character into buffer
CheckXmt:
    lda #UART_TDRE
    bit UartStRam           ; Check transmitter empty flag
    beq CheckError          ; Branch to next check if flag not set
    jsr UartTransmit        ; Handle transmitter
CheckError:
    ; todo--check 6850 errors and/or other conditions

; Note: because the /IRQ line is pulled high by a 3.3k resistor, it may not make
; it high within the same cycle that the /IRQ source was serviced. It might even
; persist until after the rti instruction below.  In this case a false interrupt
; will occur, and it is important that this ISR allow the case to fall through.

; Irq Service Done--pull registers from stack and return
IrqDone:
    pla
    tay
    pla
    tax
    pla
	rti

;-------------------------------------------------------------------------------
;-------------------------------------------------------------------------------
; Reset/Power Up Vector Entry Point.
;-------------------------------------------------------------------------------
;-------------------------------------------------------------------------------

RESPROC:

	cld
    jmp Init6502

Init6502RetPt:

    jmp Init62256

Init62256RetPt:

    jsr InitMOS             ; Note, initialize MOS *before* the 6850!
    lda #DEVPRESENT_UART
    sta DetectedDevices     ; for now UART is mandatory, assume it is present
    jsr Init6850            ; This sets some vital MOS variables.
    jsr InitUARTISR
    jsr InitBankedRam       ; Initialize banked RAM registers.
.ifdef Debug
    jsr DetectBRAMMsg
.endif
    jsr DetectExtRAM
.ifdef Debug
    lda #DEVPRESENT_BANKRAM
    bit DetectedDevices
    beq PrnDevNotDetected
    jsr DeviceDetectedMsg
    jmp Cont001
PrnDevNotDetected:
    jsr DeviceNotDetectedMsg
Cont001:
.endif
    jsr InitBankedRam   ; Re-initialize banked RAM registers
                        ; (DetectExtRAM switches bank)
    jsr InitDS1685      ; Initialize RTC (it also detects RTC chip)

	; Initialize custom IRQ and I/O jump vectors.

	lda #<IRQPROC		; initialize IRQ handler jump vector
	sta IrqVect
	lda #>IRQPROC
	sta IrqVect+1
	lda #<RomGetCh		; initialize GetCh function jump vector
	sta GetChVect
	lda #>RomGetCh
	sta GetChVect+1
	lda #<RomPutCh		; initialize PutCh function jump vector
	sta PutChVect
	lda #>RomPutCh
	sta PutChVect+1
	;lda #<RomGets		; initialize Gets function jump vector
	;sta GetsVect
	;lda #>RomGets
	;sta GetsVect+1
	;lda #<RomPuts		; initialize Puts function jump vector
	;sta PutsVect
	;lda #>RomPuts
	;sta PutsVect+1
    ; NOP-s below replace commented code above.
    ; I want to keep the entry point addresses in the same place.
    nop
    nop
    nop
    nop
    nop
    nop
    nop
    nop
    nop
    nop
    nop
    nop
    nop
    nop
    nop
    nop

.ifdef Debug
    ; this is fake, just checking if UART present flag did not get reset
    jsr DetectUartMsg
    lda #DEVPRESENT_UART
    bit DetectedDevices
    bne Cont002
    jsr DeviceNotDetectedMsg
    jmp InitCompleted
Cont002:
    jsr DeviceDetectedMsg
    ; Done with init; enable interrupts and goto prompt
InitCompleted:
    jsr InitCompleteMsg
.endif
    cli
    jsr ReportDetectDevices ; produce boot time report of
                            ; detected / undetected hardware
    lda #<TxtDoubleLineFeed
    sta StrPtr
    lda #>TxtDoubleLineFeed
    sta StrPtr+1
    jsr Puts
    jmp MOSPrompt

;-----------------------------------------------------------------------------
; Produce report of detected or undetected hardware.
;-----------------------------------------------------------------------------
ReportDetectDevices:
    lda #<TxtRTCDevice
    sta StrPtr
    lda #>TxtRTCDevice
    sta StrPtr+1
    jsr Puts
    lda #DEVPRESENT_RTC
    bit DetectedDevices
    bne ReportRTCFound
    ; RTC found bit not set.
    lda #<TxtNotFound
    sta StrPtr
    lda #>TxtNotFound
    sta StrPtr+1
    jsr Puts
    jmp ReportCheckExtRam
ReportRTCFound:
    lda #<TxtFound
    sta StrPtr
    lda #>TxtFound
    sta StrPtr+1
    jsr Puts
ReportCheckExtRam:
    lda #<TxtExtRAM
    sta StrPtr
    lda #>TxtExtRAM
    sta StrPtr+1
    jsr Puts
    lda #DEVPRESENT_EXTRAM
    bit DetectedDevices
    beq ReportExtRamNotFound
    ; Extended RAM found.
    lda #<TxtFound
    sta StrPtr
    lda #>TxtFound
    sta StrPtr+1
    jsr Puts
    lda #<TxtExtRAM
    sta StrPtr
    lda #>TxtExtRAM
    sta StrPtr+1
    jsr Puts
    lda #DEVPRESENT_BANKRAM
    bit DetectedDevices
    bne ReportExtRAMBanked
    ; Extended RAM is not banked.
    lda #<TxtNotBanked
    sta StrPtr
    lda #>TxtNotBanked
    sta StrPtr+1
    jsr Puts
    rts
ReportExtRamNotFound:
    lda #<TxtNotFound
    sta StrPtr
    lda #>TxtNotFound
    sta StrPtr+1
    jsr Puts
    rts
ReportExtRAMBanked:
    lda #<TxtBanked
    sta StrPtr
    lda #>TxtBanked
    sta StrPtr+1
    jsr Puts
    rts

;-----------------------------------------------------------------------------
; Detect extended RAM in $8000 - $BFFF range. Check if banked.
;-----------------------------------------------------------------------------
DetectExtRAM:
    ; check if write / read possible in select addresses in extended RAM range
    lda #$00            ; switch to bank #0 (we don't know yet if banking is
                        ; possible but that's fine, we check the bank
                        ; switching later on)
    jsr BankedRamSel
    lda #$01
    sta $8000
    lda $8000
    cmp #$01
    bne DER01           ; write / read test failed
    lda #$02
    sta $90ff
    lda $90ff
    cmp #$02
    bne DER01           ; write / read test failed
    lda #$03
    sta $bfff
    lda $bfff
    cmp #$03
    bne DER01           ; write / read test failed
    lda DetectedDevices     ; write / read test succeeded, set extended RAM
                            ; device bit
    ora #DEVPRESENT_EXTRAM
    sta DetectedDevices
    ; -- check if banked --
    ; Switch to bank #1 and see if there are the same values present at
    ; addresses examined in previous step. If any values are different,
    ; check write / read operation to confirm. If all values are the same,
    ; there is no banking.
    lda #$01
    jsr BankedRamSel
    lda $8000
    cmp #$01
    bne DER02
    lda $90ff
    cmp #$02
    bne DER02
    lda $bfff
    cmp #$03
    bne DER02
    ; all values seem to be the same as in bank #0, so there is no banking
DER01:
    rts
DER02:
    ; some values in bank #1 are different than in bank #0
    ; now we need to confirm that banking indeed works
    ; we are still in bank #1 at this point
    ; perform write / read test
    lda #$11
    sta $8000
    lda $8000
    cmp #$11
    bne DER03   ; something is not right, write / read test did not pass
    lda #$00    ; write / read test passed, switch to bank #0
    jsr BankedRamSel
    lda $8000   ; examine previously written value
    cmp #$01
    bne DER03   ; something is not right, value in bank #0 written in previous
                ; test step was not preserved
    ; test succeeded, extended RAM is banked
    lda DetectedDevices
    ora #DEVPRESENT_BANKRAM
    sta DetectedDevices
    rts
; tests failed, clear extended RAM and banked RAM devices bits
DER03:
    lda DetectedDevices
    and #DEVNOEXTRAM
    sta DetectedDevices
    rts

;-----------------------------------------------------------------------------
; 6502 MPU initialization
;-----------------------------------------------------------------------------

; Note: Can't be a subroutine because stack pointer is cleared.

Init6502:
    ldx #$FF                ; Reset stack pointer to top of page
    txs
    lda #0                  ; Clear A, X, and Y
    tax
    tay
    pha
    plp                     ; Clear all flags, including Decimal
    sei                     ; Disable interrupts
    jmp Init6502RetPt

;-----------------------------------------------------------------------------
; 62256 RAM Initialization
;-----------------------------------------------------------------------------

; Note: Can't be a subroutine because stack is cleared. Also this will appear
; to clear its own indirect pointer while clearing the zero page, but that's
; OK because the pointer will contain zero anyway to point to zero page

Init62256:
    lda #>RamEnd            ; Point to last page in RAM
    sta $1
    lda #$00
    sta $0
    tay                     ; Initialize Y to first byte (A still zero)
Init62256Loop:
    sta ($0),y              ; Clear top half of zero page
    dey
    bne Init62256Loop
Init62256NextPage:           ; Adjust page pointer at end
    dec $1
    bpl Init62256Loop        ; BPL works for RAM less than 32K; clears page
                             ; zero
    sta $1                   ; Clear the pointer
    jmp Init62256RetPt

;-----------------------------------------------------------------------------
; 6850 ACIA (UART) Initialization
;-----------------------------------------------------------------------------
Init6850:
UART_SET    = UART_N_8_1|UART_DIV_16
    lda #UART_RESET         ; Reset ACIA
    sta UartCtRam           ; Always save a copy of the register in RAM
    sta UartCt              ; Set RAM copy first because hardware generates
                            ; IRQs
    lda #UART_SET           ; Set N-8-1, Div 16 (overwrite old control byte)
    sta UartCtRam
    sta UartCt
    jsr Init6850Msg         ; This displays a message in polled form, for
                            ; debug
    lda UartCtRam           ; Always load the control byte image from RAM
    ora #UART_RXI_EN        ; Also enable Rx interrupt (no Tx IRQ at this
                            ; time)
    sta UartCtRam
    sta UartCt
    rts

TxtInit6850:
.BYTE   $0D,$0A,"6850 Init.",$0D,$0A,0

    ; This will be used for development, and can be removed afterward. It
    ; displays a short message using polling.  In case the first revision of
    ; my hardware and/or software doesn't display anything, this will at least
    ; help to narrow the problem down to either an interrupt issue, or an
    ; address/data/control line issue.
Init6850Msg:
    ldx #0
Init6850MsgLoop:
    lda UartSt              ; Get UART status
    sta UartStRam
    and #UART_TDRE          ; Check transmit data register empty bit
    beq Init6850MsgLoop     ; Loop if not empty

    lda TxtInit6850,x       ; Get character
    beq Init6850MsgDone     ; If it's zero, we're done
    sta UartTx              ; Transmit
    inx
    jmp Init6850MsgLoop     ; Loop back to wait for empty buffer

Init6850MsgDone:
    rts

.ifdef Debug
; Using polling display message of detecting RTC chip.
DetectRTCMsg:
    ldx #0
DetectRTCMsgLoop:
    lda UartSt              ; Get UART status
    sta UartStRam
    and #UART_TDRE          ; Check transmit data register empty bit
    beq DetectRTCMsgLoop     ; Loop if not empty

    lda TxtDetectRTC,x       ; Get character
    beq DetectRTCMsgDone     ; If it's zero, we're done
    sta UartTx              ; Transmit
    inx
    jmp DetectRTCMsgLoop     ; Loop back to wait for empty buffer

DetectRTCMsgDone:
    rts

; Using polling display message of detecting banked RAM.
DetectBRAMMsg:
    ldx #0
DetectBRAMMsgLoop:
    lda UartSt              ; Get UART status
    sta UartStRam
    and #UART_TDRE          ; Check transmit data register empty bit
    beq DetectBRAMMsgLoop     ; Loop if not empty

    lda TxtDetectBRAM,x       ; Get character
    beq DetectBRAMMsgDone     ; If it's zero, we're done
    sta UartTx              ; Transmit
    inx
    jmp DetectBRAMMsgLoop     ; Loop back to wait for empty buffer

DetectBRAMMsgDone:
    rts

; Using polling display message of competed initialization.
InitCompleteMsg:
    ldx #0
InitCompleteMsgLoop:
    lda UartSt              ; Get UART status
    sta UartStRam
    and #UART_TDRE          ; Check transmit data register empty bit
    beq InitCompleteMsgLoop     ; Loop if not empty

    lda TxtInitComplete,x       ; Get character
    beq InitComleteMsgDone     ; If it's zero, we're done
    sta UartTx              ; Transmit
    inx
    jmp InitCompleteMsgLoop     ; Loop back to wait for empty buffer

InitComleteMsgDone:
    rts

; Using polling display message of device detected.
DeviceDetectedMsg:
    ldx #0
DeviceDetectedMsgLoop:
    lda UartSt              ; Get UART status
    sta UartStRam
    and #UART_TDRE          ; Check transmit data register empty bit
    beq DeviceDetectedMsgLoop     ; Loop if not empty

    lda TxtDeviceDetected,x       ; Get character
    beq DeviceDetectedMsgDone     ; If it's zero, we're done
    sta UartTx              ; Transmit
    inx
    jmp DeviceDetectedMsgLoop     ; Loop back to wait for empty buffer

DeviceDetectedMsgDone:
    rts

; Using polling display message of device not detected.
DeviceNotDetectedMsg:
    ldx #0
DeviceNotDetectedMsgLoop:
    lda UartSt              ; Get UART status
    sta UartStRam
    and #UART_TDRE          ; Check transmit data register empty bit
    beq DeviceNotDetectedMsgLoop     ; Loop if not empty

    lda TxtDeviceNotDetected,x       ; Get character
    beq DeviceNotDetectedMsgDone     ; If it's zero, we're done
    sta UartTx              ; Transmit
    inx
    jmp DeviceNotDetectedMsgLoop     ; Loop back to wait for empty buffer

DeviceNotDetectedMsgDone:
    rts

; Using polling display message of detecting UART.
DetectUartMsg:
    ldx #0
DetectUartMsgLoop:
    lda UartSt              ; Get UART status
    sta UartStRam
    and #UART_TDRE          ; Check transmit data register empty bit
    beq DetectUartMsgLoop     ; Loop if not empty

    lda TxtDetectUart,x       ; Get character
    beq DetectUartMsgDone     ; If it's zero, we're done
    sta UartTx              ; Transmit
    inx
    jmp DetectUartMsgLoop     ; Loop back to wait for empty buffer

DetectUartMsgDone:
    rts

.endif  ; Debug

;-----------------------------------------------------------------------------
; MOS Initialization (clear MOS variables)
;-----------------------------------------------------------------------------
InitMOS:
    lda #$0
    ldx #$7F                ; Do half a page ($7F to $00 inclusive)
InitMOSLoop:
    sta $80,x               ; Clear top half of zero page
    dex
    bpl InitMOSLoop
    rts

;-----------------------------------------------------------------------------
; UART ISR Initialization
;-----------------------------------------------------------------------------
InitUARTISR:
    lda #0
    sta UartTxInPt
    sta UartTxOutPt
    sta UartRxInPt
    sta UartRxOutPt
    rts

;-----------------------------------------------------------------------------
; RTC Initialization, disable interrupts when calling this function
;-----------------------------------------------------------------------------
InitDS1685:
    ; *** detect RTC chip
.ifdef Debug
    jsr DetectRTCMsg
.endif
    jsr DetectDS1685
    bne InitDS1685OK        ; different than 0, device detected
.ifdef Debug
    jsr DeviceNotDetectedMsg
.endif
    rts                     ; not detected, return
InitDS1685OK:
.ifdef Debug
    jsr DeviceDetectedMsg
.endif
    ; *** setup shadow RTC registers in RAM
    lda #0
    sta Timer64Hz
    sta Timer64Hz+1
    sta Timer64Hz+2
    sta Timer64Hz+3
    lda #DSC_REGB_SQWE      ; enable square-wave output
    ora #DSC_REGB_DM        ; set binary mode
    ora #DSC_REGB_24o12     ; set 24h format
    ora #DSC_REGB_PIE       ; enable periodic interrupt
    sta RegB
    lda #DSC_REGA_OSCEN     ; oscillator ON
    ora #$0A                ; 64 Hz on SQWE and PF
    sta RegA
    lda #$B0                ; AUX battery enable, CS=1 (12.5pF crystal), E32K=0
                            ; (SQWE freq. set by RS3..RS0), RCE=1 (/RCLR enable)
    sta RegXB
    lda #$08                ; set PWR pin to high impedance
    sta RegXA
    ; *** call the actual RTC chip driver function
    jsr DS1685Init
    rts

;-----------------------------------------------------------------------------
; Detect the presence of DS1685 RTC device / chip.
; return 0 in Acc if not detected, non-zero otherwise
;-----------------------------------------------------------------------------
DetectDS1685:
    jsr Switch2Bank1RTC
    lda #$40
    jsr RdRTC
    cmp #$71
    beq DetectDS1685OK
    lda #$00            ; device not detected
    rts
DetectDS1685OK:
    jsr Switch2Bank0RTC
    lda DetectedDevices
    ora #DEVPRESENT_RTC
    sta DetectedDevices
    rts

;-----------------------------------------------------------------------------
; Initialize Banked RAM.
; Memory itself is not initialized.
; Bank #0 is selected and the bank# is put in the RAM register RamBankNum
;-----------------------------------------------------------------------------
InitBankedRam:
    lda #$00
    sta RamBankNum
    sta RamBankSwitch
    rts

;-------------------------------------------------------------------------------
; MOS Prompt
;-------------------------------------------------------------------------------

MOSPrompt:
    ; Display intro message
    lda #<TxtHeader
    sta StrPtr
    lda #>TxtHeader
    sta StrPtr+1
    jsr Puts

MOSPromptLoop:

    ; Output the prompt
    lda #<TxtPrompt
    sta StrPtr
    lda #>TxtPrompt
    sta StrPtr+1
    jsr Puts

    ; Get a line
    jsr GetLine

    ; Echo a CR sequence
    lda #$0D
    jsr PutCh
    lda #$0A
    jsr PutCh

    ; Process the command
    jsr MOSProcess

    ; Loop eternally
    jmp MOSPromptLoop

;-------------------------------------------------------------------------------
; Process Command Line
;-------------------------------------------------------------------------------

MOSProcess:
    ; Check for null command
    lda #0
    cmp PromptLine
    bne MOSProcess1
    ; If the user simply pressed return, simply redisplay the prompt.
    rts

MOSProcess1:
    ; First letter must be a command--find it in the table
    ldx #MOSCmdNum
MOSProcessLoop:
    dex
    bmi MOSProcessBad
    lda MOSCmdTxt,x
    cmp PromptLine
    bne MOSProcessLoop

    ; Now jump using the table; load the address into RAM and use indirect
    txa                             ; Multiply X by two for a WORD pointer
    asl a
    tax
    lda MOSCmdLoc,x                 ; Get pointer into a ram VAR
    sta ArrayPtr1
    lda MOSCmdLoc+1,x
    sta ArrayPtr1+1
    jmp (ArrayPtr1)                 ; Jump indirect

MOSProcessBad:
    lda #<TxtNoCmd
    sta StrPtr
    lda #>TxtNoCmd
    sta StrPtr+1
    jsr Puts
    rts

;-------------------------------------------------------------------------------
; The MOS Commands
;-------------------------------------------------------------------------------

    ; --- The "Help" Command ---
MOSHelp:
    lda #<TxtHelp
    sta StrPtr
    lda #>TxtHelp
    sta StrPtr+1
    jsr Puts
    rts

    ; --- The "Write Memory" Command ---
MOSWriteMem:
    ; Verify 2nd char is space
    lda #' '
    cmp PromptLine+1
    beq MOSWriteMem1
    jmp ProcessNoFmt
MOSWriteMem1:

    ; Get addr into array ptr 1
    lda #PromptLine+2
    sta StrPtr
    lda #0
    sta StrPtr+1
    jsr Hex2Word
    ; Move ArrayPtr1 to ArrayPtr2
    lda ArrayPtr1
    sta ArrayPtr2
    lda ArrayPtr1+1
    sta ArrayPtr2+1

    ; Convert and store bytes
MOSWriteMemLoop:
    ldy #0                          ; Set Y for indirect
    lda #' '                        ; Verify next char is a space
    cmp (StrPtr),y
    bne MOSWriteMemDone             ; Non-space means end of write
    inc StrPtr                      ; Point to databyte
    jsr Hex2Byte                    ; Convert string to value
    lda ArrayPtr1                   ; Get the byte
    sta (ArrayPtr2),y               ; Store it.
    inc ArrayPtr2                   ; Incrememt storage location pointer
    bne MOSWriteMemLoop             ; If inc result was zero, carry is required
    inc ArrayPtr2+1                 ; Incrememt hi-byte of pointer
    jmp MOSWriteMemLoop
MOSWriteMemDone:
    rts

    ; --- The "Read Memory" Command ---
MOSReadMem:
    ; Verify 2nd char is space
    lda #' '
    cmp PromptLine+1
    beq MOSReadMem1
    jmp ProcessNoFmt
MOSReadMem1:

    ; Get start addr into array ptr 3
    lda #PromptLine+2
    sta StrPtr
    lda #0
    sta StrPtr+1
    jsr Hex2Word

    ; Move ArrayPtr1 to ArrayPtr3 (this is the start address)
    lda ArrayPtr1
    sta ArrayPtr3
    lda ArrayPtr1+1
    sta ArrayPtr3+1

    ; Get end addr into array ptr 4
    lda #'-'                ; Dash in 7th col indicates range address to follow
    cmp PromptLine+6
    beq MOSReadMemGetEndAddr
    jmp MOSReadMemPage
MOSReadMemGetEndAddr:
    lda #PromptLine+7       ; Get ending address
    sta StrPtr
    lda #0
    sta StrPtr+1
    jsr Hex2Word

    ; Move ArrayPtr1 to ArrayPtr4 (this is the end address)
    lda ArrayPtr1
    sta ArrayPtr4
    lda ArrayPtr1+1
    sta ArrayPtr4+1

    ; Make end address inclusive (by adding one)
    inc ArrayPtr4               ; Increment the lo byte
    bne MOSReadMemGet3rdArg
    inc ArrayPtr4+1             ; And cover the case where a carry is required

    ; todo... some checking-- that the end address is at least one more
MOSReadMemGet3rdArg:

    lda #' '
    cmp PromptLine+11           ; looking for 3-rd argument
    beq MOSReadMemChk3rdArg
    jmp MOSReadMemRow
MOSReadMemChk3rdArg:
    lda #'+'
    cmp PromptLine+12           ; check if canonical dump flag
    beq MOSReadMemCanonical     ; yes, user wants hex + ascii dump
    jmp ProcessNoFmt            ; something wrong with command format

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

MOSReadMemPage:           ; confirmed no 2-nd address
    ; No second address means display one page. First, copy start to end
    lda ArrayPtr3
    sta ArrayPtr4
    lda ArrayPtr3+1
    sta ArrayPtr4+1
    inc ArrayPtr4+1             ; Increment the hi byte of the end address
    ; check if there is optional argument for canonical dump
    lda #' '                ; Space in 7th col indicates possible optional '+'
    cmp PromptLine+6        ; as 2-nd argument
    bne MOSReadMemRow       ; not a space, just do page dump
    lda #'+'                ; check if optional '+' argument for ascii dump
    cmp PromptLine+7
    beq MOSReadMemCanonical ; yes, branch to canonical memory dump procedure
    jmp ProcessNoFmt        ; something wrong with command format

    ; The top of the loop, for rows
MOSReadMemRow:
    ; Display the address, prepended with 'w' (this is so output can be captured
    ; by a terminal program and replayed later, implementing a clever, crude,
    ; but effective load/save of memory).
    lda #'w'                ; 'w' display
    jsr PutCh
    lda #' '                ; ' ' (space) display
    jsr PutCh
    lda ArrayPtr3+1         ; Address display
    jsr PutHex
    lda ArrayPtr3
    jsr PutHex

    ldy #0
    sty Cnt1                ; Initialize column counter

    ; The inner loop for columns
MOSReadMemCol:
    lda #' '                ; ' ' (space) between bytes
    jsr PutCh
    ldy #0
    lda (ArrayPtr3),y       ; Get the byte
    jsr PutHex              ; Put a byte to the display

    ; Increment the address
    inc ArrayPtr3
    bne MOSReadMemComp      ; If the last inc produced a zero result...
    inc ArrayPtr3+1         ; ...then a carry to hi-byte is required.
MOSReadMemComp:
    ; Check for end of range
    lda ArrayPtr3+1
    cmp ArrayPtr4+1         ; Compare hi bytes of addresses first
    bmi MOSReadMemCont      ; If end hi byte less than current hi byte, continue
    lda ArrayPtr3
    cmp ArrayPtr4           ; Compare lo bytes of addresses if hi were equal
    bmi MOSReadMemCont      ; If end lo byte less than current lo byte, continue

    ; Otherwise the addresses are equal, and the routine is done (one last CR)
    lda #$0D
    jsr PutCh
    lda #$0A
    jsr PutCh
    rts                     ; Exit point

MOSReadMemCont:
    inc Cnt1                ; Increment column counter
    ldy Cnt1
    cpy #$10
    bmi MOSReadMemCol

    ; Carriage return
    lda #$0D
    jsr PutCh
    lda #$0A
    jsr PutCh

    ; Do next row
    jmp MOSReadMemRow

    ; --- The "Execute" Command ---
MOSExecute:
    ; Verify 2nd char is space
    lda #' '
    cmp PromptLine+1
    beq MOSExecute1
    jmp ProcessNoFmt
MOSExecute1:

    ; Get addr into array ptr 1
    lda #PromptLine+2
    sta StrPtr
    lda #0
    sta StrPtr+1
    jsr Hex2Word

    ; Jump to it (the routine to execute should end with the rts instruction)
    jmp (ArrayPtr1)

    ; --- The "Continue" Command ---
MOSContinue:
    lda StackDumpV          ; Check for stack frame
    beq MOSContinueNoStack
    sei                     ; Disable interrupts
    lda #0                  ; Clear leftover 'c' from command line)
    sta PromptLine
    sta PromptLen
    sta StackDumpV          ; Clear valid stack dump flag
    ldx StackDump           ; Get stack frame pointer
    txs
    ; The same code for finishing an IRQ will restore the system's state.
    jmp IrqDone

MOSContinueNoStack:
    lda #<TxtNoStack
    sta StrPtr
    lda #>TxtNoStack
    sta StrPtr+1
    jsr Puts
    rts

;-------------------------
; Incorrect command format
ProcessNoFmt:
    lda #<TxtNoFmt
    sta StrPtr
    lda #>TxtNoFmt
    sta StrPtr+1
    jsr Puts
    rts

; ---------------- Print date / time command ------------------
MOSPrintDtTm:
    lda #FuncCodeDtTm
    sta FuncCodeReg
    jsr RomLibFunc
    rts

; ----------------- Set date / time command -------------------
MOSSetDtTm:
    lda #FuncCodeSetDtTm
    sta FuncCodeReg
    jsr RomLibFunc
    rts

; ------------- Show / select RAM bank command ----------------
MOSRamBank:
    lda #' '
    cmp PromptLine+1
    bne MOSRamBankShow
    ; select RAM bank
    lda #PromptLine+2
    sta StrPtr
    lda #0
    sta StrPtr+1
    ldx #0
    jsr Hex2Byte
    lda ArrayPtr1
    jsr BankedRamSel
    ; show RAM bank
MOSRamBankShow:
    lda RamBankNum
    jsr PutHex
    lda #$0D
    jsr PutCh
    lda #$0A
    jsr PutCh
    rts

; ------------------- Copy memory command ---------------------
; m <dest> <src> <size>
; m HHHH HHHH HHHH
MOSMemCpy:
    ; check if format of last argument correct (4-digit)
    ; using wrong format here may lead to overwriting wrong area of memory
    lda #0
    cmp PromptLine+12
    beq MOSMemCpyFmtErr
    lda #0
    cmp PromptLine+13
    beq MOSMemCpyFmtErr
    lda #0
    cmp PromptLine+14
    beq MOSMemCpyFmtErr
    lda #0
    cmp PromptLine+15
    beq MOSMemCpyFmtErr
    ; Verify 2nd char is space
    lda #' '
    cmp PromptLine+1
    beq MOSMemCpy1
MOSMemCpyFmtErr:
    jmp ProcessNoFmt
MOSMemCpy1:
    ; Get dest addr into FuncCodeArgs
    lda #PromptLine+2
    sta StrPtr
    lda #0
    sta StrPtr+1
    jsr Hex2Word
    ; Move ArrayPtr1 to FuncCodeArgs (this is the destination address)
    lda ArrayPtr1
    sta FuncCodeArgs
    lda ArrayPtr1+1
    sta FuncCodeArgs+1
    ; Get src addr into FuncCodeArgs+1
    lda #' '                ; check if space
    cmp PromptLine+6
    bne MOSMemCpyFmtErr     ; no - error
    lda #PromptLine+7       ; Get src address
    sta StrPtr
    lda #0
    sta StrPtr+1
    jsr Hex2Word
    ; Move ArrayPtr1 to FuncCodeArgs+2 (this is the source address)
    lda ArrayPtr1
    sta FuncCodeArgs+2
    lda ArrayPtr1+1
    sta FuncCodeArgs+3
    ; and finally get bytes count to copy
    lda #' '
    cmp PromptLine+11
    beq MOSMemCpy2
    jmp ProcessNoFmt
MOSMemCpy2:
    lda #PromptLine+12
    sta StrPtr
    lda #0
    sta StrPtr+1
    jsr Hex2Word
    ; Move ArrayPtr1 to FuncCodeArgs+4 (this is the bytes count to copy)
    lda ArrayPtr1
    sta FuncCodeArgs+4
    lda ArrayPtr1+1
    sta FuncCodeArgs+5
    ; now setup pointer to the array of arguments, which starts at index 4
    lda #<FuncCodeArgs
    sta FuncCodeArgPtr
    lda #>FuncCodeArgs
    sta FuncCodeArgPtr+1
    ; set the function code (memory copy)
    lda #FuncCodeMemCpy
    sta FuncCodeReg
    ; and call rom library function
    jsr RomLibFunc
    rts

; ------------------- Init memory command ---------------------
; i hhhh-hhhh hh
MOSMemInit:
    ; check command format
    lda #'-'
    cmp PromptLine+6
    beq MOSMemInit1
MOSMemInitFmtErr:
    jmp ProcessNoFmt
MOSMemInit1:
    lda #' '
    cmp PromptLine+11
    bne MOSMemInitFmtErr
MOSMemInit2:
    lda #' '
    cmp PromptLine+1
    bne MOSMemInitFmtErr
    ; format good
    ; get start addr
    lda #PromptLine+2
    sta StrPtr
    lda #0
    sta StrPtr+1
    jsr Hex2Word
    lda ArrayPtr1
    sta FuncCodeArgs
    lda ArrayPtr1+1
    sta FuncCodeArgs+1
    ; get end addr
    lda #PromptLine+7
    sta StrPtr
    lda #0
    sta StrPtr+1
    jsr Hex2Word
    lda ArrayPtr1
    sta FuncCodeArgs+2
    lda ArrayPtr1+1
    sta FuncCodeArgs+3
    ; get init value
    lda #PromptLine+12
    sta StrPtr
    lda #0
    sta StrPtr+1
    tax
    jsr Hex2Byte
    lda ArrayPtr1
    sta FuncCodeArgs+4
    lda ArrayPtr1+1
    sta FuncCodeArgs+5
    ; now setup pointer to the arguments
    lda #<FuncCodeArgs
    sta FuncCodeArgPtr
    lda #>FuncCodeArgs
    sta FuncCodeArgPtr+1
    ; set function code
    lda #FuncCodeMemInit
    sta FuncCodeReg
    ; call rom library function
    jsr RomLibFunc
    rts

;-------------------------------------------------------------------------------
;-------------------------------------------------------------------------------
; Some Useful Functions
;-------------------------------------------------------------------------------
;-------------------------------------------------------------------------------

;-------------------------------------------------------------------------------
; Put a string to output--address is in StrPtr (lo) and StrPtr+1 (hi); string is
; zero-terminated. If the output queue contains enough space to store the whole
; string, this routine will return immediately and the ISR will asynchronously
; transmit the string; otherwise execution will spin here until there is enough
; space in the queue. StrPtr is preserved.  Alternately, load Y with an offset
; into the string and call PutsIndexed.
;-------------------------------------------------------------------------------

Puts:
	;jmp (PutsVect)
    nop
    nop
    nop

RomPuts:

    ldy #0                  ; Non-indexed variant starts at zero, of course
PutsIndexed:                ; Indexed variant allows caller to specify start
    lda StrPtr+1            ; Save StrPtr so it isn't modified
    pha
PutsLoop:
    lda (StrPtr),y          ; Get the next char in the string
    beq PutsDone            ; Zero means end of string
    jsr PutCh               ; Otherwise put the char

    ; Update string pointer
    iny                     ; Increment StrPtr-lo
    bne PutsLoop            ; No rollover? Loop back for next character
    inc StrPtr+1            ; StrPtr-lo rolled over--carry hi byte
    jmp PutsLoop            ; Now loop back

PutsDone:
    pla
    sta StrPtr+1            ; Restore StrPtr
    rts

;-------------------------------------------------------------------------------
; Put a character to output (character in A)
;-------------------------------------------------------------------------------

PutCh:
	jmp (PutChVect)

RomPutCh:
    ; Check for Transmit queue full; in-ptr will be one less than out-ptr. (this
    ; wastes one byte in the queue, but otherwise a totally empty and a totally
    ; full queue would both have equal pointers.)
    ldx UartTxInPt
    inx                     ; Increment to check for "one less" condition
PutChWait:
    cpx UartTxOutPt
    beq PutChWait            ; Spin until ISR transmits and updates UartTxOutPtr

    ; Put the char into the transmit queue
    dex                     ; Decrement in-ptr back to where it was
    sta UartTxQue,x         ; Store char in the queue
    inx                     ; Now increment in-ptr for real
    stx UartTxInPt

    ; Enable IRQ--may interrupt immediately if transmitter isn't busy
PutChEn:
    lda UartCtRam
    ora #UART_TXI_EN        ; Use "or"  to set the bits for the transmit IRQ
    sta UartCtRam
    sta UartCt

    ; Done
    rts

;-------------------------------------------------------------------------------
; Get a line by waiting for the CR sequence (provides for editing the line, too)
; The line is stored in PromptLine, and its length is in PromptLen.  The text is
; also zero-terminated. Only backspace is permitted on editing so far.
;-------------------------------------------------------------------------------

GetLine:

	;jmp (GetsVect)
    nop
    nop
    nop

RomGets:

	; The following instruction is useless but harmless
    ;lda PromptFlag          ; Simulator will detect this read and suspend

    lda #$0
    sta PromptLen           ; Clear prompt length variable
GetLineLoop:
    jsr GetCh               ; Get the character into A
    cmp #$0D                ; Compare with CR
    beq GetLineFinish       ; Finish line
    cmp #$08                ; Compare with BS
    beq GetLineBkSpc        ; Handle backspace

GetLineStoreCh:
    ldx PromptLen           ; Get current length of line
    cpx #PromptMax-1        ; Compare with max length (room for a terminator)
    beq GetLineLoop         ; If at max, silently drop character
    sta PromptLine,x        ; Otherwise, store character
    inc PromptLen           ; Update length of line

GetLineEcho:
    pha                     ; Save character
    jsr PutCh               ; Echo the character (todo..check for editing key)
    pla

    jmp GetLineLoop         ; Loop for next character

GetLineBkSpc:
    ldx PromptLen           ; Get current length of line
    beq GetLineLoop         ; If line is blank, loop for next character
    dec PromptLen           ; Otherwise simply back up the length variable
    jmp GetLineEcho         ; Echo the $08 character so the terminal can BS

GetLineFinish:
    ; Zero-terminate the line
    ldx PromptLen
    lda #0
    sta PromptLine,x
    ; Do NOT echo any CR or CRLF (the calling routine should do this if needed)

    ; This would be the place to implement a "moskey" command history program.

    rts

;-------------------------------------------------------------------------------
; Wait until at least one character is available in the input queue, and return
; the first character in A
;-------------------------------------------------------------------------------

GetCh:
	jmp (GetChVect)
RomGetCh:
    ldx UartRxOutPt
    cpx UartRxInPt          ; If the in-ptr equals the out-ptr, queue is empty.
    beq RomGetCh            ; Branch (wait) if queue is empty.
    lda UartRxQue,x         ; Get the character
    inc UartRxOutPt         ; Update out-ptr
    rts

;-------------------------------------------------------------------------------
; Check if there is a character available in the input queue.
; Return 0 in Acc if not, return character in Acc if there is one.
; Do not update / modify input queue. (character will be still waiting to be
; read with GetCh.)
;-------------------------------------------------------------------------------

KbHit:
    lda #0
    ldx UartRxOutPt
    cpx UartRxInPt          ; If the in-ptr equals the out-ptr, queue is empty.
    bne KbHit01             ; queue not empty
    rts
KbHit01:
    lda UartRxQue,x
    rts

;-------------------------------------------------------------------------------
; Translate the character string at StrPtr into an address; store in ArrayPtr1
; At this time, no conversion errors are checked--what you enter is what you
; get, even if it's nonsense.
;-------------------------------------------------------------------------------

Hex2Word:
    ldx #1                  ; Do hi (2nd) byte of address first

    jsr Hex2Byte            ; Convert hi byte
    dex                     ; Now do lo (1st) byte of address
    jsr Hex2Byte            ; Convert lo byte
    rts

;-------------------------------------------------------------------------------
; Convert two ASCII characters (pointed to by StrPtr) into a byte; store in
; ArrayPtr1,x (X must contain offset from first byte of ArrayPtr1). StrPtr
; modified to point to next char after 4-char address
;-------------------------------------------------------------------------------

Hex2Byte:
    ldy #0                  ; Clear Y to use for indirect
    lda (StrPtr),y          ; Get the 1st char (hi nybble)
    inc StrPtr              ; note: cannot cross page boundary
    jsr AHex2Val            ; Convert to value
    asl a                   ; Shift 4 bits left
    asl a
    asl a
    asl a
    sta ArrayPtr1,x         ; Store
    lda (StrPtr),y          ; Get the 2nd char
    inc StrPtr
    jsr AHex2Val            ; Convert
    clc
    adc ArrayPtr1,x         ; Add hi nybble
    sta ArrayPtr1,x         ; Store total in byte
    rts

;-------------------------------------------------------------------------------
; Convert an ASCII character (0-9 and a-f) in A to its hex value. Hi nybble is
; non-zero if error occurred.
;-------------------------------------------------------------------------------

AHex2Val:
    sec
    sbc #'0'                ; Convert from ASCII to decimal digit range
    cmp #10                 ; Is in decimal digit range?
    bmi AHex2ValRts
    sec
    sbc #'a'-'0'-10         ; Check for alpha-hex (a thru f)
    cmp #$10                ; Is it in alpha range? ($0A thru $0F)?
    bmi AHex2ValRts
    ora #$F0                ; Conversion error
    sta Error
AHex2ValRts:
    rts

;-------------------------------------------------------------------------------
; Convert a value in A into two hex characters; put them.
;-------------------------------------------------------------------------------

PutHex:
    pha                     ; Save A to do lo nybble later
    lsr a                   ; Get hi nybble into lo nybble
    lsr a
    lsr a
    lsr a
    jsr AVal2Hex            ; Convert to ASCII
    jsr PutCh               ; Put it
    pla                     ; Now do lo nybble
    and #$0F
    jsr AVal2Hex
    jsr PutCh

;-------------------------------------------------------------------------------
; Convert a value in A into its ASCII representation of a Hex digit (low nybble
; only considered).
;-------------------------------------------------------------------------------

AVal2Hex:
    cmp #10                 ; Check for decimal digit conversion or alpha
    bpl AVal2HexAlpha
    adc #'0'                ; Convert in decimal band
    rts

AVal2HexAlpha:
    adc #'a'-10-1           ; Convert in alpha band
    rts

;-------------------------------------------------------------------------------
; Uart Transmit ISR
;-------------------------------------------------------------------------------

UartTransmit:
    ; We know that the transmitter is empty; check to see if any characters are
    ; queued in the transmit queue--if so, load them into the transmitter. The
    ; transmit queue will have a character in it when in-ptr and out-ptr do NOT
    ; match.
    lda UartTxOutPt
    cmp UartTxInPt          ; Compare head to tail
    beq UartXmtDis          ; Empty? Branch to cancel the Xmit IRQ

    ; Load up the next character (this will drop the IRQ until the character is
    ; done transmitting; keep interrupt enabled).
    tay                     ; A contains ptr, transfer to Y
    lda UartTxQue,y         ; Load character into X
    sta UartTx              ; Put in Uart transmitter buffer
    inc UartTxOutPt         ; Increment pointer
    rts                     ; Done

UartXmtDis:
    ; No more characters in the queue; turn off the transmit-empty IRQ.
    lda UartCtRam
    and #UART_TXI_DS        ; Use "and" to clear the bits for the transmit IRQ
    sta UartCtRam
    sta UartCt
    rts                     ; Done

;-------------------------------------------------------------------------------
; Uart Receive ISR
;-------------------------------------------------------------------------------

UartReceive:
    ; Get the character from the Uart to X
    lda UartRx
    ; Check for queue full; as with the transmit side, if the in-ptr is one less
    ; than the out-ptr, the queue is full (this wastes one byte but if I used
    ; that byte then it would be difficult to distinguish between a totally
    ; empty queue and a totally full queue).
    ldx UartRxInPt
    inx                     ; Increment to check for "one less" condition
    cpx UartRxOutPt         ; Compare head to tail
    beq UartRecRts          ; Full? Branch to rts (silently drop the character)
    dex                     ; Decrement in-ptr back to where it was
    sta UartRxQue,x         ; Store char in the queue
    inc UartRxInPt          ; Now increment in-ptr for real

UartRecRts:
    rts

;-------------------------------------------------------------------------------
; Register report (it is assumed that A, X, and Y have been pushed, as well as
; P and PC, as would be normal in an ISR).
;-------------------------------------------------------------------------------

RegReport:
    ; Prepare formatted string
    lda #<TxtRegReport
    sta StrPtr
    lda #>TxtRegReport
    sta StrPtr+1

    lda StackDump           ; Get the pointer to the stack frame
    clc                     ; Back it up to the beginning of the frame
    adc #6
    sta StackDump           ; Store it for use here (at exit, should be same)

    ldy #0                  ; Report PC
    jsr RegReportPutPC
    ldy #4                  ; Report P
    jsr RegReportPut
    ldy #9                 ; Report A
    jsr RegReportPut
    ldy #13                 ; Report X
    jsr RegReportPut
    ldy #17                 ; Report Y
    jsr RegReportPut
    ldy #21                 ; Report S
    jsr RegReportPutS

    lda #$0D
    jsr PutCh
    lda #$0A
    jsr PutCh
    rts

    ; A local subroutine to put the byte from the stack pointed to by X to
    ; the console (X is stepped to the next stack position)
RegReportPut:
    jsr PutsIndexed         ; Output the text part of the variable report
    ldx StackDump
    lda $0100,x             ; Get byte from stack
    jsr PutHex              ; Put it to console
    dec StackDump           ; Move to next stack position
    rts

    ; Same as RegReportPut, except puts a 2-byte value.
RegReportPutPC:
    jsr PutsIndexed         ; Output the text part of the variable report
    ldx StackDump
    lda $0100,x             ; Get hi-byte from stack
    jsr PutHex              ; Put it to console
    ldx StackDump           ; Get to lo-byte of value
    dex
    lda $0100,x             ; Get byte from stack
    jsr PutHex              ; Put it to console
    dec StackDump           ; Move to next stack position (two bytes)
    dec StackDump
    rts

    ; Same as RegReportPut, except puts the saved stack pointer.
RegReportPutS:
    jsr PutsIndexed         ; Output the text part of the variable report
    lda StackDump           ; Get stack dump
    clc
    adc #6                  ; Adjust stack to top of frame
    jsr PutHex              ; Put it to console
    rts

;-------------------------------------------------------------------------------
;-----------------------------------------------------------------------------
; DS-1685 routines
; WARNING: Because the IRQ service routine for DS-1685 RTC requires reading
;          of the chip's status register (RegC), using the functions below
;          without disabling/masking interrupts first will mess it up.
;          Remember SEI before calling any of the RTC functions, then CLI.
;          The reason for this is in hardware. DS-1685 is a multiplexed bus
;          type chip interfaced to this system bus with a clever trick.
;          Therefore reading from / writing to an address of the chip is not
;          an atomic operation - requires two steps - writing the address,
;          then reading or writing from / to that address.
;          See WrRTC / RdRTC functions.
;-----------------------------------------------------------------------------
;-------------------------------------------------------------------------------

;-------------------------------------------------------------------------------
; Initialize RTC chip.
; Parameters: RegB, RegA, RegXB, RegXA
; Returns: RegC in Acc.
; NOTE: Disable interrupts when calling this function
;-------------------------------------------------------------------------------

DS1685Init:
	; initialize control register B
	ldx RegB
	lda #$0b
	jsr WrRTC
	; read status register C
	lda #$0c
	jsr RdRTC
	sta RegC
	; initialize control register A, switch to bank 1
	lda RegA
    jsr S2Bank1RTC001       ; switch to bank 1
    ;----------------- this section replaced with above JSR call
	;ora #DSC_REGA_BANK1		; switch to bank 1
	;tax
	;lda #$0a
	;jsr WrRTC
    ;----------------- end replaced section - to be removed
	; initialize extended control register B
	ldx RegXB
	lda #$4b
	jsr WrRTC
	; initialize extended control register A
	ldx RegXA
	lda #$4a
	jsr WrRTC
	; switch to bank 0
    jsr Switch2Bank0RTC
	ldx #$00
	lda RegC
    rts

;-------------------------------------------------------------------------------
; Read clock data.
; Note: The data is returned via hardware stack.
;       Calling subroutine is responsible for allocating 8 bytes on stack
;       before calling JSR DS1685ReadClock. Return address goes on top
;       of the stack, this subroutine will alter 8 bytes under the return
;       address.
; Clock data are stored in following order below the subroutine return address:
;   seconds, minutes, hours, dayofweek, date, month, year, century
; with 'seconds' being at the bottom and 'century' on top.
; Note:
;   Valid return data on stack only if Acc > 0 (Acc = # of bytes on stack).
;   If Acc = 0, no valid data on stack.
;   Calling subroutine still should deallocate stack space by calling PLA x 8.
; NOTE: Disable interrupts when calling this function
;-------------------------------------------------------------------------------

DS1685ReadClock:
; disable update transfers

	lda #$0b
	jsr RdRTC
	sta RegB			; save register B for later
	ora #DSC_REGB_SET
	tax
	lda #$0b
	jsr WrRTC


; determine mode (BCD or BIN)

	lda #DSC_REGB_DM
	sta Temp
	lda RegB
	bit Temp
	bne binmoderead

; can't do BCD mode yet, return
	; enable update transfers
	lda #$0b
	jsr RdRTC
	and #DSC_REGB_UNSET
	tax
	lda #$0b
	jsr WrRTC
    lda #$00    ; no valid data on stack
	rts

binmoderead:
	; binary mode read

	lda #$00		; load register address of seconds
	jsr RdRTC		; read register value to Acc
	and #%00111111	; mask 2 upper bits
    tsx
    sta $010A,x     ; store Acc (seconds) to allocated stack space

	lda #$02		; load register address of minutes
	jsr RdRTC		; read register value to Acc
	and #%00111111	; mask 2 upper bits
    tsx
    sta $0109,x     ; store Acc (minutes) to allocated stack space

	lda #$04		; load register address of hours
	jsr RdRTC		; read register value to Acc
	pha
	lda #DSC_REGB_24o12
	sta Temp
	lda RegB		; determine which hours mode (12/24 hours)
	bit Temp
	beq mode12hbin
	pla
	and #%00011111	; mask 3 upper bits for 24H mode read
	clc
	bcc storehours
mode12hbin:
	pla
	and #%00001111	; mask 4 upper bits for 12H mode read
storehours:
    tsx
    sta $0108,x     ; store Acc (hours) to allocated stack space

	lda #$06		; load register address of day (of week)
	jsr RdRTC		; read register value to Acc
	and #%00000111	; mask 5 upper bits
    tsx
    sta $0107,x     ; store Acc (day of week) to allocated stack space

	lda #$07		; load register address of date (day of month)
	jsr RdRTC		; read register value to Acc
	and #%00011111	; mask 3 upper bits
    tsx
    sta $0106,x     ; store Acc (day of month) to allocated stack space

	lda #$08		; load register address of month
	jsr RdRTC		; read register value to Acc
	and #%00001111	; mask 4 upper bits
    tsx
    sta $0105,x     ; store Acc (month) to allocated stack space

	lda #$09		; load register address of year
	jsr RdRTC		; read register value to Acc
	and #%011111111	; mask the highest bit
    tsx
    sta $0104,x     ; store Acc (year) to allocated stack space

    jsr Switch2Bank1RTC

	lda #$48		; load register address of century
	jsr RdRTC		; read register value to Acc
    tsx
    sta $0103,x     ; store Acc (century) to allocated stack space

    jsr Switch2Bank0RTC

	; enable update transfers

	lda #$0b
	jsr RdRTC
	and #DSC_REGB_UNSET
	tax
	lda #$0b
	jsr WrRTC
    lda #$08    ; Acc = # of return bytes on stack
	rts

;-------------------------------------------------------------------------------
; Set clock time and date.
; Data for clock are passed via stack:
;   seconds, minutes, hours, day of week, day of month, month, year, century
; in that order - 'seconds' on the bottom, 'century' on top below return address.
; Calling subroutine is responsible to allocate 8 bytes on stack and fill it
; with valid values before calling JSR DS1685SetClock.
; Calling subroutine is also responsible for freeing stack space (PLA x 8).
; NOTE: Disable interrupts when calling this function
;-------------------------------------------------------------------------------

DS1685SetClock:
	; disable update transfers
	; set binary mode
	lda #$0b
	jsr RdRTC
	ora #DSC_REGB_SET
	ora #DSC_REGB_DM
	tax
	lda #$0b
	jsr WrRTC

	; get argument 0 (seconds) from stack
    tsx
    lda $010A,x
	tax
	lda #$00			; write to DS1685 seconds register
	jsr WrRTC

	; get argument 1 (minutes) from stack
    tsx
    lda $0109,x
	tax
	lda #$02			; write to DS1685 minutes register
	jsr WrRTC

	tsx			; hours
    lda $0108,x
	tax
	lda #$04
	jsr WrRTC

	tsx         ; day of week
    lda $0107,x
	tax
	lda #$06
	jsr WrRTC

	tsx         ; date (day of month)
    lda $0106,x
	tax
	lda #$07
	jsr WrRTC

	tsx         ; month
    lda $0105,x
	tax
	lda #$08
	jsr WrRTC

	tsx         ; year
    lda $0104,x
	tax
	lda #$09
	jsr WrRTC

	; enable update transfers

	lda #$0b
	jsr RdRTC
	and #DSC_REGB_UNSET
	tax
	lda #$0b
	jsr WrRTC
	; disable update transfers
	lda #$0b
	jsr RdRTC
	ora #DSC_REGB_SET
	tax
	lda #$0b
	jsr WrRTC

	lda #$0a			; get reg. A
	jsr RdRTC
	ora #DSC_REGA_BANK1		; switch to bank 1
	tax
	lda #$0a
	jsr WrRTC

	tsx         ; century
    lda $0103,x
	tax
	lda #$48
	jsr WrRTC

    jsr Switch2Bank0RTC

	; enable update transfers

	lda #$0b
	jsr RdRTC
	and #DSC_REGB_UNSET
	tax
	lda #$0b
	jsr WrRTC

    rts

;-------------------------------------------------------------------------------
; Set clock time.
; Arguments are passed via stack (3 bytes).
; Calling subroutine is responsible for allocating space on stack before
; calling JSR DS1685SetTime:
;   seconds, minutes, hour
; and then deallocate by calling PLA 3 times.
; NOTE: Disable interrupts when calling this function
;-------------------------------------------------------------------------------

DS1685SetTime:
    jsr Switch2Bank0RTC

	; disable update transfers
	; set binary mode
	lda #$0b
	jsr RdRTC
	ora #DSC_REGB_SET
	ora #DSC_REGB_DM
	tax
	lda #$0b
	jsr WrRTC

	tsx                 ; get argument 0 (seconds)
    lda $0105,x
	tax
	lda #$00			; write to DS1685 seconds register
	jsr WrRTC

	tsx                 ;get argument 1 (minutes)
    lda $0104,x
	tax
	lda #$02			; write to DS1685 minutes register
	jsr WrRTC

	tsx                 ; hours
    lda $0103,x
	tax
	lda #$04
	jsr WrRTC

	; enable update transfers

	lda #$0b
	jsr RdRTC
	and #DSC_REGB_UNSET
	tax
	lda #$0b
	jsr WrRTC

	rts

;-------------------------------------------------------------------------------
; Store value in non-volatile RAM of DS1685 (RTC).
; Parameters: BankNum, RamAddr, RamVal
; NOTE: Disable interrupts when calling this function
;-------------------------------------------------------------------------------

DS1685StoreRam:
    ; if Bank #1, jump to lwrextram
    lda BankNum
    bne lwrextram

    ; switch to Bank 0
    jsr Switch2Bank0RTC

    ; A = address
    ; X = value
    ; write RAM
    lda RamAddr
    ldx RamVal
    jsr WrRTC
    ; exit
    rts

    ; Write RAM in Bank 1
lwrextram:

    jsr Switch2Bank1RTC
    ; load RAM addr. into extended ram address register
    ldx RamAddr
    lda #ExtRamAddr
    jsr WrRTC
    ; load RAM value into extended RAM port
    ldx RamVal
    lda #ExtRamPort
    jsr WrRTC

    ; switch to Bank 0
    jsr Switch2Bank0RTC

    ; exit
    rts

;-------------------------------------------------------------------------------
; Read value from non-volatile RAM of DS1685 (RTC).
; Parameters: BankNum, RamAddr
; Return: value in Acc
; NOTE: Disable interrupts when calling this function
;-------------------------------------------------------------------------------

DS1685ReadRam:
    ; if Bank #1, jump to lwrextram
    lda BankNum
    bne lrdextram

    ; switch to Bank 0
    jsr Switch2Bank0RTC

    ; A = address
    ; read RAM
    lda RamAddr
    jsr RdRTC
    ; exit
    rts

    ; read RAM from Bank 1
    lrdextram:

    jsr Switch2Bank1RTC
    ; load RAM addr. into extended ram address register
    ldx RamAddr
    lda #ExtRamAddr
    jsr WrRTC
    ; load RAM value into extended RAM port
    lda #ExtRamPort
    jsr RdRTC
    sta Temp

    ; switch to Bank 0
    jsr Switch2Bank0RTC

    ; exit
    lda Temp
    rts

; helper procedures

;-------------------------------------------------------------------------------
; Write DS1685 address (Acc).
;-------------------------------------------------------------------------------

WrRTCAddr:
	sta DSCALADDR
	rts

;-------------------------------------------------------------------------------
; Write DS1685 data (Acc).
;-------------------------------------------------------------------------------

WrRTCData:
	sta DSCALDATA
	rts

;-------------------------------------------------------------------------------
; Read DS1685 data (-> Acc).
;-------------------------------------------------------------------------------

RdRTCData:
	lda DSCALDATA
	rts

;-------------------------------------------------------------------------------
; Write DS1685 Acc = Addr, X = Data
;-------------------------------------------------------------------------------

WrRTC:
	jsr WrRTCAddr
	txa
	jsr WrRTCData
	rts

;-------------------------------------------------------------------------------
; Read DS1685 Acc = Addr -> A = Data
;-------------------------------------------------------------------------------

RdRTC:
	jsr WrRTCAddr
	jsr RdRTCData
	rts

;-------------------------------------------------------------------------------
; Swicth to RTC registers in bank 0.
; NOTE: Disable interrupts when calling this function
;-------------------------------------------------------------------------------

Switch2Bank0RTC:

    lda #$0a            ; get register A
	jsr RdRTC
	and #DSC_REGA_BANK0 ; switch to bank 0
	tax
	lda #$0a            ; write register A
	jsr WrRTC
    rts

;-------------------------------------------------------------------------------
; Swicth to RTC registers in bank 0.
; NOTE: Disable interrupts when calling this function
;-------------------------------------------------------------------------------

Switch2Bank1RTC:

	lda #$0a			; get reg. A
	jsr RdRTC

S2Bank1RTC001:          ; this entry point is for if you already have RegA
                        ; loaded in Acc

	ora #DSC_REGA_BANK1	; switch to bank 1
	tax
	lda #$0a            ; write reg. A
	jsr WrRTC
    rts

;-----------------------------------------------------------------------------
;-----------------------------------------------------------------------------
; Banked RAM routines.
;-----------------------------------------------------------------------------
;-----------------------------------------------------------------------------

;-----------------------------------------------------------------------------
; Select RAM bank#.
; Bank# in Acc.
;-----------------------------------------------------------------------------
BankedRamSel:
    sta RamBankNum
    sta RamBankSwitch
    rts

;-----------------------------------------------------------------------------
; Kernel jump table.
;-----------------------------------------------------------------------------
.segment "KERN"

CallSetDt:      ; $FFBD
    jmp MOSSetDtTm

CallPrnDt:      ; $FFC0
    jmp MOSPrintDtTm

; NOTE: This function is different than CallBankRamSel.
;       It expects the command and arguments entered by user in the PromptLine
;       buffer, then it interprets it accordingly.
CallRamBank:    ; $FFC3
    jmp MOSRamBank

CallMemCpy:     ; $FFC6
    jmp MOSMemCpy

CallMemInit:    ; $FFC9
    jmp MOSMemInit
;.ORG    $FFCC
CallKbHit:
  jmp KbHit

;.ORG    $FFCF
CallBankRamSel:
  jmp BankedRamSel

;.ORG    $FFD2
CallDS1685Init:
  jmp DS1685Init

;.ORG    $FFD5
CallDS1685ReadClock:
  jmp DS1685ReadClock

;.ORG    $FFD8
CallDS1685SetClock:
  jmp DS1685SetClock

;.ORG    $FFDB
CallDS1685SetTime:
  jmp DS1685SetTime

;.ORG    $FFDE
CallDS1685StoreRam:
  jmp DS1685StoreRam

;.ORG    $FFE1
CallDS1685ReadRam:
  jmp DS1685ReadRam

;.ORG    $FFE4
CallReadMem:
  jmp MOSReadMem

;.ORG    $FFE7
CallWriteMem:
  jmp MOSWriteMem

;.ORG    $FFEA
CallExecute:
  jmp MOSExecute

;.ORG	$FFED
CallGetCh:
  jmp GetCh

;.ORG    $FFF0
CallPutCh:
  jmp PutCh

;.ORG 	$FFF3
CallGets:
  jmp GetLine

;.ORG 	$FFF6
CallPuts:
  jmp Puts

;-----------------------------------------------------------------------------
; 6502 Reset and Interrupt vectors.
;-----------------------------------------------------------------------------

.segment "VECTORS"

;.ORG	$FFFA

.BYTE	<NMIJUMP, >NMIJUMP
.BYTE	<RESJUMP, >RESJUMP
.BYTE	<IRQJUMP, >IRQJUMP

; ------------------------ THE END -------------------------------------------
