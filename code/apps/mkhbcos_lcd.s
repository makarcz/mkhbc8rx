;------------------------------------------------------------------
;
; File: 	mkhbcos_lcd.s
; Author:	Marek Karcz
; Purpose:	Implement's LCD 1602 module routines to be
;			called from C programs (cc65).
;
; Revision history:
;	2012-01-18:
;		Initial revision.
;       (NOTE: The LCD routines will eventually make their way
;              to EPROM as a part of firmware. At that time,
;              this file will be revised to call up the API
;              functions in the kernal table, instead of
;              being full implementation.)
;
;   2012-01-19:
;		Added lcdinitseq table.
;
;------------------------------------------------------------------

; M.O.S. API defines (kernal)

.define 	mos_StrPtr		$E0
.define		tmp_zpgPt		$F6
.define		IOBase			$c000
.define		IO3				IOBase+3*256
.define		LCDcmd			IO3
.define		LCDdat			IO3+1

Ct02	=	tmp_zpgPt+2
Reg01	=	tmp_zpgPt+4


.setcpu	"6502"
.import ldaxysp,pushax,popax,pusha,popa
.import incsp2

.define		sp				$20

; code

.export _lcd_busy,_lcd_init,_lcd_putchar,_lcd_clear,_lcd_line,_lcdinitseq
;,_read

.segment "CODE"

; wait for LCD module to finish operation

.proc _lcd_busy: near

l0:  
	lda LCDcmd          ; read from LCD register 0
    and #$80            ; check bit 7 (busy)
    bne l0				; loop until cleared
    rts

.endproc

; send initialization sequence to LCD module

.proc _lcd_init: near

.segment "CODE"

	sta tmp_zpgPt
	stx tmp_zpgPt+1
	ldy #$00
l1:
	lda (tmp_zpgPt),y	; read indirectly from address
    beq l2				; on zpg, if 0, seq. finished
	sta LCDcmd			; send byte to LCD command reg.
	jsr _lcd_busy		; wait for LCD to finish proc.
	iny					; take next code in seq.
	bne l1				; loop
l2:
	rts

.endproc

; send character to LCD module

.proc _lcd_putchar: near

.segment "CODE"

	sta LCDdat			; send character from A to LCD data reg.
	jsr _lcd_busy		; wait for LCD to finish processing
	rts

.endproc

; clear LCD module

.proc _lcd_clear: near

.segment "CODE"

	lda #$01			; clear screen code
	sta LCDcmd			; send to LCD command register
	ldx #$ff			; load delay counter
	ldy #$2f
	jsr Delay			; count to 0
	rts

.endproc

; set current text line
; $80 - line 1, $c0 - line 2

.proc _lcd_line: near

.segment "CODE"

	cmp #$80			; check if proper line address
	beq l3
	cmp #$c0
	bne l4				; wrong address, do nothing
l3:
	sta LCDcmd			; send line address to LCD command reg.
	ldx #$ff			; load delay counter
	ldy #$2f
	jsr Delay			; count to 0
l4:
	rts

.endproc

; helper procedures

.segment "CODE"

; Short delay.
; X = lo_delay, Y = hi_delay
; Counts down the 16-bit value to zero, then exits.

Delay:			

	stx Ct02
	sty Ct02+1
	stx Reg01
	sty Reg01+1

DelLoop:

	dec Ct02
	bne DelLoop
	lda Reg01
	sta Ct02
	dec Ct02+1
	bne DelLoop
	rts

_lcdinitseq:

	.byte $38,$0c,$06,$01,0

