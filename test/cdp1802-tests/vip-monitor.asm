.. VIP operating system
.. recreated from the hex listing
.. and using information from VIPER Vol 1, Issue 3
.. by J.W. Wentworth's Interpretation of VIP
.. D. Hunter            7/11/14

.. Enter the VIP operating system by holding down the 'C' key when entering RUN
.. A hex address is entered followed by a command key
.. e.g. 0123 <cmd>
.. There are four commands possible:
.. Memory Read   = A
.. Memory Write  = 0    (enter the byte to write)
.. Tape Read     = B    (enter the number of pages to read)
.. Tape Write    = F    (enter the number of pages to write)
..
.. Execute a RESET (RUN OFF, RUN ON) to exit a command and start again
..
.. PORTS
..      VIDEO ON        INPUT  1
..      VIDEO OFF       OUTPUT 1
..      KEYPAD SCAN     OUTPUT 2
..      INPUT PORT      INPUT  3
..      OUTPUT PORT     OUTPUT 3
..      ROM MAP OFF     OUTPUT 4

.. Q
..      TAPE OUT
..      LED

.. EF PINS
..      VIDEO FRAME     EF1
..      TAPE IN         EF2
..      KEYBD           EF3
..      (OPEN)          EF4

STKTOP: EQU     #AF             .. top of stack is below register memory
LINE27: EQU     216             .. display starts at line 27, position 0
                                .. for 5 rows (27,28,29,30,31)
                                .. so the characters are at the bottom
                                .. of the screen
                                .. 27 * 8 = 216 = #D8
NXTCHR: EQU     217             .. line 27, position 1

