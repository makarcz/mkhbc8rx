;-------------------------------------------------------------------------------
;
; File: mkhbcos_sbc.asm
;
; Purpose:
; 6502 Home Brew Computer OS / firmware.
; Based on MOS (6502 SBC Meadow Operating System).
;
; Assembles on ca65 / cl65.
;
; Created on: 2011-12-31
;
; Original credits and copyright for MOS:
;-------------------------------------------------------------------------------
;
; MOS.A65
;
; 6502 SBC Meadow Operating System
;
; by Scott Chidester
; (c) 1993-2002
;
; This file should assemble on any decent assembler with perhaps a few
; modifications. A65 works fine.
;
; This is the assembly source file for the Meadow 6502 SBC, containing an
; operating system with a 6850 interrupt driven buffered driver, a monitor
; program, and perhaps (in the future) a text editor, assembler, disassembler,
; a few simple games, math library, calculator, etc.
;
; This file contains revision 1.02, completed on 12/17/02.
;
;-------------------------------------------------------------------------------
;
; Revision history:
;
; 2011-12-31:
;	Created.
;
; 2012-01-06:
;	Introduced customizable IRQ and I/O functions vectors on page 0.
;	BRK interrupt is now recognized. It returns control to M.O.S.
;
; 2012-01-07
;	Custom vectors in address range $30 - $3a collided with Tiny Basic.
;	Changed addresses from $11 - $1b.
;
; 2015-11-28
;   	UART is now at I/O slot #4.
;
; 2018-01-20
;	Project is now branched / split to be a firmware for MKHBC-8-Rx
;	computer in SBC (Single Board Computer) mode of operation. This means UART
;   hardcoded at I/O slot #1.
;
;-------------------------------------------------------------------------------
; GVIM:
; set tabstop=4 shiftwidth=4 expandtab
;-------------------------------------------------------------------------------

.segment "CODE"

;-------------------------------------------------------------------------------
; Defines
;-------------------------------------------------------------------------------

; MKHBC-8-Rx OS Version number
VerMaj      = 1
VerMin      = 1
VerMnt      = 6

; 6502 CPU

; 62256 Static Random Access Memory (SRAM)

RamBase     = $0000
RamEnd      = RamBase+$7FFF 	;with no VRAM option

; 2764 Erasable Programmable Read Only Memory (EPROM)

RomBase1    =   $C800       ; 1st EPROM chip (optional)
RomBase2    =   $E000		; 2nd EPROM chip (mandatory)

IOSTART = $C000

IO0 = IOSTART   ; $C000 - $C0FF
IO1 = IO0+256   ; $C100 - $C1FF
IO2 = IO1+256   ; $C200 - $C2FF
IO3 = IO2+256   ; $C300 - $C3FF
IO4 = IO3+256   ; $C400 - $C4FF
IO5 = IO4+256   ; $C500 - $C5FF
IO6 = IO5+256   ; $C600 - $C6FF
IO7 = IO6+256   ; $C700 - $C7FF

; 6850 Asynchronous Communications Interface Adapter (ACIA)

; Register addresses
UartBase    = IO1           ; NOTE: In SBC mode of operation this stays
                            ;       hardcoded.
UartCt      = UartBase+$0   ; Control register
UartSt      = UartBase+$0   ; Status register
UartTx      = UartBase+$1   ; Transmit buffer register
UartRx      = UartBase+$1   ; Receive buffer register

; Control register bit fields
UART_RXI_EN = %10000000     ; Receive interrupt enable
UART_TXI_EN = %00100000     ; Transmit interrupt enable, /rts low
UART_TXI_DS = %10011111     ; Transmit interrupt disable, /rts low (AND)
UART_N_8_1  = %00010100     ; No parity, 8 bit data, 1 stop (see docs)
UART_DIV_16 = %00000001     ; Divide tx & rx clock by 16, sample middle
UART_RESET  = %00000011     ; Master reset

; Status register bit fields
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
PromptLine  = $80           ; Prompt line (entered by user)
PromptMax   = $50           ; An 80-character line is permitted ($80 to $CF)
PromptLen   = $D0           ; Location of length variable

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
StrPtr      = $E0           ; String pointer for I/O functions

; MOS Debug variables

; MOS Uart variables
UartTxInPt  = $F0           ; Tx head pointer, for chars placed in buf
UartTxOutPt = $F1           ; Tx tail pointer, for chars taken from buf
UartRxInPt  = $F2           ; Rx head pointer, for chars placed in buf
UartRxOutPt = $F3           ; Rx tail pointer, for chars taken from buf
UartCtRam   = $F4           ; Control register in RAM
UartStRam   = $F5           ; Status register in RAM

; Customizable jump vectors
; Program loaded and run in RAM can modify these vectors
; to drive custom I/O console hardware and attach/change
; handler to IRQ procedure. Interrupt flag should be
; cleared before changes are applied and set when ready.
; Custom IRQ handler routine should make a jump to standard
; handler at the end. Custom I/O function routine should
; end with RTS.

