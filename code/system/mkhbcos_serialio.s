;-----------------------------------------------------------------------------
;
; File: 	mkhbcos_serialio.s
; Author:	Marek Karcz
; Purpose:	Implement's serial console functions.
;           This file is a part of MKHBCOS operating system programming API
;           for MKHBC-8-Rx computer.
;
; Revision history:
;	2012-01-10:
;		Initial revision.
;
;   2012-01-11:
;       Added several I/O routines.
;
;   2018-02-05
;       Added kbhit() impl.
;
;   2018-02-11
;       Replaced local MKHBCOS definitions with ones coming from
;       "mkhbcos_ml.inc".
;
;   2018-02-21
;       In this implementation, functions 'getc', 'getchar' and 'fgets' are
;       identical. I replaced duplicate code with a jump to a common
;       procedure 'getcharacter'.
;       Got rid of function mos_puts().
;
;-----------------------------------------------------------------------------

.include "mkhbcos_ml.inc"

.setcpu	"6502"
.import ldaxysp,pushax,popax,pusha,popa
.import incsp2

; code

.export _puts,_putchar,_gets,_getchar,_getc,_fgetc,_kbhit
;_mos_puts
;,_read

;.segment "CODE"

;.proc _mos_puts: near

;.segment "CODE"

;	ldy #$01
;	jsr ldaxysp
;	sta mos_StrPtr
;	stx mos_StrPtr+1
;	jsr mos_CallPuts
;	jsr incsp2
;	rts

;.endproc

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

.proc _gets: near

.segment "CODE"

	sta tmp_zpgPt
	stx tmp_zpgPt+1
	jsr mos_CallGets    ; entered string stored in mos_PromptLine

; copy string to the return pointer location

	ldy #$00

gets_l001:

	lda mos_PromptLine,y
	sta (tmp_zpgPt),y
	beq gets_end
	iny
	bne gets_l001

gets_end:

	lda tmp_zpgPt
	ldx tmp_zpgPt+1
	rts

.endproc

.proc _getchar: near

.segment "CODE"

    jmp getcharacter
	;jsr mos_CallGetCh
	;ldx #$00
	;rts

.endproc

.proc _getc: near

.segment "CODE"

    jmp getcharacter
	;jsr mos_CallGetCh
	;ldx #$00
	;rts

.endproc

.proc _fgetc: near

.segment "CODE"

    jmp getcharacter
	;jsr mos_CallGetCh
	;ldx #$00
	;rts

.endproc

.proc _kbhit: near

.segment "CODE"

	jsr mos_KbHit
	ldx #$00
	rts

.endproc

.segment "CODE"

getcharacter:

    jsr mos_CallGetCh
    ldx #$00
    rts
