;-----------------------------------------------------------------------------
; File:	mkhbcos_ml.h
; Purpose:	Declarations and definitions for MKHBCOS.
;           Include this file in assembly source files of programs that run on
;           MKHBCOS.
;
; Author:	Marek Karcz
; Created:	2/11/2018
;
; Revision history:
;
; 2/18/2018
;   Removed hardcoded definition of 'sp' variable.
;   Added .include "zeropage.inc" to import ZP objects instead.
;
; 3/7/2018
;   This header file is now included while compiling firmware as well as
;   applications running on the system.
;
; 3/8/2018
;   Added entries in kernel jump table.
;
;-----------------------------------------------------------------------------
.ifndef MKHBCOS_ML_INC
.define MKHBCOS_ML_INC

.include "zeropage.inc"     ; import zero page objects

.import   __IO0_START__,    __IO0_SIZE__
.import   __IO1_START__,    __IO1_SIZE__
.import   __IO2_START__,    __IO2_SIZE__
.import   __IO3_START__,    __IO3_SIZE__
.import   __IO4_START__,    __IO4_SIZE__
.import   __IO5_START__,    __IO5_SIZE__
.import   __IO6_START__,    __IO6_SIZE__
.import   __IO7_START__,    __IO7_SIZE__

; MOS registers and definitions.
.define     Timer64Hz       $E2     ; 4-byte counter
.define     RamBankNum      $E6     ; RAM bank# register
.define     DetectedDev     $E7     ; detected devices bit flags
.define     UartRxInPt      $F2     ; beg. or UART RX queue
.define     UartRxOutPt     $F3     ; end of UART RX queue
.define	    mos_PromptLine	$80
.define	    mos_PromptLen	$D0

.define 	mos_StrPtr	    $E0
.define		tmp_zpgPt		$F6
.define		IOBase			$C000
;.define 	RTC				IOBase+256
.define     RTC             __IO1_START__

.define 	DSCALADDR		RTC     ; port for accessing RTC reg. address
.define		DSCALDATA		RTC+1   ; port for accessing RTC reg. data

; 2/18/2018 - this should not be hardcoded but imported.
;             When I changed ZP start, the statement below caused problems
;             for runtime. I added .include "zeropage.inc" instead.
;.define		sp				$20     ; stack pointer (CC65)

; RTC shadow registers in RAM
RegB	=	tmp_zpgPt
RegA	=	tmp_zpgPt+1
RegXB	=	tmp_zpgPt+2
RegXA	=	tmp_zpgPt+3
RegC	=	tmp_zpgPt+4
Temp	=	tmp_zpgPt+5
BankNum =   tmp_zpgPt+6
RamAddr =   tmp_zpgPt+7
RamVal  =   tmp_zpgPt+8

ExtRamAddr = $50
ExtRamPort = $53

; DS RTC registers mask bits

; reg. A
DSC_REGA_UIP	=	%10000000
DSC_REGA_DV2	=	%01000000
DSC_REGA_DV1	=	%00100000
DSC_REGA_DV0	=	%00010000
DSC_REGA_RS3	=	%00001000
DSC_REGA_RS2	=	%00000100
DSC_REGA_RS1	=	%00000010
DSC_REGA_RS0	=	%00000001
; aliases
DSC_REGA_CTDWN	=	DSC_REGA_DV2
DSC_REGA_OSCEN	=	DSC_REGA_DV1
DSC_REGA_BSEL	=	DSC_REGA_DV0
DSC_REGA_BANK0	=	$EF
DSC_REGA_BANK1	=	$10

; reg. B
DSC_REGB_SET		=	%10000000
DSC_REGB_PIE		=	%01000000
DSC_REGB_AIE		=	%00100000
DSC_REGB_UIE		=	%00010000
DSC_REGB_SQWE		=	%00001000
DSC_REGB_DM			=	%00000100
DSC_REGB_24o12	    =	%00000010
DSC_REGB_DSE		=	%00000001

; aliases

DSC_REGB_UNSET	=	%01111111

; Devices detected flags.
DEVPRESENT_RTC      =   %10000000
DEVPRESENT_NORTC    =   %01111111
DEVPRESENT_EXTRAM   =   %01000000
DEVPRESENT_NOEXTRAM =   $10111111
DEVPRESENT_BANKRAM  =   %00100000
DEVPRESENT_NOBRAM   =   %11011111
DEVPRESENT_UART     =   %00010000
DEVPRESENT_NOUART   =   %11101111
;DEVNOEXTRAM         =   %10011111   ; alias for
                                    ; DEVPRESENT_NOEXTRAM & DEVPRESENT_NOBRAM
DEVNOEXTRAM         =   DEVPRESENT_NOEXTRAM & DEVPRESENT_NOBRAM

; Kernel jump table.
.define     mos_KbHit           $FFCC
.define     mos_BankedRamSel    $FFCF
.define     mos_DS1685Init      $FFD2
.define     mos_DS1685ReadClock $FFD5
.define     mos_DS1685SetClock  $FFD8
.define     mos_DS1685SetTime   $FFDB
.define     mos_DS1685StoreRam  $FFDE
.define     mos_DS1685ReadRam   $FFE1
.define     mos_ReadMem	        $FFE4
.define     mos_WriteMem	    $FFE7
.define     mos_Execute	        $FFEA
.define		mos_CallGetCh	    $FFED
.define		mos_CallPutCh	    $FFF0
.define		mos_CallGets	    $FFF3
.define		mos_CallPuts	    $FFF6
.define     mos_MemInit         $FFC9
.define     mos_MemCpy          $FFC6
.define     mos_RamBank         $FFC3
.define     mos_PrnDtTm         $FFC0
.define     mos_SetDtTm         $FFBD

.endif