StoreAcc	= 	$11			; Temporary Accumulator store.
IrqVect		=	$0012		; Customizable IRQ vector
GetChVect	=	$0014		; Custom GetCh function jump vector
PutChVect	=	$0016		; Custom PutCh function jump vector
GetsVect	=	$0018		; Custom Gets function jump vector
PutsVect	=	$001a		; Custom Puts function jump vector

.ORG RomBase2

    ; Header
TxtHeader:
	.BYTE	"Welcome to MKHBC-8-R2, MOS6502 based system.",     $0d,$0a
    .BYTE   "(C) 2012-2018 Marek Karcz. All rights reserved.",  $0d,$0a
	.BYTE   "Based on following original work:",                $0d,$0a
    .BYTE   "6502 SBC Meadow Operating System 1.02",            $0D,$0A
    .BYTE   "(C) 1993-2002 Scott Chidester",                    $0d,$0a
	.BYTE	"MKHBC OS "
    .BYTE   $30+VerMaj,'.',$30+VerMin,'.',$30+VerMnt
    .BYTE   ", SBC variant."
    .BYTE   $0D,$0A,$0D,$0A,0


    ; This causes an attempt to "type" the binary file in MS-DOS to stop here.
    .BYTE   $1A

    ; Command Line Prompt
TxtPrompt:
    .BYTE   "mos>",0

    ; Help
TxtHelp:
    .BYTE   $0D,$0A
    .BYTE   "Commands are:",$0D,$0A
    .BYTE   " h                           This help screen",$0D,$0A
    .BYTE   " w <addr> <data> [data] ...  Write data to address",$0D,$0A
    .BYTE   " r <addr>[-addr]             Read address range",$0D,$0A
    .BYTE   " x <addr>                    Execute at address",$0D,$0A
    .BYTE   " c                           Continue from NMI event",$0D,$0A
    .BYTE   " d                           Demo ",'"',"Hello, world!",'"'," program",$0D,$0A
    .BYTE   "All values in hex, addr is 4 characters and data is 2,",$0D,$0A
    .BYTE   "separated by exactly one space; parameters are <required>",$0D,$0A
    .BYTE   "or [optional]. Use lower case.",$0D,$0A,$0D,$0A,0

TxtNoStack:
    .BYTE   "Can''t continue; need a valid stack frame from NMI.",$0D,$0A,0

TxtNoCmd:
    .BYTE   "Unrecognized command. Enter h for help.",$0D,$0A,0

TxtNoFmt:
    .BYTE   "Invalid command format. Enter h for help.",$0D,$0A,0

TxtNmiEvent:
    .BYTE   $0D,$0A,"NMI event.",$0D,$0A,0

TxtRegReport:
    .BYTE   "PC:",0," SR:",0," A:",0," X:",0," Y:",0," SP:",0

TxtPFlags:
    .BYTE   "nv-bdizc"

TxtDemo:
    .BYTE   "w 0400 a9 0c 85 e0 a9 04 85 e1 20 f6 ff 60 48 65 6c 6c",$0D
    .BYTE   "w 0410 6f 2c 20 77 6f 72 6c 64 21 0d 0a 00",$0D
    .BYTE   "x 0400",$0D,0

;-------------------------------------------------------------------------------
; MOS Command prompt intrinsic command tables
;-------------------------------------------------------------------------------

MOSCmdTxt:
    .BYTE   'h'
    .BYTE   'w'
    .BYTE   'r'
    .BYTE   'x'
    .BYTE   'c'
    .BYTE   'd'

MOSCmdLoc:
    .WORD   MOSHelp
    .WORD   MOSWriteMem
    .WORD   MOSReadMem
    .WORD   MOSExecute
    .WORD   MOSContinue
    .WORD   MOSRunDemo

; Number of commands
MOSCmdNum=6


NMIJUMP:

	jmp	NMIPROC
	
RESJUMP:

	jmp RESPROC
	
IRQJUMP:

	jmp (IrqVect)
	;jmp IRQPROC

;-------------------------------------------------------------------------------
;-------------------------------------------------------------------------------
; Non Maskable Interrupt Vector Entry Point and Interrupt Service Routines (ISR)
;-------------------------------------------------------------------------------
;-------------------------------------------------------------------------------

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

SrcIrqLine:

	lda StoreAcc			; restore acc. to be preserved on stack

; Push 6502 registers
    pha
    txa
    pha
    tya
    pha

; 6850 Interrupt Service Routine
; Currently it is the only IRQ source.
; When IRQ controller is built, code needs to go in here that checks
; which device is the source of interrupt.
    lda UartSt              ; Get status from 6850
    sta UartStRam           ; Store in MOS variable in RAM

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

; Irq Service Done--pull registers and return
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

    jsr InitMOS         ; Note, initialize MOS *before* the 6850!
    jsr Init6850        ; This sets some vital MOS variables.
    jsr InitUARTISR

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
	lda #<RomGets		; initialize Gets function jump vector
	sta GetsVect
	lda #>RomGets
	sta GetsVect+1
	lda #<RomPuts		; initialize Puts function jump vector
	sta PutsVect
	lda #>RomPuts
	sta PutsVect+1

    ; Done with init; enable interrupts and goto prompt
    cli
    jmp MOSPrompt

