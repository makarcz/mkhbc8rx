; This simple code tests the I/O decoding
; (and in intermediate way, memory decoding)
; circuits of my homebrew 6502 computer MKHBC-8-R1
; (revision 1, CPU + ROM + IO decoder).
; Prototype HW consists of clock generator, CPU,
; I/O and ROM decoder and I/O specific device
; addresses decoding and RAM.
; RAM is currently the base 32 kB ($0000 - $7FFF)
; enabled with A15 from uP bus with no upper 128kB
; banked memory nor video memory decoder yet implemented.
; 3 LEDs are connected to IO device decoder outputs
; IO0, IO1 and IO2. By sending memory write requests
; to them in a loop with delay we can observe LEDs 
; going on and off sequentially.

.segment "CODE"

IOSTART = $C000

IO0 = IOSTART
IO1 = IO0+256
IO2 = IO1+256
IO3 = IO2+256
IO4 = IO3+256
IO5 = IO4+256
IO6 = IO5+256
IO7 = IO6+256

COUNT = $0800
DELAY = $FFFF

.ORG $E000


NMIJUMP:

	jmp	NMIPROC
	
RESJUMP:

	jmp RESPROC
	
IRQJUMP:

	jmp IRQPROC

NMIPROC:
	
	rti	

IRQPROC:

	rti

RESPROC:

	cld
mainloop:
	lda #<DELAY
	sta COUNT
	lda #>DELAY
	sta COUNT+1
loop1:
	sta IO0
	dec COUNT
	bne loop1
	lda #<DELAY
	sta COUNT
	dec COUNT+1
	bne loop1
	lda #<DELAY
	sta COUNT
	lda #>DELAY
	sta COUNT+1
loop2:
	sta IO1
	dec COUNT
	bne loop2
	lda #<DELAY
	sta COUNT
	dec COUNT+1
	bne loop2
	lda #<DELAY
	sta COUNT
	lda #>DELAY
	sta COUNT+1
loop3:
	sta IO2
	dec COUNT
	bne loop3
	lda #<DELAY
	sta COUNT
	dec COUNT+1
	bne loop3
	jmp mainloop

.segment "VECT"

.ORG	$FFFA

.BYTE	<NMIJUMP, >NMIJUMP
.BYTE	<RESJUMP, >RESJUMP
.BYTE	<IRQJUMP, >IRQJUMP
