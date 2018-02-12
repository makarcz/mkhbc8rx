;------------------------------------------------------------------
;
; File: 	mkhbcos_ds1685.s
; Author:	Marek Karcz
; Purpose:	Implements initialization routines and API for
;			Real Time Clock chip DS1685 with multiplexed
;           address bus connected to buffered I/O bus as an I/O
;           device.
;           This file is a part of MKHBCOS operating system programming API
;           for MKHBC-8-Rx computer.
;
; Revision history:
; 2012-01-31:
;	Initial revision.
;   (NOTE: These routines will eventually make their way
;          to EPROM as a part of firmware. At that time,
;          this file will be revised to call up the API
;          functions in the kernal table, instead of
;          being full implementation.)
;
; 2012-02-06:
;	Implementation.
;
; 2015-11-29
;   I/O slot assignment changed.
;
; 2015-12-5
;   Modification due to hardware changes (Chris Ward variant):
;   DSCALADDR and DSCALDATA order changed.
;
; 2017-06-15:
;    Added functions to store and read non-volatile RAM.
;
; 2018-01-21
;   Moved RTC routines to EPROM (BIOS) and replaced implementation in this
;   source file to passing parameters, calling MOS API and retrieving return
;   values.
;
; 2018-01-30
;   Disabled interrupts during each RTC API call.
;
; 2018-02-11
;   Replaced local MKHBCOS definitions with ones coming from "mkhbcos_ml.inc".
;
;-----------------------------------------------------------------------------
; GVIM
; set tabstop=2 shiftwidth=2 expandtab
; NOTE: I switched to ATOM editor in Feb 2018, setup soft TAB=4.
;       I'm using language-65asm 5.0.0 package with it for syntax.
;       I'm not sure how well this file will work in GVIM anymore
;       after saving it from ATOM with above settings.
;-----------------------------------------------------------------------------

; Load MKHBCOS definitions.
.include "mkhbcos_ml.inc"

; Assembler and linker directives.
.setcpu	"6502"
.import ldaxysp,pushax,popax,pusha,popa,staspidx
.import incsp2,incsp3,incsp4,ldauidx

.segment "DATA"

; code

.export _ds1685_init,_ds1685_rdclock,_ds1685_setclock,_ds1685_settime
.export _ds1685_readram,_ds1685_storeram
;,_read

.segment "CODE"

; Initialize DS1685 RTC chip.
; unsigned char __fastcall__ ds1685_init (unsigned char regb,
;                                         unsigned char rega,
;                                         unsigned char regextb,
;                                         unsigned char regexta)

.proc _ds1685_init: near

.segment "CODE"
	; get parameters, put them in temp. buffer
	jsr pusha
	ldy #$03
	lda (sp),y
	sta RegB
	ldy #$02
	lda (sp),y
	sta RegA
	ldy #$01
	lda (sp),y
	sta RegXB
	ldy #$00
	lda (sp),y
	sta RegXA
    sei
    jsr mos_DS1685Init
    cli
    jsr incsp4
	rts

.endproc

.segment "CODE"

; read clock data
; struct ds1685_clkdata *ds1685_rdclock(struct ds1685_clkdata *buf);
; struct ds1685_clkdata
; {
;	unsigned char seconds;
;	unsigned char minutes;
;	unsigned char hours;
;	unsigned char dayofweek;
;	unsigned char date; // day
;	unsigned char month;
;	unsigned char year;
;	unsigned char century;
; };

.proc _ds1685_rdclock:near

.segment "CODE"
    ; allocate 8 bytes on stack
    lda #$00
    pha
    pha
    pha
    pha
    pha
    pha
    pha
    pha
    ; call MOS subroutine
    sei
    jsr mos_DS1685ReadClock
    cli
    ; transfer returned data from HW stack to return stack
    pla
	ldy #$07
	jsr Tfer2RetBuf	; transfer to return buffer at index 7 (century)
    pla
	ldy #$06
	jsr Tfer2RetBuf	; transfer to return buffer at index 6 (year)
    pla
	ldy #$05
	jsr Tfer2RetBuf	; transfer to return buffer at index 5 (month)
    pla
	ldy #$04
	jsr Tfer2RetBuf	; transfer to return buffer at index 4 (day of month)
    pla
	ldy #$03
	jsr Tfer2RetBuf	; transfer to return buffer at index 3 (day of week)
    pla
	ldy #$02
	jsr Tfer2RetBuf	; transfer to return buffer at index 2 (hours)
    pla
	ldy #$01
	jsr Tfer2RetBuf	; transfer to return buffer at index 1 (minutes)
    pla
	ldy #$00
	jsr Tfer2RetBuf	; transfer to return buffer at index 0 (seconds)

	ldy #$01
	jsr ldaxysp
	jsr incsp2
	rts