.. REGISTER USAGE
.. R0 = DMA (video memory)
.. R1 = INTERRUPT
.. R2 = STACK POINTER
.. R3 = PC
.. R4 = DISPLAY CONTROL SUBROUTINE (#81DD)
.. R5 = 5 ROW DISPLAY SUBROUTINE (#81C6)
.. R6 = POINTER TO ADDRESSED BYTE ENTERED BY OPERATOR
.. R7 = GET BYTE FROM KEYPAD SUBROUTINE (#81BA)
..      R7.1 STORES BYTES FOR PROCESSING IN TAPE READS AND WRITES
..      R7.0 PARITY COUNTER FOR TAPE READS AND WRITES
.. R8 = R8.1 IS A TIMER FOR CHIP 8 PROGRAMS, TESTED EVERY TV SCREEN
..      R8.0 TIMER TESTED AND DECREMENTED EVERY TV SCREEN (60 HZ)
..      A TONE IS ON (Q SET) WHEN TIMER > 0
..      R8.0 TIMER ALSO CONTROLS DEBOUNCE FOR THE KEYBOARD
.. R9 = SPECIAL VALUE USED BY THE RANDOM NUMBER GENERATOR IN CHIP 8,
..      INCREMENTED ONCE PER TV SCREEN
..      R9.1 IS A TIMER FOR THE TAPE LEADER IN TAPE READS AND WRITES
..      R9.0 IS THE BIT COUNTER FOR TAPE READS AND WRITES
.. RA = DATA POINTER USED IN 5 ROW HEX DIGIT DISPLAY  -POINTS TO CHARACTER DATA
.. RB = RB.1 STORES DISPLAY PAGE UPPER ADDRESS FOR USE DURING THE INTERRUPT ROUTINE
..      RB.0 USED DURING THE INTERRUPT ROUTINE TO DECREMENT R8.1 W/O CHANGING DF
.. RC = KEYBOARD SCAN SUBROUTINE (#8195)
.. RD = DESTINATION POINTER TO BE DISPLAYED ON DISPLAY PAGE
.. RE = RE.1 STORES CONTROL DIGIT (A, 0, B OR F) FROM USER
..      RE.0 STORES THE MOST SIGNIFICANT DIGIT FOR THE KEYBOARD INPUT
.. RF = RF.1 STORES THE TIMER CONSTANT FOR THE TAPE WRITE
..      RF.0 IS USED FOR KEYBOARD SCANNING AND A COUNTER FOR THE 5 ROW DISPLAY

        ORG     #8000

COLD:   LDI     A.1(MON)        .. jump to MON from boot vector
        PHI     R2
        LDI     A.0(MON)
        PLO     R2
        SEX     R2              .. 2 -> X
        SEP     R2              .. 2 -> P
MON:    OUT     4               .. output 00 to bus to turn off the boot flip-flop
        ,#00
        OUT     2               .. set keyboard scan for switch 'C'
        ,#0C
        
.. Find the top of RAM starting at the 4K boundary (max size for a VIP)
MEMCK:  LDI     #FF             .. check for top of memory, starting at #0FFF
        PLO     R1
        LDI     #0F
        PHI     R1
MEMTST: LDI     #AA             .. write a byte and check that it got there
        STR     R1
        LDN     R1
        XRI     #AA
        BZ      DONE            .. exit at highest page
        
        GHI     R1
        SMI     #04             .. drop down 1K (#0400)
        BM      DONE            .. quit if at start of memory
        PHI     R1
        BR      MEMTST
        
DONE:   B3      CLRDSP          .. start if KEY 'C' pressed
        GHI     R0              .. else, start user program at #0000
        PLO     R0
        SEX     R0
        SEP     R0
        
.. Erase the display page from 0XFF to 0XB0 
CLRDSP: SEX     R1
CLEAR:  LDI     #00
        STXD
        GLO     R1
        XRI     STKTOP          .. clear register table to memory top
        BNZ     CLEAR           .. (#0XB0 to #0XFF)  (0X = top of memory)
                                .. e.g. X = #7(2K), #B(3K) or #F(4K)
                                
.. copy registers to register table
.. by creating op codes on the fly
.. #0XB3-#0XBF <- R3.0-RF.0
.. #0XC3-#0XCF <- R3.1-RF.1
REGDMP: LDI     #D2             .. SEP R2 op code
        STXD
        LDI     #9F             .. GHI RF op code
        STR     R1
        GLO     R1
        PLO     R0
        GHI     R1
        PHI     R0              .. R0 <- R1
        LDI     #CF
        PLO     R1              .. R1 <- #0xCF  (top of register table)
REGRD:  SEP     R0
        STXD
        DEC     R0
        DEC     R0
        LDA     R0
        SMI     #01
        DEC     R0
        STR     R0
        XRI     #82             .. stop after R3.0 is stored (GLO R2 opcode)
        BNZ     REGRD
        
.. Initialize the registers to start the operating system
        GHI     R2
        PHI     R3
        LDI     A.0(MAIN)
        PLO     R3
        SEP     R3              .. go to MAIN
MAIN:   GHI     R0
        PHI     R2              .. R2.1 = top of memory (stack)
        PHI     RB              .. RB.1 = top of memory (video)
        PHI     RD              .. RD.1 = top of memory (display mem)
        LDI     #81             .. point subroutines to page 2 of ROM
        PHI     R1              .. #81 -> R1.1
        PHI     R4              .. #81 -> R4.1
        PHI     R5              .. #81 -> R5.1
        PHI     R7              .. #81 -> R7.1
        PHI     RA              .. #81 -> RA.1
        PHI     RC              .. #81 -> RC.1
        
        LDI     A.0(INT)
        PLO     R1              .. set interrupt pointer

        LDI     STKTOP
        PLO     R2              .. set stack pointer
        
        LDI     A.0(DISPD)
        PLO     R4              .. R4 = display data
        
        LDI     A.0(DISPW)
        PLO     R5              .. R5 = write character to display memory
        
        LDI     A.0(GETB)       .. RA = get 2 hex digits into a byte
        PLO     R7
        
        LDI     A.0(DBNCE)      .. RC = keyboard debounce initially
                                ..    = keyboard scan after first call
        PLO     RC
        
        SEX     R2              .. set R2 as stack pointer
        INP     1               .. turn display on
        
.. Enter address
        SEP     RC              .. call keypad debounce for the 'C' key
        SEP     R7              .. get address upper byte
        SEP     R7              .. routine must be called 3 times
        SEP     R7              .. because R7 returns to main
        PHI     R6              .. put high byte in R6
        
        SEP     R7              .. get address lower byte
        SEP     R7
        SEP     R7
        PLO     R6              .. R6 = address entered by user
        
        SEP     R4              .. display address
        
.. get user command
        SEP     RC              .. keyboard scan
        PHI     RE              .. store control value (key) in RE.1
        BZ      MWR             .. Memory write?
        XRI     #0A
        BZ      MRD             .. Memory read?
        
                                .. No, then a TAPE command
        SEP     RC              .. get number of pages (1 page = 256 bytes)
        PLO     RE              .. RE.0 = number of pages
        DEC     R2              .. decrement stack pointer (advances with OUT)
        OUT     1               .. turn the display off during tape operations
                                .. so bit timing is maintained
                                
        GHI     RE              .. get control value
        XRI     #0B             .. TAPE READ?
        BZ      TRD
        
        GHI     RE
        XRI     #0F
        BNZ     $               .. TAPE WRITE?
                                .. invalid command, go into endless loop

.. TAPE ROUTINES
.. tape format:
..      0000....S01234567PS01234567P...
..      <LEADER>< BYTE 0 >< BYTE 1 >...
..      LEADER is ~4s of zeros
..      S = start bit (1),      P = parity bit (odd parity)
..      A zero is one cycle of 2 kHz, a one is one cycle of 800 Hz
..      See RCA ap note ICAN-6934 for more info

.. Tape Write Routine
TWR:    LDI     A.0(WRBIT)
        PLO     RC              .. RC = Write bit routine
        
        LDI     #40
        PHI     R9              .. set tape leader timer
TWR1:   GHI     R3              .. #80 -> D
        SHR                     .. clear DF
        SEP     RC              .. write bit
        DEC     R9
        GHI     R9              
        BNZ     TWR1            .. done writing leader? (~ 4s)
        
TWR2:   LDI     #10             .. preset parity counter to even number
        PLO     R7
        LDI     #08             .. set number of bits
        PLO     R9
        LDA     R6              .. get byte
        PHI     R7

        GHI     R3              .. #80 -> D
        SHL                     .. set DF for start bit
        SEP     RC              .. write start bit
        GLO     R6              .. get next address
        BNZ     TWR3            .. end of page?
        DEC     RE              .. yes, decrement number of pages
        
TWR3:   GHI     R7              .. get byte
        SHR                     .. put LSB in DF
        PHI     R7
        SEP     RC              .. write data bit
        DEC     R9              .. decrement bit count
        GLO     R9              .. done?
        BNZ     TWR3
        
TWR4:   INC     R7              .. inc R7 - sets ODD parity
        GLO     R7
        SHR                     .. put parity bit in DF
        SEP     RC              .. write parity bit
        GLO     RE
        BNZ     TWR2            .. out of pages?
        
        SEP     RC              .. yes, write last parity bit again
        
TDONE:  INP     1               .. turn display back on
        DEC     R6              .. get last byte
        SEP     R4              .. and display it
        BR      $               .. endless loop
        
.. Tape Read Routine
TRD:    LDI     A.0(RDBIT)
        PLO     RC              .. RC = Read Bit routine
        
SRCH:   LDI     #0A
        PHI     R9              .. set timer for leader search
LDR:    SEP     RC              .. get bit
        BDF     SRCH            .. look for a starting leader of zeros
        
        DEC     R9
        GHI     R9
        BNZ     LDR             .. check to make sure have enough zeros 
                                .. for a leader
        
TRD1:   SEP     RC              .. get bit
        BNF     TRD1            .. wait for start bit
        
        LDI     #09
        PLO     R9              .. bit counter = 9
        PLO     R7              .. set parity counter to an odd number

TRD2:   GHI     R7              .. move bit in to R7.1
        SHRC
        PHI     R7
        DEC     R9              .. decrement number of bits
        SEP     RC              .. get data bit
        GLO     R9
        BNZ     TRD2            .. done with bits?
        
        GLO     R7
        SHR                     .. get parity check
        BDF     TRD3            .. if no parity error, continue
        SEQ                     .. else, turn on the tone
        
TRD3:   GHI     R7
        STR     R6              .. store byte
        INC     R6              .. next address
        GLO     R6
        BNZ     TRD1            .. at page boundary?
        DEC     RE              .. yes, decrement number of pages
        
TRD4:   GLO     RE
        BNZ     TRD1            .. done?
        
        BR      TDONE           .. yes, display last byte and spin
        
.. MEMORY ROUTINES

.. Memory read
.. displays the next byte with a press of any key
MRD:    SEP     RC              .. get any key from keyboard
        INC     R6              .. go to next address
        SEP     R4              .. display byte
        BR      MRD
        
.. Memory Write
MWR:    SEP     R7              .. get byte to write
        SEP     R7
        SEP     R7
        STR     R6              .. store in the address
        SEP     R4              .. display byte
        INC     R6              .. next address
        BR      MWR
        
.. end of main routine  
        ,#00
        ,#00
        ,#00
        ,#00

        ORG #8100
        
.. CHARACTER MAP LOOK UP TABLE
.. offsets to the character data

CH0:    ,A.0(DIG0)
CH1:    ,A.0(DIG1)
CH2:    ,A.0(DIG2)
CH3:    ,A.0(DIG3)
CH4:    ,A.0(DIG4)
CH5:    ,A.0(DIG5)
CH6:    ,A.0(DIG6)
CH7:    ,A.0(DIG7)
CH8:    ,A.0(DIG8)
CH9:    ,A.0(DIG9)
CHA:    ,A.0(DIGA)
CHB:    ,A.0(DIGB)
CHC:    ,A.0(DIGC)
CHD:    ,A.0(DIGD)
CHE:    ,A.0(DIGE)
CHF:    ,A.0(DIGF)

.. CHARACTER MAP
.. each character is 5 bytes
.. in a 8w x 5h matrix
.. the characters are 4w by 5h, left justified

DIGE:   ,#F0            .. ****....
        ,#80            .. *.......
DIGF:   ,#F0            .. ****....
        ,#80            .. *.......
DIGC:   ,#F0            .. ****....
        ,#80            .. *.......
        ,#80            .. *.......
        ,#80            .. *.......
DIGB:   ,#F0            .. ****....
        ,#50            .. .*.*....
        ,#70            .. .***....
        ,#50            .. .*.*....
DIGD:   ,#F0            .. ****....
        ,#50            .. .*.*....
        ,#50            .. .*.*....
        ,#50            .. .*.*....
DIG5:   ,#F0            .. ****....
        ,#80            .. *.......
DIG2:   ,#F0            .. ****....
        ,#10            .. ...*....
DIG6:   ,#F0            .. ****....
        ,#80            .. *.......
DIG8:   ,#F0            .. ****....
        ,#90            .. *..*....
DIG9:   ,#F0            .. ****....
        ,#90            .. *  *....
DIG3:   ,#F0            .. ****....
        ,#10            .. ...*....
        ,#F0            .. ****....
        ,#10            .. ...*....
DIGA:   ,#F0            .. ****....
        ,#90            .. *..*....
DIG0:   ,#F0            .. ****....
        ,#90            .. *..*....
        ,#90            .. *..*....
        ,#90            .. *..*....
DIG7:   ,#F0            .. ****....
        ,#10            .. ...*....
        ,#10            .. ...*....
        ,#10            .. ...*....
        ,#10            .. ...*....
DIG1:   ,#60            .. .**.....
        ,#20            .. ..*.....
        ,#20            .. ..*.....
        ,#20            .. ..*.....
        ,#70            .. .***....
DIG4:   ,#A0            .. *.*.....
        ,#A0            .. *.*.....
        ,#F0            .. ****....
        ,#20            .. ..*.....
        ,#20            .. ..*.....

        .. END OF CHARACTER MAP

.. video interrupt (64 x 32 format)
INTX:   REQ                     .. tone off
        LDA     R2              .. get D from stack
        RET                     .. exit and re-enable interrupt
INT:    DEC     R2
        SAV                     .. T-> STACK
        DEC     R2
        STR     R2              .. D-> STACK
                                .. note DF is not changed by this routine
                                
        NOP                     .. 3 cycle instruction to synchronize
        INC     R9              .. increment random number value
        LDI     #00
        PLO     R0
        GHI     RB
        PHI     R0              .. reset R0 (DMA pointer) to video memory

        SEX     R2              .. NOP
        SEX     R2              .. NOP
DISP:   GLO     R0              .. LINE START ADDR -> D
        SEX     R2
        SEX     R2
        DEC     R0              .. reset R0.1 if pass page
        PLO     R0              .. line start addr -> R0.0
        SEX     R2              .. nop
        DEC     R0              .. reset R0.1 if pass page      
        PLO     R0              .. line start addr -> R0.0
        SEX     R2              .. nop
        DEC     R0              .. reset R0.1 if pass page      
        PLO     R0              .. line start addr -> R0.0
        BN1     DISP            .. loops 32 times

.. decrement timers every video frame (~16ms)
..      R8.1 is a regular timer
..      R8.0 is the tone timer

TIMER:  GHI     R8
        BZ      TIMX
        PLO     RB
        DEC     RB
        GLO     RB
        PHI     R8              .. dec timer.1 without changing DF
TIMX:   GLO     R8
        BZ      INTX            .. exit with tone off if timer = 0
        SEQ                     .. turn tone on if timer > 0
        DEC     R8              .. decrement tone timer
        BR      INTX+1          .. exit with tone on
        
.. write one bit on the tape
        SEP     R3              .. return
WRBIT:  LDI     #0A             .. set timer for a '0'
        BNF     WRBIT1          .. if DF = 0, send a 0
        LDI     #20             .. else, send a one
        INC     R7              .. and increment the parity counter
        
WRBIT1: SEQ                     .. set Q (write signal)
        PHI     RF              .. RF.1 = timer value (10 or 32)
WRBIT2: SMI     #01
        BNZ     $-2             .. send bit half cycle
        
        BNQ     WRBIT-1         .. if Q = 0, cycle is done
        REQ                     .. otherwise start second half of bit cycle
        GHI     RF              .. get timer value back in D
        BR      WRBIT2

.. read one bit on the tape
        SEP     R3
RDBIT:  LDI     #10             .. set timer to value between 10 and 32
        BN2     $               .. wait for start bit
RDBIT1: BN2     RDBIT2          .. if EF2 = 0, it is a zero bit,
                                .. note: EF2 will be 1 first time through (start bit)
        SMI     #01             .. (MSB of D = 0)
        BNZ     RDBIT1          .. time bit
        
                                .. if D = 0 before EF2 = 0, it must be a '1' bit
        INC     R7              .. inc parity counter
        GHI     RC              .. #81 -> D
        
RDBIT2: SHL                     .. shift in a 1 or 0
        B2      $               .. wait for next bit to start
        BR      RDBIT-1
        
.. keyboard scan
        SEP     R3
KSCN:   SEX     R2              .. this subroutine uses the stack
        GHI     RC
        PLO     RF              .. #81 -> RF.0

KSCN1:  DEC     RF              .. next key value
        DEC     R2              .. move stack down because OUT increments it
        GLO     RF
        STR     R2              .. store RF.0 on stack
        
        OUT     2               .. set keypad latch value, INC R2
        SEX     R2
        SEX     R2              .. pause
        BN3     KSCN1

.. alternative entry point to just debounce
DBNCE:  LDI     #04
        PLO     R8              .. set R8.0 to debounce timer (~66ms)
        GLO     R8
        BNZ     $-1             .. wait for timeout
        
DBNCE1: LDI     #04
        PLO     R8              .. additional timing operation
        
        B3      DBNCE1          .. wait for key 
        GLO     R8              .. pause
        BQ      $-3             .. if Q = 1, R8.0 timer is still running

        GLO     RF              .. get scan value
        ANI     #0F             .. mask off lower byte
        STR     R2              .. store on the stack
        BR      KSCN-1          .. exit

.. filler
        ,#00
        ,#00
        ,#00
        ,#00

.. assemble two keyboard entries into a byte
.. note: this needs to be called 3 times to get through it completely
.. because the keyboard scan keeps returning to main
..
        SEP     R3
GETB:   SEP     RC              .. get first digit (end 3rd SEP R7)
        SHL                     .. (end 1st SEP R7)
        SHL
        SHL
        SHL
        PLO     RE              .. put it in the high nibble

        SEP     RC              .. get second digit
        GLO     RE              .. (end 2nd SEP R7)
        OR                      .. combine the digits
        BR      GETB-1          .. and return with digits in D
        
.. write digit character to the display memory
        SEP     R4
DISPW:  PLO     RA              .. put digit to display in RA.0
        LDN     RA              .. offset into character map
        PLO     RA              .. get first byte of character data
        
        LDI     #05
        PLO     RF              .. set row counter for character data
        
STRDM:  LDA     RA              .. get byte
        STR     RD              .. save in display memory
        GLO     RD
        ADI     #08
        PLO     RD              .. offset to next display line (64 bits = 8 bytes)
        DEC     RF
        GLO     RF
        BNZ     STRDM           .. all 5 rows?

        GLO     RD
        ADI     NXTCHR          .. sets RD to 1 byte to the right
        PLO     RD              .. so characters do not overlap
        BR      DISPW-1

.. subroutine to display data address and contents on screen
        SEP     R3
DISPD:  DEC     R2              .. dec stack pointer
        LDN     R6
        STXD                    .. store byte on stack addressed by R6
        GLO     R6
        STXD                    .. store lower byte of address
        GHI     R6
        STR     R2              .. store upper byte of address
        
        LDI     #06
        PLO     RE              .. set special control byte
        
        LDI     LINE27          .. point to location for address display
        PLO     RD              .. in RD
        
DSPBY:  LDN     R2              .. get upper byte
        SHR
        SHR
        SHR
        SHR                     .. get upper nibble
        SEP     R5              .. display the digit
        
        LDA     R2
        ANI     #0F             .. get lower nibble
        SEP     R5              .. display the digit
        
        GLO     RE
        SHR
        PLO     RE              .. shift over control byte
        
        BZ      DISPD-1         .. exit if all digits written
        
        BNF     DSPBY           .. DF=0 after writing high address byte
                                .. else, provide space between the address and data
        INC     RD
        INC     RD
        BR      DSPBY           .. next byte
        
        ,#01                    .. filler and check byte
        END