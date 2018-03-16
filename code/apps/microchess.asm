;***********************************************************************
;
;  MicroChess (c) 1996-2002 Peter Jennings, peterj@benlo.com
;
;***********************************************************************
; I have been given permission to distribute this program by the
; author and copyright holder, Peter Jennings.  Please get his
; permission if you wish to re-distribute a modified copy of
; this file to others.  He specifically requested that his
; copyright notice be included in the source and binary images.
; Thanks!
;
; 1/14/2012
; 		Modified by Marek Karcz to run on MKHBC-8-Rx under MKHBCOS
;       (derivative of M.O.S. by Scott Chidester)
;
; 3/8..11/2018
;   Relocated to $0B00.
;   Change in implementation of banner and board paint flags.
;   Include header for mkhbcos.
;
; 3/15/2018
;   Attempt at fixing bug #1:
;   Experimental code. Instead of disabling RTC periodic interrupt, I mask
;   the interrupt for the time when algorithm swaps stacks.
;   I also relocated some ZP addresses. If this doesn't help with board
;   corruption issue after computer move, I will go back to previous version
;   which disables periodic interrupts from RTC.
;
; 3/16/2018
;   Masking IRQ and relocating some ZP addresses alone didn't seem to work.
;   In the last desperate attempt to make microchess working, I moved the 2-nd
;   stack pointer 16 bytes away from the 1-st stack. This in addition to
;   masking interrupts while program is swapping stacks seems to have done the
;   trick. Not only I played 2 long games without board corruption. Microchess
;   gave me good run for my money too. I won 1-st game and lost 2-nd one.
;   I will test this code some more, to be sure, but I am checking it in to
;   source code repository as a good candidate for stable version.
;
;-----------------------------------------------------------------------------
; BUGS
;
; 1) There is a problem, program doesn't work properly if the RTC periodic
;    interrupts are enabled.
;   After several moves the board state is not consistent with the history of
;   the moves. It gets corrupt. (pieces disappear, move to different locations
;   all by themselves, etc.)
;   I noticed so far that when I take out BRAM/RTC card so that RTC service
;   interrupt becomes inactive and there are no periodic interrupts from RTC,
;   this problem doesn't occur. So I disable periodic interrupts from RTC
;   and re-enable them at exit. Not an elegant solution and one day I may need
;   to fix whatever the root cause of this conflict is.
;   STATUS: Possibly fixed, needs more testing.
;
;-----------------------------------------------------------------------------
;

; M.O.S. API defines (kernal)
.include "mkhbcos_ml.inc"