;-------------------------------------------------------------------------------
; 6502 MPU initialization
;-------------------------------------------------------------------------------

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

;-------------------------------------------------------------------------------
; 62256 RAM Initialization
;-------------------------------------------------------------------------------

; Note: Can't be a subroutine because stack is cleared. Also this will appear to
; clear its own indirect pointer while clearing the zero page, but that's OK
; because the pointer will contain zero anyway to point to zero page

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
Init62256NextPage:            ; Adjust page pointer at end
    dec $1
    bpl Init62256Loop        ; BPL works for RAM less than 32K; clears page zero
    sta $1                  ; Clear the pointer
    jmp Init62256RetPt

;-------------------------------------------------------------------------------
; 6850 ACIA (UART) Initialization
;-------------------------------------------------------------------------------
Init6850:
UART_SET    = UART_N_8_1|UART_DIV_16
    lda #UART_RESET         ; Reset ACIA
    sta UartCtRam           ; Always save a copy of the register in RAM
    sta UartCt              ; Set RAM copy first because hardware generates IRQs
    lda #UART_SET           ; Set N-8-1, Div 16 (overwrite old control byte)
    sta UartCtRam
    sta UartCt
    jsr Init6850Msg         ; This displays a message in polled form, for debug
    lda UartCtRam           ; Always load the control byte image from RAM
    ora #UART_RXI_EN        ; Also enable Rx interrupt (no Tx IRQ at this time)
    sta UartCtRam
    sta UartCt          
    rts

TxtInit6850:
.BYTE   $0D,$0A,"6850 Init.",$0D,$0A,0

    ; This will be used for development, and can be removed afterward. It
    ; displays a short message using polling.  In case the first revision of my
    ; hardware and/or software doesn't display anything, this will at least help
    ; to narrow the problem down to either an interrupt issue, or an address/
    ; data/control line issue.
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

;-------------------------------------------------------------------------------
; MOS Initialization (clear MOS variables)
;-------------------------------------------------------------------------------
InitMOS:
    lda #$0
    ldx #$7F                ; Do half a page ($7F to $00 inclusive)
InitMOSLoop:
    sta $80,x               ; Clear top half of zero page
    dex
    bpl InitMOSLoop
    rts

;-------------------------------------------------------------------------------
; UART ISR Initialization
;-------------------------------------------------------------------------------
InitUARTISR:
    lda #0
    sta UartTxInPt
    sta UartTxOutPt
    sta UartRxInPt
    sta UartRxOutPt
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
    ldx #6
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
    bne MOSReadMemPage
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
    bne MOSReadMemRow
    inc ArrayPtr4+1             ; And cover the case where a carry is required

    ; todo... some checking-- that the end address is at least one more

    jmp MOSReadMemRow

MOSReadMemPage:
    ; No second address means display one page. First, copy start to end
    lda ArrayPtr3
    sta ArrayPtr4
    lda ArrayPtr3+1
    sta ArrayPtr4+1
    inc ArrayPtr4+1             ; Increment the hi byte of the end address

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

    ; --- The "Demo" command ---
MOSRunDemo:
    ldy #0
    sei                     ; Disable interrupts
MOSRunDemoLoop:
    lda TxtDemo,y           ; Get character for commands to run
    beq MOSRunDemoDone      ; Stop on string's null termination
    jsr MOSRunDemoHackPt    ; Insert character artificially into receive buffer
    iny                     ; Next command character
    jmp MOSRunDemoLoop

MOSRunDemoDone:
    cli                     ; On return, MOS prompt will think the user entered
    rts                     ;  the commands to run the demo.

;-------------------------
; Incorrect command format
ProcessNoFmt:
    lda #<TxtNoFmt
    sta StrPtr
    lda #>TxtNoFmt
    sta StrPtr+1
    jsr Puts
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
	jmp (PutsVect)

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

	jmp (GetsVect)

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
    beq GetCh               ; Branch (wait) if queue is empty.
    lda UartRxQue,x         ; Get the character
    inc UartRxOutPt         ; Update out-ptr
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

MOSRunDemoHackPt:           ; This is a hack entry point for the demo

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

;-------------------------------------------------------------------------------
;-------------------------------------------------------------------------------
; The call table--external programs can rely on this call table always being
; the same (these addresses should never change)
;-------------------------------------------------------------------------------
;-------------------------------------------------------------------------------

.segment "KERN"

.ORG	$FFED

CallGetCh:
    jmp GetCh

.ORG    $FFF0
CallPutCh:
    jmp PutCh

.ORG 	$FFF3
CallGets:
    jmp GetLine

.ORG 	$FFF6
CallPuts:
    jmp Puts

;-------------------------------------------------------------------------------
; 6502 Reset and Interrupt vectors.
;-------------------------------------------------------------------------------

.segment "VECT"

.ORG	$FFFA

.BYTE	<NMIJUMP, >NMIJUMP
.BYTE	<RESJUMP, >RESJUMP
.BYTE	<IRQJUMP, >IRQJUMP

; ================================== THE END ===================================
