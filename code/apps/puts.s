;------------------------------------------------------------------
;
; File: 	puts.s
; Author:	Marek Karcz
; Purpose:	Implement's mos_puts procedure (MKHBCOS's Puts) to be
;			called from C programs (cc65).
;
; Revision history:
;	2012-01-10:
;		Initial revision.
;
;------------------------------------------------------------------

; M.O.S. API defines

.define 	mos_StrPtr		$E0
.define		mos_CallPuts	$FFF6
.define		mos_CallPutCh	$FFF0

.setcpu	"6502"
.import ldaxysp
.import incsp2

; code

.export _mos_puts,_puts,_putchar

.segment "CODE"

.proc _mos_puts: near

.segment "CODE"

	ldy #$01
	jsr ldaxysp
	sta mos_StrPtr
	stx mos_StrPtr+1
	jsr mos_CallPuts
	jsr incsp2
	rts

.endproc
	
.proc _puts: near

.segment "CODE"

	sta mos_StrPtr
	stx mos_StrPtr+1
	jsr mos_CallPuts
	lda #$00
	tax
	rts

.endproc

.proc _putchar: near

.segment "CODE"

	jsr mos_CallPutCh
	lda #$00
	tax
	rts

.endproc
