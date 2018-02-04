; ADC, test decimal mode.
;
; NV-BDIZC
; ??1110??
;
; Expected results:
; 00 + 00 and C=0 gives 00 and N=0 V=0 Z=1 C=0 (3A)
; 79 + 00 and C=1 gives 80 and N=1 V=1 Z=0 C=0 (F8)
; 24 + 56 and C=0 gives 80 and N=1 V=1 Z=0 C=0 (F8)
; 93 + 82 and C=0 gives 75 and N=0 V=1 Z=0 C=1 (79)
; 89 + 76 and C=0 gives 65 and N=0 V=0 Z=0 C=1 (39)
; 89 + 76 and C=1 gives 66 and N=0 V=0 Z=0 C=1 (39)
; 80 + f0 and C=0 gives d0 and N=1 V=1 Z=0 C=1 (F9)
; 80 + fa and C=0 gives e0 and N=1 V=0 Z=0 C=1 (B9)
; 2f + 4f and C=0 gives 74 and N=0 V=0 Z=0 C=0 (38)
; 6f + 00 and C=1 gives 76 and N=0 V=0 Z=0 C=0 (38)

.segment "CODE"

RES=$8000

	.ORG $3000
	
	SED
	CLC
	LDA #$00
	ADC #$00
	STA RES
	PHP
	PLA
	STA RES+1
	SEC
	LDA #$79
	ADC #$00
	STA RES+2
	PHP
	PLA
	STA RES+3
	CLC
	LDA #$24
	ADC #$56
	STA RES+4
	PHP
	PLA
	STA RES+5
	CLC
	LDA #$93
	ADC #$82
	STA RES+6
	PHP
	PLA
	STA RES+7
	CLC
	LDA #$89
	ADC #$76
	STA RES+8
	PHP
	PLA
	STA RES+9
	SEC
	LDA #$89
	ADC #$76
	STA RES+10
	PHP
	PLA
	STA RES+11
	CLC
	LDA #$80
	ADC #$F0
	STA RES+12
	PHP
	PLA
	STA RES+13
	CLC
	LDA #$80
	ADC #$FA
	STA RES+14
	PHP
	PLA
	STA RES+15
	CLC
	LDA #$2F
	ADC #$4F
	STA RES+16
	PHP
	PLA
	STA RES+17
	SEC
	LDA #$6F
	ADC #$00
	STA RES+18
	PHP
	PLA
	STA RES+19
	CLD
	RTS
	