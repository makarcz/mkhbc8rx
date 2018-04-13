; ---------------------------------------------------------------------------
;
; File:		mkhbcos_init.s
; Author:	Marek Karcz, 2012-2018.
;
; Original work: cc65 documentation/customization tutorial
;
; Purpose:
;
;	 Startup code for cc65 (MKHBC-8-R2 version, to run under MKHBCOS,
;	 M.O.S. derivative).
;
; ---------------------------------------------------------------------------

.export   _init, _exit
.import   _main

.export   __STARTUP__ : absolute = 1        ; Mark as startup
.import   __RAM_START__, __RAM_SIZE__       ; Linker generated

.import    copydata, zerobss, initlib, donelib

;.include  "zeropage.inc"
.include  "mkhbcos_ml.inc"

; ---------------------------------------------------------------------------
; Place the startup code in a special segment

.segment  "STARTUP"

; ---------------------------------------------------------------------------
; A little light 6502 housekeeping

; This is the entry point of compiled and linked program when ran under
; M.O.S. Look in the map file for beginning of STARTUP segment.
; Provide that address to 'x' command (remember to use lower letters).

_init:
		; NOTE: This part is already done by MKHBCOS
        ;       Don't uncomment unless entire OS is compiled
        ;       with cc65 and loaded as single monolithic code.

		  ;LDX     #$FF                 ; Initialize stack pointer to $01FF
          ;TXS
          ;CLD                          ; Clear decimal mode

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
; Call main(), then destructors and return.

          JSR     _main
          JSR     donelib
          RTS

; ---------------------------------------------------------------------------
; Exit function / _exit entry:  force a software break

_exit:    JSR     donelib              ; Run destructors
          LDA     #$00                 ; Reset RAM bank to bank #0
          JSR     mos_BankedRamSel
          JSR     mos_RTCEnablePIE     ; Enable periodic RTC interrupt
exitl001: JSR     mos_KbHit            ; Flush keyboard / serial port buffer
          BEQ     exitend
          JSR     mos_CallGetCh
          JMP     exitl001
exitend:  BRK                          ; Pass control back to O.S.
		  BRK