; page zero variables
;
BOARD   =	$50
BK      =	$60
PIECE   =	$B0
SQUARE  =	$B1
SP2     =	$B2
SP1     =	$B3
INCHEK  =	$B4
STATE   =	$B5
MOVEN   =	$B6
REV		=   $B7
OMOVE   =	$2C
WCAP0   =	$2D
COUNT   =	$2E
BCAP2   =	$2E
WCAP2   =	$2F
BCAP1   =	$30
WCAP1   =	$31
BCAP0   =	$32
MOB     =	$33
MAXC    =	$34
CC      =	$35
PCAP    =	$36
BMOB    =	$33
BMAXC   =	$34
BMCC    =	$35 		; was BCC (TASS doesn't like it as a label)
BMAXP   =	$36
XMAXC   =	$38
WMOB    =	$3B
WMAXC   =	$3C
WCC     =	$3D
WMAXP   =	$3E
PMOB    =	$3F
PMAXC   =	$40
PCC     =	$41
PCP     =	$42
OLDKY   =	$43
BESTP   =	$4B
BESTV   =	$4A
BESTM   =	$49
DIS1    =	$4B
DIS2    =	$4A
DIS3    =	$49
temp    =   $4C
PAINTFL =   $10
SAVREG1 =   $11
SAVREG2 =   $12
SAVREG3 =   $13
SAVSTK1 =   $14
SAVSTK2 =   $15
SAVDEVR =   $16

PAINT_BOARD     = %10000000
PAINT_BANNER    = %01000000

.segment "CODE"

        ; save current return address
        SEI
        PLA
        STA    SAVSTK1
        PLA
        STA    SAVSTK2
        CLI
        ; initialize delay (count down from $FFFF)
        LDA     #$ff
        STA     SAVREG1
        STA     SAVREG2
        ; disable RTC service in ISR
;        LDA     DetectedDev
;        STA     SAVDEVR
;        AND     #DEVPRESENT_RTC
;        BEQ     ST01    ; no need to do anything if RTC is not present
        ; RTC is present : disable periodic interrupts from it
        ; (A temporary hack to work around a bug in MKHBCOS's ISR)
;        jsr     mos_RTCDisablePIE
;        SEI
;ST00:   ; make MKHBCOS think that RTC is not present
;        LDA     SAVDEVR
;        AND     #DEVPRESENT_NORTC
;        STA     DetectedDev
;        CLI
ST01:   ; count down from $FFFF to $0000
        ; this delay is needed when working with PropTermMK as terminal device
        DEC     SAVREG1
        BNE     ST01
        DEC     SAVREG2
        BNE     ST01
        ; flush any input characters in buffer
ST02:
        JSR     mos_KbHit
        BEQ     INITFLAGS       ; no keypress in buffer, proceed
        JSR     mos_CallGetCh   ; consume character
        BNE     ST02            ; check again until buffer empty
INITFLAGS:
        LDA     #PAINT_BOARD    ; set both flags to paint
        ORA     #PAINT_BANNER   ; board and copyright banner
        STA     PAINTFL
        LDA     #$00            ; REVERSE TOGGLE
        STA     REV
        ;JSR     Init_6551
CHESS:  CLD
        SEI
        ; initialize 2 stacks
        LDX    #$FF
        TXS
        ;LDX    #$C8
        LDX    #$B8     ; moved the 2-nd stack a bit further apart from
        STX    SP2      ; the 1-st one
        CLI
;
;       I/O routines
;
OUT:    JSR    POUT       ; DISPLAY AND
        JSR    KIN        ; GET INPUT   *** my routine waits for a keypress
        CMP    #$43        ; [C]
        BNE    NOSET       ; SET UP
        LDA    PAINTFL
        ORA    #PAINT_BOARD
        STA    PAINTFL
        LDX    #$1F        ; BOARD
WHSET:  LDA    SETW,X      ; FROM
        STA    BOARD,X     ; SETW
        DEX
        BPL    WHSET
        LDX    #$1B        ; *ADDED
        STX    OMOVE       ; INITS TO $FF
        LDA    #$CC        ; Display CCC
        BNE    CLDSP
;
NOSET:  CMP    #$45        ; [E]
        BNE    NOREV       ; REVERSE
        LDA    PAINTFL
        ORA    #PAINT_BOARD
        STA    PAINTFL
        JSR    REVERSE     ; BOARD IS
        SEC
        LDA    #$01
        SBC    REV
        STA    REV          ; TOGGLE REV FLAG
        LDA    #$EE         ; IS
        BNE    CLDSP
;
NOREV:  CMP    #$40         ; [P]
        BNE    NOGO         ; PLAY CHESS
        LDA    PAINTFL
        ORA    #PAINT_BOARD
        STA    PAINTFL
        JSR    GO
CLDSP:  STA    DIS1         ; DISPLAY
        STA    DIS2         ; ACROSS
        STA    DIS3         ; DISPLAY
        BNE    CHESS
;
NOGO:   CMP    #$0D         ; [Enter]
        BNE    NOMV         ; MOVE MAN
        PHA
        LDA    PAINTFL
        ORA    #PAINT_BOARD
        STA    PAINTFL
        PLA
        JSR    MOVE         ; AS ENTERED
        JMP    DISP
NOMV:   CMP    #$41         ; [Q] ***Added to allow game exit***
        BEQ    DONE         ; quit the game, exit back to system.
        pha
        lda    PAINTFL
        and    #~PAINT_BOARD
        sta    PAINTFL
        pla
        JMP    INPUT        ; process move
DONE:   ; restore original return address from before stacks initialization
        SEI    ; mask (disable) interrupts
        LDA    SAVSTK2
        PHA
        LDA    SAVSTK1
        PHA
        ; restore original detected devices flags
;        LDA    SAVDEVR
;        STA    DetectedDev
;        AND    #DEVPRESENT_RTC
;        BEQ    DONE01       ; no need to do anything if RTC is not present
        ; RTC is present : re-enable periodic interrupts from RTC
;        jsr     mos_RTCEnablePIE
;DONE01:
        CLI     ; enable interrupts
        ; return to originally saved return address
        RTS
;
;       THE ROUTINE JANUS DIRECTS THE
;       ANALYSIS BY DETERMINING WHAT
;       SHOULD OCCUR AFTER EACH MOVE
;       GENERATED BY GNM
;
;
;
JANUS:  LDX    STATE
        BMI    NOCOUNT
;
;       THIS ROUTINE COUNTS OCCURRENCES
;       IT DEPENDS UPON STATE TO INDEX
;       THE CORRECT COUNTERS
;
COUNTS: LDA    PIECE
        BEQ    OVER             ; IF STATE=8
        CPX    #$08             ; DO NOT COUNT
        BNE    OVER             ; BLK MAX CAP
        CMP    BMAXP            ; MOVES FOR
        BEQ    XRT              ; WHITE
;
OVER:   INC    MOB,X            ; MOBILITY
        CMP    #$01             ;  + QUEEN
        BNE    NOQ              ; FOR TWO
        INC    MOB,X
;
NOQ:    BVC    NOCAP
        LDY    #$0F             ; CALCULATE
        LDA    SQUARE           ; POINTS
ELOOP:  CMP    BK,Y             ; CAPTURED
        BEQ    FOUN             ; BY THIS
        DEY            ; MOVE
        BPL    ELOOP
FOUN:   LDA    POINTS,Y
        CMP    MAXC,X
        BCC    LESS             ; SAVE IF
        STY    PCAP,X           ; BEST THIS
        STA    MAXC,X           ; STATE
;
LESS:   CLC
        PHP            ; ADD TO
        ADC    CC,X             ; CAPTURE
        STA    CC,X             ; COUNTS
        PLP
;
NOCAP:  CPX    #$04
        BEQ    ON4
        BMI    TREE             ;(=00 ONLY)
XRT:    RTS
;
;      GENERATE FURTHER MOVES FOR COUNT
;      AND ANALYSIS
;
ON4:    LDA    XMAXC            ; SAVE ACTUAL
        STA    WCAP0            ; CAPTURE
        LDA    #$00             ; STATE=0
        STA    STATE
        JSR    MOVE             ; GENERATE
        JSR    REVERSE          ; IMMEDIATE
        JSR    GNMZ             ; REPLY MOVES
        JSR    REVERSE
;
        LDA    #$08             ; STATE=8
        STA    STATE            ; GENERATE
;        JSR    OHM             ; CONTINUATION
        JSR    UMOVE            ; MOVES
;
        JMP    STRATGY          ; FINAL EVALUATION
NOCOUNT:CPX    #$F9
        BNE    TREE
;
;      DETERMINE IF THE KING CAN BE
;      TAKEN, USED BY CHKCHK
;
        LDA    BK               ; IS KING
        CMP    SQUARE           ; IN CHECK?
        BNE    RETJ             ; SET INCHEK=0
        LDA    #$00             ; IF IT IS
        STA    INCHEK
RETJ:   RTS
;
;      IF A PIECE HAS BEEN CAPTURED BY
;      A TRIAL MOVE, GENERATE REPLIES &
;      EVALUATE THE EXCHANGE GAIN/LOSS
;
TREE:   BVC    RETJ              ; NO CAP
        LDY    #$07               ; (PIECES)
        LDA    SQUARE
LOOPX:  CMP    BK,Y
        BEQ    FOUNX
        DEY
        BEQ    RETJ              ; (KING)
        BPL    LOOPX             ; SAVE
FOUNX:  LDA    POINTS,Y          ; BEST CAP
        CMP    BCAP0,X           ; AT THIS
        BCC    NOMAX             ; LEVEL
        STA    BCAP0,X
NOMAX:  DEC    STATE
        LDA    #$FB              ; IF STATE=FB
        CMP    STATE             ; TIME TO TURN
        BEQ    UPTREE            ; AROUND
        JSR    GENRM             ; GENERATE FURTHER
UPTREE: INC    STATE             ; CAPTURES
        RTS
;
;      THE PLAYER'S MOVE IS INPUT
;
INPUT:  CMP    #$08              ; NOT A LEGAL
        BCS    ERROR             ; SQUARE #
        JSR    DISMV
DISP:   LDX    #$1F
SEARCH: LDA    BOARD,X
        CMP    DIS2
        BEQ    HERE              ; DISPLAY
        DEX                      ; PIECE AT
        BPL    SEARCH            ; FROM
HERE:   STX    DIS1              ; SQUARE
        STX    PIECE
ERROR:  JMP    CHESS
;
;      GENERATE ALL MOVES FOR ONE
;      SIDE, CALL JANUS AFTER EACH
;      ONE FOR NEXT STE?
;
;
GNMZ:   LDX    #$10            ; CLEAR
GNMX:   LDA    #$00            ; COUNTERS
CLEAR:  STA    COUNT,X
        DEX
        BPL    CLEAR
;
GNM:    LDA    #$10            ; SET UP
        STA    PIECE            ; PIECE
NEWP:   DEC    PIECE            ; NEW PIECE
        BPL    NEX               ; ALL DONE?
        RTS            ; #NAME?
;
NEX:    JSR    RESET            ; READY
        LDY    PIECE            ; GET PIECE
        LDX    #$08
        STX    MOVEN            ; COMMON START
        CPY    #$08            ; WHAT IS IT?
        BPL    PAWN              ; PAWN
        CPY    #$06
        BPL    KNIGHT            ; KNIGHT
        CPY    #$04
        BPL    BISHOP           ; BISHOP
        CPY    #$01
        BEQ    QUEEN             ; QUEEN
        BPL    ROOK              ; ROOK
;
KING:   JSR    SNGMV             ; MUST BE KING!
        BNE    KING              ; MOVES
        BEQ    NEWP              ; 8 TO 1
QUEEN:  JSR    LINE
        BNE    QUEEN             ; MOVES
        BEQ    NEWP              ; 8 TO 1
;
ROOK:   LDX    #$04
        STX    MOVEN            ; MOVES
AGNR:   JSR    LINE              ; 4 TO 1
        BNE    AGNR
        BEQ    NEWP
;
BISHOP: JSR    LINE
        LDA    MOVEN            ; MOVES
        CMP    #$04               ; 8 TO 5
        BNE    BISHOP
        BEQ    NEWP
;
KNIGHT: LDX    #$10
        STX    MOVEN            ; MOVES
AGNN:   JSR    SNGMV             ; 16 TO 9
        LDA    MOVEN
        CMP    #$08
        BNE    AGNN
        BEQ    NEWP
;
PAWN:   LDX    #$06
        STX    MOVEN
P1:     JSR    CMOVE             ; RIGHT CAP?
        BVC    P2
        BMI    P2
        JSR    JANUS             ; YES
P2:     JSR    RESET
        DEC    MOVEN            ; LEFT CAP?
        LDA    MOVEN
        CMP    #$05
        BEQ    P1
P3:     JSR    CMOVE             ; AHEAD
        BVS    NEWP              ; ILLEGAL
        BMI    NEWP
        JSR    JANUS
        LDA    SQUARE           ; GETS TO
        AND    #$F0               ; 3RD RANK?
        CMP    #$20
        BEQ    P3                ; DO DOUBLE
        JMP    NEWP
;
;      CALCULATE SINGLE STEP MOVES
;      FOR K,N
;
SNGMV:  JSR    CMOVE            ; CALC MOVE
        BMI    ILL1               ; -IF LEGAL
        JSR    JANUS           ; -EVALUATE
ILL1:   JSR    RESET
        DEC    MOVEN
        RTS
;
;     CALCULATE ALL MOVES DOWN A
;     STRAIGHT LINE FOR Q,B,R
;
LINE:   JSR    CMOVE             ; CALC MOVE
        BCC    OVL                ; NO CHK
        BVC    LINE        ; NOCAP
OVL:    BMI    ILL             ; RETURN
        PHP
        JSR    JANUS             ; EVALUATE POSN
        PLP
        BVC    LINE              ; NOT A CAP
ILL:    JSR    RESET             ; LINE STOPPED
        DEC    MOVEN             ; NEXT DIR
        RTS
;
;      EXCHANGE SIDES FOR REPLY
;      ANALYSIS
;
REVERSE:LDX    #$0F
ETC:    SEC
        LDY    BK,X               ; SUBTRACT
        LDA    #$77               ; POSITION
        SBC    BOARD,X            ; FROM 77
        STA    BK,X
        STY    BOARD,X            ; AND
        SEC
        LDA    #$77               ; EXCHANGE
        SBC    BOARD,X            ; PIECES
        STA    BOARD,X
        DEX
        BPL    ETC
        RTS
;
;       CMOVE CALCULATES THE TO SQUARE
;       USING SQUARE AND THE MOVE
;       TABLE  FLAGS SET AS FOLLOWS:
;       N#NAME?    MOVE
;       V#NAME?    (LEGAL UNLESS IN CR)
;       C#NAME?    BECAUSE OF CHECK
;       [MY &THANKS TO JIM BUTTERFIELD
;        WHO WROTE THIS MORE EFFICIENT
;        VERSION OF CMOVE]
;
CMOVE:  LDA    SQUARE          ; GET SQUARE
        LDX    MOVEN           ; MOVE POINTER
        CLC
        ADC    MOVEX,X         ; MOVE LIST
        STA    SQUARE          ; NEW POS'N
        AND    #$88
        BNE    ILLEGAL         ; OFF BOARD
        LDA    SQUARE
;
        LDX    #$20
LOOP:   DEX                    ; IS TO
        BMI    NO              ; SQUARE
        CMP    BOARD,X         ; OCCUPIED?
        BNE    LOOP
;
        CPX    #$10            ; BY SELF?
        BMI    ILLEGAL
;
        LDA    #$7F            ; MUST BE CAP!
        ADC    #$01            ; SET V FLAG
        BVS    SPX             ; (JMP)
;
NO:     CLV                    ; NO CAPTURE
;
SPX:    LDA    STATE           ; SHOULD WE
        BMI    RETL            ; DO THE
        CMP    #$08            ; CHECK CHECK?
        BPL    RETL
;
;        CHKCHK REVERSES SIDES
;       AND LOOKS FOR A KING
;       CAPTURE TO INDICATE
;       ILLEGAL MOVE BECAUSE OF
;       CHECK  SINCE THIS IS
;       TIME CONSUMING, IT IS NOT
;       ALWAYS DONE
;
CHKCHK: PHA                      ; STATE  #392
        PHP
        LDA    #$F9
        STA    STATE             ; GENERATE
        STA    INCHEK            ; ALL REPLY
        JSR    MOVE              ; MOVES TO
        JSR    REVERSE           ; SEE IF KING
        JSR    GNM               ; IS IN
        JSR    RUM               ; CHECK
        PLP
        PLA
        STA    STATE
        LDA    INCHEK
        BMI    RETL              ; NO - SAFE
        SEC                      ; YES - IN CHK
        LDA    #$FF
        RTS
;
RETL:   CLC                      ; LEGAL
        LDA    #$00              ; RETURN
        RTS
;
ILLEGAL:LDA    #$FF
        CLC                      ; ILLEGAL
        CLV                      ; RETURN
        RTS
;
;       REPLACE PIECE ON CORRECT SQUARE
;
RESET:  LDX    PIECE          ; GET LOGAT
        LDA    BOARD,X        ; FOR PIECE
        STA    SQUARE         ; FROM BOARD
        RTS
;
;
;
GENRM:  JSR    MOVE           ; MAKE MOVE
GENR2:  JSR    REVERSE        ; REVERSE BOARD
        JSR    GNM            ; GENERATE MOVES
RUM:    JSR    REVERSE        ; REVERSE BACK
;
;       ROUTINE TO UNMAKE A MOVE MADE BY
;      MOVE
;
UMOVE:  SEI            ; while program is swapping stacks mask IRQ
        TSX            ; UNMAKE MOVE
        STX    SP1
        LDX    SP2     ; EXCHANGE
        TXS            ; STACKS
        PLA            ; MOVEN
        STA    MOVEN
        PLA            ; CAPTURED
        STA    PIECE   ; PIECE
        TAX
        PLA            ; FROM SQUARE
        STA    BOARD,X
        PLA            ; PIECE
        TAX
        PLA            ; TO SOUARE
        STA    SQUARE
        STA    BOARD,X
        JMP    STRV
;
;       THIS ROUTINE MOVES PIECE
;       TO SQUARE, PARAMETERS
;       ARE SAVED IN A STACK TO UNMAKE
;       THE MOVE LATER
;
MOVE:   SEI            ; mask IRQ while swapping stacks to avoid corruption
        TSX
        STX    SP1     ; SWITCH
        LDX    SP2     ; STACKS
        TXS
        LDA    SQUARE
        PHA            ; TO SQUARE
        TAY
        LDX    #$1F
CHECK:  CMP    BOARD,X ; CHECK FOR
        BEQ    TAKE    ; CAPTURE
        DEX
        BPL    CHECK
TAKE:   LDA    #$CC
        STA    BOARD,X
        TXA            ; CAPTURED
        PHA            ; PIECE
        LDX    PIECE
        LDA    BOARD,X
        STY    BOARD,X ; FROM
        PHA            ; SQUARE
        TXA
        PHA            ; PIECE
        LDA    MOVEN
        PHA            ; MOVEN
STRV:   TSX
        STX    SP2     ; SWITCH
        LDX    SP1     ; STACKS
        TXS            ; BACK
        CLI            ; unmask interrupts now that 1-st stack is restored
        RTS
;
;       CONTINUATION OF SUB STRATGY
;       -CHECKS FOR CHECK OR CHECKMATE
;       AND ASSIGNS VALUE TO MOVE
;
CKMATE: LDY    BMAXC            ; CAN BLK CAP
        CPX    POINTS           ; MY KING?
        BNE    NOCHEK
        LDA    #$00             ; GULP!
        BEQ    RETV             ; DUMB MOVE!
;
NOCHEK: LDX    BMOB             ; IS BLACK
        BNE    RETV             ; UNABLE TO
        LDX    WMAXP            ; MOVE AND
        BNE    RETV             ; KING IN CH?
        LDA    #$FF             ; YES! MATE
;
RETV:   LDX    #$04             ; RESTORE
        STX    STATE            ; STATE=4
;
;       THE VALUE OF THE MOVE (IN ACCU)
;       IS COMPARED TO THE BEST MOVE AND
;       REPLACES IT IF IT IS BETTER
;
PUSH:   CMP    BESTV            ; IS THIS BEST
        BCC    RETP             ; MOVE SO FAR?
        BEQ    RETP
        STA    BESTV            ; YES!
        LDA    PIECE            ; SAVE IT
        STA    BESTP
        LDA    SQUARE
        STA    BESTM            ; FLASH DISPLAY
RETP:   LDA    #'.'        ; print ... instead of flashing disp
        Jmp    syschout    ; print . and return
;
;       MAIN PROGRAM TO PLAY CHESS
;       PLAY FROM OPENING OR THINK
;
GO:     LDX    OMOVE            ; OPENING?
        BMI    NOOPEN           ; -NO   *ADD CHANGE FROM BPL
        LDA    DIS3             ; -YES WAS
        CMP    OPNING,X         ; OPPONENT'S
        BNE    END              ; MOVE OK?
        DEX
        LDA    OPNING,X         ; GET NEXT
        STA    DIS1             ; CANNED
        DEX                     ; OPENING MOVE
        LDA    OPNING,X
        STA    DIS3             ; DISPLAY IT
        DEX
        STX    OMOVE            ; MOVE IT
        BNE    MV2              ; (JMP)
;
END:    LDA    #$FF             ; *ADD - STOP CANNED MOVES
        STA    OMOVE            ; FLAG OPENING
NOOPEN: LDX    #$0C             ; FINISHED
        STX    STATE            ; STATE=C
        STX    BESTV            ; CLEAR BESTV
        LDX    #$14             ; GENERATE P
        JSR    GNMX             ; MOVES
;
        LDX    #$04             ; STATE=4
        STX    STATE            ; GENERATE AND
        JSR    GNMZ             ; TEST AVAILABLE
;
;    MOVES
;
        LDX    BESTV            ; GET BEST MOVE
        CPX    #$0F             ; IF NONE
        BCC    MATE             ; OH OH!
;
MV2:    LDX    BESTP            ; MOVE
        LDA    BOARD,X          ; THE
        STA    BESTV            ; BEST
        STX    PIECE            ; MOVE
        LDA    BESTM
        STA    SQUARE           ; AND DISPLAY
        JSR    MOVE             ; IT
        JMP    CHESS
;
MATE:   LDA    #$FF             ; RESIGN
        RTS                     ; OR STALEMATE
;
;       SUBROUTINE TO ENTER THE
;       PLAYER'S MOVE
;
DISMV:  LDX    #$04    ; ROTATE
DROL:   ASL    DIS3    ; KEY
        ROL    DIS2    ; INTO
        DEX            ; DISPLAY
        BNE    DROL    ;
        ORA    DIS3
        STA    DIS3
        STA    SQUARE
        RTS
;
;       THE FOLLOWING SUBROUTINE ASSIGNS
;       A VALUE TO THE MOVE UNDER
;       CONSIDERATION AND RETURNS IT IN
;       THE ACCUMULATOR
;

STRATGY:CLC
        LDA    #$80
        ADC    WMOB            ; PARAMETERS
        ADC    WMAXC           ; WITH WHEIGHT
        ADC    WCC             ; OF O25
        ADC    WCAP1
        ADC    WCAP2
        SEC
        SBC    PMAXC
        SBC    PCC
        SBC    BCAP0
        SBC    BCAP1
        SBC    BCAP2
        SBC    PMOB
        SBC    BMOB
        BCS    POS             ; UNDERFLOW
        LDA    #$00            ; PREVENTION
POS:    LSR
        CLC                    ; **************
        ADC    #$40
        ADC    WMAXC           ; PARAMETERS
        ADC    WCC             ; WITH WEIGHT
        SEC                    ; OF 05
        SBC    BMAXC
        LSR                    ; **************
        CLC
        ADC    #$90
        ADC    WCAP0           ; PARAMETERS
        ADC    WCAP0           ; WITH WEIGHT
        ADC    WCAP0           ; OF 10
        ADC    WCAP0
        ADC    WCAP1
        SEC                    ; [UNDER OR OVER-
        SBC    BMAXC           ; FLOW MAY OCCUR
        SBC    BMAXC           ; FROM THIS
        SBC    BMCC            ; SECTION]
        SBC    BMCC
        SBC    BCAP1
        LDX    SQUARE          ; ***************
        CPX    #$33
        BEQ    POSN            ; POSITION
        CPX    #$34            ; BONUS FOR
        BEQ    POSN            ; MOVE TO
        CPX    #$22            ; CENTRE
        BEQ    POSN            ; OR
        CPX    #$25            ; OUT OF
        BEQ    POSN            ; BACK RANK
        LDX    PIECE
        BEQ    NOPOSN
        LDY    BOARD,X
        CPY    #$10
        BPL    NOPOSN
POSN:   CLC
        ADC    #$02
NOPOSN: JMP    CKMATE          ; CONTINUE


;-----------------------------------------------------------------
; The following routines were added to allow text-based board
; display over a standard character I/O.
;
POUT:   lda    PAINTFL
        and    #PAINT_BOARD
        bne    POUT0
        rts
POUT0:  jsr    POUT9       ; print CRLF
        jsr    POUT13      ; print copyright
        JSR    POUT10      ; print column labels
        LDY    #$00        ; init board location
        JSR    POUT5       ; print board horz edge
POUT1:  LDA    #'|'        ; print vert edge
        JSR    syschout    ; PRINT ONE ASCII CHR - SPACE
        LDX    #$1F
POUT2:  TYA                ; scan the pieces for a location match
        CMP    BOARD,X     ; match found?
        BEQ    POUT4       ; yes; print the piece's color and type
        DEX                ; no
        BPL    POUT2       ; if not the last piece, try again
        tya                ; empty square
        and    #$01        ; odd or even column?
        sta    temp        ; save it
        tya                ; is the row odd or even
        lsr                ; shift column right 4 spaces
        lsr                ;
        lsr                ;
        lsr                ;
        and    #$01        ; strip LSB
        clc                ;
        adc    temp        ; combine row & col to determine square color
        and    #$01        ; is board square white or blk?
        bne    POUT25      ; white, print space
        lda    #'*'        ; black, print *

        .byte  $2c         ; used to skip over LDA #$20
		;jmp    POUT25A

POUT25: LDA    #$20        ; ASCII space
POUT25A:JSR    syschout    ; PRINT ONE ASCII CHR - SPACE
        JSR    syschout    ; PRINT ONE ASCII CHR - SPACE
POUT3:  INY            ;
        TYA            ; get row number
        AND    #$08        ; have we completed the row?
        BEQ    POUT1       ; no, do next column
        LDA    #'|'        ; yes, put the right edge on
        JSR    syschout    ; PRINT ONE ASCII CHR - |
        jsr    POUT12      ; print row number
        JSR    POUT9       ; print CRLF
        JSR    POUT5       ; print bottom edge of board
        CLC                ;
        TYA                ;
        ADC    #$08        ; point y to beginning of next row
        TAY                ;
        CPY    #$80        ; was that the last row?
        BEQ    POUT8       ; yes, print the LED values
        BNE    POUT1       ; no, do new row

POUT4:  LDA    REV        ; print piece's color & type
        BEQ    POUT41     ;
        LDA    cpl+16,X   ;
        BNE    POUT42     ;
POUT41: LDA    cpl,x      ;
POUT42: JSR    syschout   ;
        lda    cph,x      ;
        jsr    syschout   ;
        BNE    POUT3      ; branch always

POUT5:  TXA               ; print "-----...-----<crlf>"
        PHA
        LDX    #$19
        LDA    #'-'
POUT6:  JSR    syschout   ; PRINT ONE ASCII CHR - "-"
        DEX
        BNE    POUT6
        PLA
        TAX
        JSR    POUT9
        RTS

POUT8:  jsr    POUT10      ;
        LDA    BESTP
        JSR    syshexout   ; PRINT 1 BYTE AS 2 HEX CHRS
        LDA    #$20
        JSR    syschout    ; PRINT ONE ASCII CHR - SPACE
        LDA    BESTV
        JSR    syshexout   ; PRINT 1 BYTE AS 2 HEX CHRS
        LDA    #$20
        JSR    syschout    ; PRINT ONE ASCII CHR - SPACE
        LDA    DIS3
        JSR    syshexout   ; PRINT 1 BYTE AS 2 HEX CHRS

POUT9:  LDA    #$0D
        JSR    syschout    ; PRINT ONE ASCII CHR - CR
        LDA    #$0A
        JSR    syschout    ; PRINT ONE ASCII CHR - LF
        RTS

POUT10: ldx    #$00        ; print the column labels
POUT11: lda    #$20        ; 00 01 02 03 ... 07 <CRLF>
        jsr    syschout
        txa
        jsr    syshexout
        INX
        CPX    #$08
        BNE    POUT11
        BEQ    POUT9
POUT12: TYA
        and    #$70
        JSR    syshexout
        rts

; print banner, preserve registers A, X, Y
POUT13: stx    SAVREG1
		sta    SAVREG2
		sty    SAVREG3
        lda    PAINTFL
        and    #PAINT_BANNER
        beq    NOPRNBANN
	    lda    #<banner
        sta    mos_StrPtr
		lda    #>banner
		sta    mos_StrPtr+1
		jsr    mos_CallPuts
NOPRNBANN:
        lda    PAINTFL
        and    #~PAINT_BANNER
        sta    PAINTFL
		ldx    SAVREG1
		lda    SAVREG2
		ldy    SAVREG3
		rts

KIN:    LDA    #'?'
        JSR    syschout    ; PRINT ONE ASCII CHR - ?
        JSR    syskin      ; GET A KEYSTROKE FROM SYSTEM
        JSR    syschout    ; echo the character
        AND    #$4F        ; MASK 0-7, AND ALPHA'S
        RTS

syskin:
        jsr mos_CallGetCh
        rts
;
; output to OutPut Port
;
syschout:		; MKHBCOS: must preserve X and A before calling PutCh
        stx SAVREG1
        sta SAVREG2
        jsr mos_CallPutCh
        ldx SAVREG1
        lda SAVREG2
        rts

syshexout:     PHA                     ;  prints AA hex digits
               LSR                     ;  MOVE UPPER NIBBLE TO LOWER
               LSR                     ;
               LSR                     ;
               LSR                     ;
               JSR   PrintDig          ;
               PLA                     ;
PrintDig:
               STY   SAVREG3
               AND   #$0F              ;
               TAY                     ;
               LDA   Hexdigdata,Y      ;
               ldy   SAVREG3           ;
               jmp   syschout          ;

Hexdigdata:   .byte    "0123456789ABCDEF"
banner:       .byte    "MicroChess (c) 1996-2002 Peter Jennings, peterj@benlo.com"
              .byte    $0d, $0a, $00
cpl:          .byte    "WWWWWWWWWWWWWWWWBBBBBBBBBBBBBBBBWWWWWWWWWWWWWWWW"
cph:          .byte    "KQCCBBRRPPPPPPPPKQCCBBRRPPPPPPPP"
              .byte    $00
;
; end of added code
;
; BLOCK DATA

.segment "DATA"

;	.ORG $0920

SETW:       .byte     $03, $04, $00, $07, $02, $05, $01, $06
            .byte     $10, $17, $11, $16, $12, $15, $14, $13
            .byte     $73, $74, $70, $77, $72, $75, $71, $76
            .byte     $60, $67, $61, $66, $62, $65, $64, $63

MOVEX:      .byte     $00, $F0, $FF, $01, $10, $11, $0F, $EF, $F1
            .byte     $DF, $E1, $EE, $F2, $12, $0E, $1F, $21

POINTS:     .byte     $0B, $0A, $06, $06, $04, $04, $04, $04
            .byte     $02, $02, $02, $02, $02, $02, $02, $02

OPNING:     .byte     $99, $25, $0B, $25, $01, $00, $33, $25
            .byte     $07, $36, $34, $0D, $34, $34, $0E, $52
            .byte     $25, $0D, $45, $35, $04, $55, $22, $06
            .byte     $43, $33, $0F, $CC, $00, $00, $00, $00

;
;
; end of file
;
