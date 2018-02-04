; ---------------------------------------------------------------------------
;
; File:		mkhbcos_init.s
; Author:	Marek Karcz
;
; Original work: cc65 documentation/customization tutorial
; 
; Purpose:
;
;	 Startup code for cc65 (MKHBC-8-R2 version, to run under MKHBCOS,
;	 M.O.S. derivative).
;
; Revision history:
;
; 2012-01-10:
;	Initial revision.
;
; 2012-01-11:
;   Initialization of memory storage enabled.
;
; 2017-06-16
;   Code moved from crt0.s to mkhbcos_init.s.
;
; ---------------------------------------------------------------------------

.export   _init, _exit
.import   _main

.export   __STARTUP__ : absolute = 1        ; Mark as startup
.import   __RAM_START__, __RAM_SIZE__       ; Linker generated

.import    copydata, zerobss, initlib, donelib

.include  "zeropage.inc"

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
; Call main()

          JSR     _main

; ---------------------------------------------------------------------------
; Back from main (this is also the _exit entry):  force a software break

_exit:    JSR     donelib              ; Run destructors
          ;BRK
		  rts