.endproc


.segment "CODE"

; set clock data
; void ds1685_setclock (struct ds1685_clkdata *buf);

.proc _ds1685_setclock:near

.segment "CODE"
	ldy #$00			; get argument 0 (seconds)
	jsr GetParFromSpIdx
    pha                 ; push 'seconds' to stack

	ldy #$01			; get argument 1 (minutes)
	jsr GetParFromSpIdx
    pha                 ; push 'minutes' to stack

	ldy #$02			; hours
	jsr GetParFromSpIdx
    pha

	ldy #$03			; day of week
	jsr GetParFromSpIdx
    pha

	ldy #$04			; date (day of month)
	jsr GetParFromSpIdx
    pha

	ldy #$05			; month
	jsr GetParFromSpIdx
    pha

	ldy #$06			; year
	jsr GetParFromSpIdx
    pha

	ldy #$07			; century
	jsr GetParFromSpIdx
    pha

  ; call MOS subroutine
    sei
    jsr mos_DS1685SetClock
    cli

  ; deallocate (free) space on HW stack
    pla
    pla
    pla
    pla
    pla
    pla
    pla
    pla

	jsr incsp2

	rts

.endproc

.segment "CODE"

; set clock data
; void ds1685_settime (struct ds1685_clkdata *buf);

.proc _ds1685_settime:near

.segment "CODE"

	ldy #$00			; get argument 0 (seconds)
	jsr GetParFromSpIdx
    pha                 ; push 'seconds' to stack

	ldy #$01			; get argument 1 (minutes)
	jsr GetParFromSpIdx
    pha                 ; push 'minutes' to stack

	ldy #$02			; hours
	jsr GetParFromSpIdx
    pha

  ; call MOS subroutine
    sei
    jsr mos_DS1685SetTime
    cli

  ; free space on HW stack
    pla
    pla
    pla

	jsr incsp2

	rts

.endproc

.segment "CODE"

; store value in non-volatile RAM
; void		ds1685_storeram (unsigned char bank,
;			               unsigned char addr,
;					       unsigned char data);

.proc _ds1685_storeram: near

.segment "CODE"
	; get parameters, put them in temp. buffer
	ldy #$02
	lda (sp),y
	sta BankNum
	ldy #$01
	lda (sp),y
	sta RamAddr
	ldy #$00
	lda (sp),y
	sta RamVal

    ; call MOS subroutine
    sei
    jsr mos_DS1685StoreRam
    cli

    ; exit
    jsr incsp3
    rts

.endproc

.segment "CODE"

; read value from non-volatile RAM
; unsigned char __fastcall__	ds1685_readram (unsigned char bank,
;			                                    unsigned char addr);

.proc _ds1685_readram: near

.segment "CODE"
	; get parameters, put them in temp. buffer
	jsr pusha
	ldy #$01
	lda (sp),y
	sta BankNum
	ldy #$00
	lda (sp),y
	sta RamAddr

    ; call MOS subroutine
    sei
    jsr mos_DS1685ReadRam
    cli

    ; exit
    ldx #00
    jsr incsp2
    rts

.endproc

.segment "CODE"

; helper routines

;-------------------------------------------------------------------------------
; Transfer A to return buffer at index Y.
;-------------------------------------------------------------------------------

Tfer2RetBuf:

	pha
	tya
	pha
    ldy     #$01	; transfer to return buffer
    jsr     ldaxysp
 	jsr     pushax
 	ldx     #$00
	pla
	tay
	pla
    jsr     staspidx
	rts

;-------------------------------------------------------------------------------
; Load to A from arguments buffer/stack at index Y.
;-------------------------------------------------------------------------------

GetParFromSpIdx:

	tya
	pha
    ldy   #$01	; get buffer
    jsr   ldaxysp
	sta		Temp
	pla
	tay
	lda		Temp
    jsr   ldauidx
	rts

;------------------------------ END OF FILE -------------------------------------
