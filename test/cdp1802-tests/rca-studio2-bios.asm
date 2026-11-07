.. *****************************************************************************
.. *****************************************************************************
..
..               SYSTEM BIOS / INTERPRETER : RCA STUDIO II
..
..          Reverse engineered by Paul Robson (autismuk@aol.com)
..
..     (Converted to RCA Assembler source by Steffen "Gulrak" Schümann)
..
.. *****************************************************************************
.. *****************************************************************************
..
..       R1      Video Interrupt Address
..       R2      Stack Pointer (initialised to $08BF)
..       R3      Sub  PC : Interpreting an Instruction
..       R4      Main PC : Interpreter Loop
..       R5      Interpreter Program Pointer
..       R6      Addr first variable (R0-RF)
..       R7      Addr second variable (R0-RF)
..       R8      Accessing system memory
..       R9      Random Seed
..       RA      Index Register
..       RB      Low = Vertical scroll position
..       RF.0    Current Instruction (Byte 1)
..
..       0800-087F               Sprite Graphic Buffers
..       0880-08B0 (approx)      Working Memory Space
..       08xx-08BF               1802 Stack
..       08C0-08CF               Variables (16 off)
..       08C9                    Sprite Number [R9]
..       08CA                    Keypad Select [RA] (0=P1,1=P2)
..       08CB                    Carry [RB]
..       08CC                    Direction [RC]
..       08CD                    Beep length counter [RD]
..       08CE,F                  Counters [RD,RF]
..       08D0-08D7               Sprites (Position)
..       08D8-08DF               Sprites (Height, Flip)
..       08E0-08E7               Direction
..       08E8-08EF               1-Screen Position (internal ?)
..       08F0-08F7               Move counter
..       08F8-08FF               Working Memory Space
..
..       0xxx    Call 1802 code at xxx
..                       0066    Turn the video on.
..       1xxx    Jump to xxx
..       2xxx    Call Subroutine at xxx
..       3raa    Jump to aa (short) if r <> 0
..       4raa    Jump to aa (short) if r == 0
..       5rnn    Skip instruction if Register <> nn
..       6rnn    Load constant nn to r
..       70aa    Decrement R0,short jump if not zero
..       7rnn    Add constant nn to r (NOT r = 0)
..       8xyf    Arithmetic and logic operations, all set carry register RB
..               if not specified then unpredictable.
..                       8xy0    RX := RY,
..                       8xy1    RX := RX or RY
..                       8xy2    RX := RX and RY
..                       8xy3    RX := RX xor RY
..                       8xy4    RX := RX + RY, RB Carry
..                       8xy5    RX := RY - RX, RB Not Borrow
..                       8xy6    RX := RX >> 1, RB LSB
..                       8xy7    RX := RX - RY, RB Not Borrow
..                       8xyE    RX := RX << 1, RB MSB
..       9xy0    Skip instruction if RX <> RY
..       9xyf    System Memory operations (the lowest set bit decides function)
..                       9xy1    RX := RY                (also 3,5,7,9,B,D,F)
..                       9xy2    RX := Memory[RY]        (also 6,A,E)
..                       9xy4    Memory[RY] := RX        (also 4,C)
..                       9xy8    RX -> Memory[RY..RY+2] as BCD, adds 3 to RY
..       Annn    Load index register with nnn
..       Bcnn    Store nn at index register, add c to it (LSB only, wraps)
..       C0xx    Return from subroutine
..       Crnn    Register r = Random Number & nn
..       Dkaa    Check if key k pressed (if F then key in RB), if so
..               short jump to aa and set RB = Key Number. RA is player selector
..       E0      Clear Graphic Buffer for given sprite [Sprite No in R9]
..       E1      Move Sprite in given direction (also 3,5,7,9,B,D,F)
..       E2      Move sprite in direction in RC (also 6,10,E)
..       E4      XOR Data at Index into current picture, Index incremented by Height
..       E8aa    Draw Sprite on screen ; short jump to aa on collision.
..       Frnn    General function nn [0rnn equivalent]
..                       FrA6    Read Memory[Index] into Rr
..                       FrA9    Write Rr to Memory[Index]
..                       FrAC    Read Memory[Index] into Rr and inc. index
..                       FrAF    Write Rr to Memory[Index] and inc. index
..                       FrB3    Set LSB of Index to Register r
..                       FrB6    And Register r with 15, then or with index LSB
..                       F?F2    Short delay based on LSB of Index, 0->Index
..
.. *****************************************************************************

        ORG #0000               .. Starts at base address 0.

Memory  = #0800                 .. System/Data memory page
VideoMem = #0900                .. Video Memory Page

MemStack = Memory+#0BF          .. Initial stack value

Variables = Memory+#0C0         .. Variables (16 off,last 3 counters)
Carry = Memory+#0CB             .. Carry Flag
Direction = Memory+#0CC         .. Direction override flag
Player = Memory+#0CA            .. Keypad/Player Selector
SpriteID = Memory+#0C9          .. Sprite Number
CountEnd = Memory+#0CF          .. Counters decremented at 50hz
CountStart = Memory+#0CD        .. till zero (3 off)
SoundCounter = Memory+#0CD      .. Sound Counter

Program = #0300                 .. Interpreted program starts here
.. Must be on page boundary

.. *****************************************************************************
..
..       Come here at reset. Initialise everything, start the
..       main interpreting loop.
..
.. *****************************************************************************

Reset:
        ghi r0                  .. Clear accumulator (D=0 on reset)
        phi r1                  .. R1.High = Interrupt Routine
        phi r4                  .. R4.High = Interpreter Loop
        plo r5                  .. R5.Low = Program Pointer
        plo rb                  .. RB.Low = Vertical Scroll

        ldi A.1(Memory)         .. D = System Memory Page
        phi r2                  .. R2.High = Stack Base
        phi r6
        phi r8
        ldi A.0(VideoInt)       .. R1.Low = Interrupt Routine
        plo r1
        ldi A.0(MemStack)       .. R2.Low = Stack Base
        plo r2
        ldi A.0(MainLoop)       .. R4.Low = Interpreter Loop
        plo r4
        ldi A.1(Program)        .. R5.High = Program Pointer
        phi r5
        sep r4                  .. Go to main interpreter loop

.. *****************************************************************************
..
..       Exit from video interrupt.
..
.. *****************************************************************************

RetNoSound:
        req                     .. Turn off sound
RetSoundCont:
        lda r2                  .. D = Data flag byte
        shr                     .. Shift into DF
        lda r2                  .. Restore D
        ret                     .. And return to caller.

.. *****************************************************************************
..
..       Video interrupt. See Bill Richman's Elf Pages for more
..       information at http://incolor.intenebr.com/bill_r
..
.. *****************************************************************************

VideoInt:
        dec r2                  .. Save (X,P) at R2-1 (stack)
        sav
        dec r2                  .. Save D at R2-1 (stack)
        stxd
        lbr WasteCycles         .. Waste a few cycles for video
WasteCycles:
        shlc                    .. D bit 0 now contains the DF Flag
        str r2                  .. Save DF at R2 (stack)

        inc r9                  .. Change Random Seed

        ldi A.1(VideoMem)       .. R0.High points to video
        phi r0

        ldi A.0(CountEnd)+1     .. R8 points to 08D0h
        plo r8
        glo rb                  .. Get start of display position

        plo r0
        sex r2                  .. 8 DMA Cycles
Refresh:
        dec r0
        plo r0
        sex r2                  .. 8 DMA Cycles
        dec r0
        plo r0
        sex r2                  .. 8 DMA Cycles
        dec r0
        plo r0
        glo r0                  .. 8 DMA Cycles
        dec r0
        plo r0
        bn1 Refresh             .. Not end of this frame
WaitEnd:
        dec r0                  .. Tidy up
        plo r0
        b1  WaitEnd             .. Wait for end of display pulse

_CountCheck:
        dec r8                  .. Look at next byte of counters
        ldn r8                  .. Decrement if non-zero
        bz  _CountIsZero
        smi #001
        str r8
_CountIsZero:
        glo r8                  .. Has counter reached $00000
        xri A.0(CountStart)
        bnz _CountCheck         .. if not, keep going

        ldn r8                  .. Is the counter zero ?
        bz  RetNoSound          .. if so, sound off
        seq                     .. sound on
        br  RetSoundCont        .. and exit
..
..       Code for Crnn, Random number generator.
..
_Random:
        inc r9                  .. Bump the random pointer
        glo r9                  .. RE := $00.R9Low
        plo re
        ghi r3
        phi re
        ghi r9                  .. D := R9High
        sex re
        add                     .. Calc R9.1 + Mem[R9.Low]
        str r6                  .. Save in the register specified
        shr                     .. Add half the value to it
        sex r6
        add
        phi r9                  .. Tweak the MSB of random pointer
        str r6                  .. Update the value
        lda r5                  .. Get the AND mask
        and                     .. AND with the random value
        str r6                  .. Write it back
        sep r4                  .. Next instruction

        idl                     .. Wasted space ?
        idl
..
..       Turn on the video display
..
        sex r2                  .. Make space for INP command
        dec r2
        inp 1                   .. INP1 turns video on
        inc r2                  .. Fix Stack
        sep r4                  .. Next Instruction

.. *****************************************************************************
..
..                               Main interpreter loop
..
.. *****************************************************************************

MainLoop:                       .. Main interpreter loop
        ghi r6                  .. Set 2nd variable High
        phi r7
        ghi r4                  .. RC.High := $00
        phi rc
        lda r5                  .. Read instruction, adv R5
        plo rc                  .. RC.Low := Instruction
        plo rf                  .. RF.Low := Instruction
        shr                     .. Put 4 MSB of instruction into D
        shr
        shr
        shr
        bz  CodeExec            .. If zero, exec as code

        ori A.0(AddrTable)      .. D := E1-EF (depends on inst MSN)
        plo rc                  .. RC points to instr addr table
        glo rf                  .. D = original instruction
        ani #00F                .. D = C0-CF (inst LSB)
        ori A.0(Variables)
        plo r6                  .. R6.Low = C0-CF (Instr Byte1 LSN)

        ldn r5                  .. Get instruction 2nd byte
        shr                     .. Isolate MS Nibble
        shr
        shr
        shr
        ori  A.0(Variables)     .. R7.Low = C0-CF (Instr Byte2 MSN)
        plo r7

        lda rc                  .. Read Instruction Addr High
        phi r3                  .. Into R3.High
        glo rc                  .. Add 15 to address table now
        adi #00F                .. F1-FF [16 total with LDA]
        plo rc
        lda rc                  .. Read Instruction Addr Low
_Execute:
        plo r3                  .. Exec Addr.Low := R3
        sep r3                  .. And jump there
        br  MainLoop            .. Loop back on return
..
..       Execute instructions as code.
..
CodeExec:
        glo rf                  .. Get Instruction Byte
        ani #00F                .. Get Addr 4 MSB
        phi r3                  .. Put in R3
        lda r5                  .. Get Low Byte of Address
        br  _Execute            .. and Jump there
..
..       8XYF    RX := RX (function) RY [see above for functions]
..
Cmd_ALU:
        dec r2                  .. Make space on stack
        sex r2                  .. Write to stack area
        ldi #0D3                .. Last instruction "SEP 3"
        stxd
        lda r5                  .. Make MSNibble byte 1 into a
        ori #0F0                .. 1802 ALU Instruction
        str r2                  .. Save that
        sex r6                  .. R(X) is now the first register
        lda r7                  .. Read the second register
        sep r2                  .. Do the instruction, fixes R2 too
        str r6                  .. Save the result back
        ldi A.0(Carry)          .. R6 := $08CB, Register B
        plo r6
        ghi r1                  .. D = 0
        shlc                    .. D = Carry value
        str r6                  .. Save carry value
        sep r4                  .. Return
..
..       C0xx, return from subroutine, Crxx Register r = Random & 255
..
Cmd_Ret:
        glo r6                  .. Get the address of MSN Byte 1
        xri A.0(Variables)      .. Check it is C0
        bnz _Random             .. No, go to random number bit
        lda r2                  .. Pop MSB off stack
        phi r5
        lda r2                  .. Pop LSB off stack
        plo r5
        sep r4                  .. Next instruction
..
..       6RNN    Load R Immediate with NN
..
Cmd_LAcc:
        lda r5                  .. Get the 2nd byte (data)
        str r6                  .. Store in register
        sep r4                  .. Next instruction
..
..       Powers of 10 Data Table (used in BCD conversion)
..
Power10:
        DB #64                  .. 100
        DB #0A                  .. 10
        DB #01                  .. 1
..
..       3RAA    Short jump to AA if R <> 0
..
Cmd_Jnz:
        ldn r6                  .. Read the register
        bnz _Jump0              .. If non zero jump
_Skip0: inc r5                  .. Advance past address
        sep r4                  .. and exit
..
..       4RAA    Short jump to AA if R = 0
..
Cmd_Jz:
        ldn r6                  .. Read the register
        bnz _Skip0              .. If non zero skip
_Jump0:
        ldn r5                  .. Get address
        plo r5                  .. Update Interpreter Address
        sep r4                  .. Next Instruction
..
..       2XXX    Call subroutine at xxx
..
Cmd_Call:
        inc r5                  .. R5 points to next instruction
        glo r5                  .. save LSB it on the stack
        dec r2
        str r2
        ghi r5                  .. Save MSB of return address
        dec r2
        str r2
        dec r5                  .. Restore R5 so GOTO works
..
..       1xxx    Jump Command
..
Cmd_Jump:
        lda r5                  .. Get LSB and advance pointer
        plo r5                  .. Update LSB Interpreter
        glo r6                  .. R6 := 08Cx, x is Nibl of MSB
        ani #00F                .. Strip off the rest
        phi r5                  .. Put into MSB Interpreter
        sep r4                  .. Next instruction
..
..       2xxx    Set Index Register Command
..
Cmd_LIndex:
        glo r6                  .. Get MSB and strip off MSN
        ani #00F
        phi ra                  .. Put in High Byte Index
        lda r5                  .. Get LSB and advance pointer
        plo ra                  .. Put in Low Byte Index
        sep r4                  .. Next Instruction

.. *****************************************************************************
..
..       Instruction Address Table. First 16 bytes are High byte, Second
..       16 are Low Byte
..
.. *****************************************************************************

AddrTable:
        DB 0                    .. 0xxx Machine Call, (seperate)
        DB A.1(Cmd_Jump)        .. 1xxx Jump to xxx
        DB A.1(Cmd_Call)        .. 2xxx Call Subroutine at xxx
        DB A.1(Cmd_Jnz)         .. 3raa Short Jump if r <> 0
        DB A.1(Cmd_Jz)          .. 4raa Short Jump if r == 0
        DB A.1(Cmd_SkipN)       .. 5rxx Skip if r <> xx
        DB A.1(Cmd_LAcc)        .. 6rnn Load r with nn (imm)
        DB A.1(Cmd_Add)         .. 7rnn Add nn to r (70xx different)
        DB A.1(Cmd_ALU)         .. 8xyf X := X (function) Y
        DB A.1(Cmd_Mem)         .. 9xyf Memory functions
        DB A.1(Cmd_LIndex)      .. Axxx Load Index with xxx
        DB A.1(Cmd_LCI)         .. Bcnn Store constant and bump
        DB A.1(Cmd_Ret)         .. Cxxx Return and Random Number
        DB A.1(Cmd_Key)         .. Dkaa Check key and branch
        DB A.1(Cmd_Draw)
        DB A.1(Cmd_F)           .. Frxx General funcs (jump to 02xx)
..
        DB 0                    .. 0xxx
        DB A.0(Cmd_Jump)        .. 1xxx
        DB A.0(Cmd_Call)        .. 2xxx
        DB A.0(Cmd_Jnz)         .. 3xxx
        DB A.0(Cmd_Jz)          .. 4xxx
        DB A.0(Cmd_SkipN)       .. 5xxx
        DB A.0(Cmd_LAcc)        .. 6xxx
        DB A.0(Cmd_Add)         .. 7xxx
        DB A.0(Cmd_ALU)         .. 8xxx
        DB A.0(Cmd_Mem)         .. 9xxx
        DB A.0(Cmd_LIndex)      .. Axxx
        DB A.0(Cmd_LCI)         .. Bxxx
        DB A.0(Cmd_Ret)         .. Cxxx
        DB A.0(Cmd_Key)         .. Dxxx
        DB A.0(Cmd_Draw)        .. Exxx
        DB A.0(Cmd_F)           .. Fxxx

.. *****************************************************************************
..
..       Drawing code.
..
..       Offsets (Table at $08D0 onwards) :-
..
..       $00     Screen Physical Position
..       $08     Height: Bits 0..3. Bit 7 switches the bytes around (flip ?)
..       $10     Direction
..       $18     1-Screen Position (no idea why)
..       $20     Horizontal Move Counter
..
..       Each of the 8 sprites has a picture buffer at $0800 + n * 16.
..       These are alternating 8 byte values, one for the left, one
..       for the right.
..
..       $E0     Clear sprite picture buffer
..       $E1     Move
..       $E2     Move in direction in RC
..       $E4     XOR Plot data at Index (changed) into sprite picture buffer
..               for height given.
..       $E8aa   Draw on screen ; short jump if collision.
..
.. *****************************************************************************

Cmd_Draw:
        ldi A.0(SpriteID)       .. R7 now accesses IR9, Sprite ID
        plo r7
        ldn r7                  .. Read register IR9
        shl                     .. Multiply by 16
        shl
        shl                     .. Sets up the sprite data pointer
        shl
        plo r6                  .. R6.0 := $0800+ Sprite# * 16

        ldi #0D0                .. Calculate IR9+0D0h
        sex r7
        add
        plo r7                  .. R7 now points to $08D0+IR9
.. the sprite base address

        ldi A.1(PointRFTo)      .. RC := $0292, the set up pointer
        phi rc                  .. routine. This sets RF equal to
        ldi A.0(PointRFTo)      .. the base pointer in R7.0 added
        plo rc                  .. to the following byte

        sep rc                  .. Point to offset $10
        DB     #10
        ldn rf                  .. Read offset $10
        phi rd                  .. Save in RD.1

        sep rc                  .. Point to offset $08, Height
        DB #08
        ldn rf                  .. Read offset $08
        ani #00F                .. only the lower 4 bits matter
        plo re                  .. Save in RE.0

        ldn rf                  .. Re read offset $08
        ani #080                .. Keep bit 7 only
        phi re                  .. Save in RE.1

        ldn r7                  .. Read offset $00
        plo rd                  .. Save in RD.0

        dec r5                  .. Re-read the first byte of the
        lda r5                  .. opcode

        shr                     .. Bit 0 set
        bdf _Move
        shr                     .. Bit 1 set
        bdf _MoveC
        shr                     .. Bit 2 set
        bdf _Draw               .. Xor Draw into Sprite picture
        shr                     .. Bit 3 set
        bdf _DrawSprite         .. Draw onto video display
..
..       Clear Picture Buffer to all Zeroes
..
        ldi #010                .. RF := 16, size of sprite space
        plo rf
_Clear0:
        ghi r1                  .. Write zero at that location
        str r6
        inc r6                  .. Go to next one
        dec rf                  .. Do all 16 bytes
        glo rf
        bnz _Clear0
_Continue:
        sep r4
..
..       XOR Data at Index into Sprite, amount dependent on the height of
..       the sprite. Index is changed (Height added). Skips 2 for some reason.
..
_Draw:                          .. function $E4,$EC.
        sex r6                  .. R[X] is the sprite data
_DrawLoop:
        glo re                  .. Height is zero ?
        bz  _Continue           .. If so, completed
        lda ra                  .. Read byte from index and inc
        xor                     .. XOR with sprite data
        str r6                  .. Update
        inc r6                  .. Next Sprite Byte x 2
        inc r6
        dec re                  .. Decrement height counter
        br  _DrawLoop           .. Do the next one.
..
..       Move in direction RC, override the read direction
..
_MoveC:
        ldi A.0(Direction)      .. RF := $08CC (Register C)
        plo rf
        ldn rf                  .. Read it
        phi rd                  .. Use instead of offset $10
..
..       Move in direction in RD.1
..
_Move:
        ghi rd                  .. Decide on direction of movement
        xri #002                .. 2 is up
        bz  _Up
        ghi rd
        xri #008                .. 8 is down
        bz  _Down
        ghi rd
        xri #004                .. 4 is left
        bz  _Left
        ghi rd                  .. 6 is right
        xri #006
        bz _Right
        sep r4                  .. All others ignored

_Up:
        ldn r7                  .. Subtract 8 from the position
        smi #008
_MoveVertical:
        str r7
        sep rc                  .. Point RF to offset $18
        DB #18
        ghi r3                  .. D := $01
        sd                      .. D = 1 - Physical Position
        str rf                  .. Store in $18
        sep r4                  .. And exit

_Down:
        ldn r7                  .. Calculate position + 8
        adi #008
        br  _MoveVertical       .. And do as for 'up' code.

_Right:
        glo re                  .. Done the whole sprite
        bz  _DecMoveCtr         .. if so, exit

        ldn r6                  .. Shift the first byte right 1
        shr
        str r6
        bnf _NoRight2           .. If no bit shifted out, skip
        ghi re
        bnz _NoRight2           .. If right side, skip
        ldi #080
        phi re
        inc rd                  .. Advance screen pointer
_NoRight2:
        inc r6                  .. Point to second byte
        ldn r6                  .. Shift that right as well
        shrc
        str r6
        bnf _NoRight3           .. No carry out, skip
        dec r6                  .. Point to previous byte
        ldn r6                  .. Set bit 7
        ori #080
        str r6
        inc r6                  .. Fix the pointer
        ghi re
        bz _NoRight3            .. Skip if not swapped
        ghi r1                  .. Swap it back
        phi re
        inc rd                  .. Advance screen pointer
_NoRight3:
        inc r6                  .. Point to next pair
        dec re                  .. Decrement Height counter
        br  _Right              .. And do next line

_Left:
        glo re                  .. Check height counter
        bz  _DecMoveCtr         .. Exit if zero
        ldn r6                  .. Shift first byte left
        shl
        str r6
        bnf _Left0              .. If no carry out, that's it
        ghi re                  .. If this is the left side, skip
        bz  _Left0
        ghi r1                  .. Swap the sides around
        phi re
        dec rd                  .. Change the pointer
_Left0:
        inc r6                  .. Look at 2nd byte
        ldn r6                  .. Shift it left
        shlc
        str r6
        bnf _Left1              .. No bit out, skip
        dec r6                  .. Set bit in 2nd byte
        ldn r6
        ori #001
        str r6
        inc r6                  .. Fix it back again
        ghi re                  .. If the right side , skip
        bnz _Left1
        ldi #080                .. Else swap them around
        phi re
        dec rd                  .. Adjust pointer
_Left1:
        inc r6                  .. Point to next byte
        dec re                  .. Decrement height counter
        br  _Left               .. And do for all the data

_DrawSprite:
        ghi r3                  .. Set up RC to point to the
        phi rc                  .. XOR Plot drawing subroutine
        ldi A.0(XORPlot)
        plo rc

        ghi r1                  .. Clear the collision flag
        plo rf

        ghi re                  .. Check flip (draw direction)
        bz  _DrawIt             .. bit.
        dec rd

_DrawIt2:
        glo re                  .. Exit if height counter is zero
        bz  ExitDraw
        sep rc                  .. Xor draw part 0 on left
        inc rd
        sep rc                  .. Xor Draw part 1 on right
        glo rd                  .. Next vertical line down
        adi #007
        plo rd
        dec re                  .. Decrement Height Counter
        br _DrawIt2             .. Loop back till finished

_DrawIt:
        glo re                  .. Exit if height counter is zero
        bz  ExitDraw
        sep rc                  .. Xor Draw part 0 on right
        dec rd
        sep rc                  .. Xor Draw part 1 on left
        glo rd                  .. Next vertical line down
        adi #009
        plo rd
        dec re                  .. Decrement height counter
        br  _DrawIt             .. And keep going until finished.

ExitDraw:
        glo rf                  .. Look at collision flag
        bnz _CSJ                .. If collide then short jump
        inc r5                  .. Skip address and return
        sep r4
_CSJ:
        ldn r5                  .. Do a short jump to the
        plo r5                  .. following address
        sep r4                  .. Next instruction
..
XORPlotNext:
        inc r6                  .. Next data item
        sep r3                  .. Exit
..
..       RD.0 points to a video memory location. XOR Plot Memory[R6]
..       at that byte, post incrementing R6. Set RF.0 non zero if collision
..
XORPlot:
        ldi #009                .. Point RD to video memory
        phi rd
        sex rd                  .. R[X] is video byte
        ldn r6                  .. And screen with R6
        and
        bz  _NoCollide          .. if zero, then no collision
        plo rf                  .. set collision flag
_NoCollide:
        ldn r6                  .. Read pixel data
        xor                     .. Xor with screen
        str rd                  .. Write back to screen
        br XORPlotNext
..
..       Decrement horizontal move counter (offset $20), update bit 7
..       of Height, update new position, and return.
..
_DecMoveCtr:
        sep rc                  .. Point to horiz move counter
        DB #20
        ldn rf                  .. Read value
        smi #001                .. Subtract 1
        str rf                  .. Write back

        sep rc                  .. Point to height
        DB #08
        ldn rf                  .. Copy new bit 7 back in from
        ani #00F                .. RE.1. Mask out height
        str rf
        ghi re                  .. Get new bit 7
        or                      .. Or with value
        str rf                  .. save it back

        glo rd                  .. Copy new position back
        str r7
        sep r4                  .. Return

.. *****************************************************************************
..
..          An Subroutine in the interpreted code.... does .....
..
.. *****************************************************************************

        DB  #022,#0F9           .. CALL $2F9 [Beep and Delay]
        DB  #023,#0C3           .. CALL $3C3 [???]
        DB  #0C0                .. RET

.. *****************************************************************************
..
..                     Offsets in this page to number data
..
.. *****************************************************************************

NumberGraphicOffset:
        DB A.0(_Zero)           .. Digit 0
        DB A.0(_One)            .. Digit 1
        DB A.0(_Two)            .. Digit 2
        DB A.0(_Three)          .. Digit 3
        DB A.0(_Four)           .. Digit 4
        DB A.0(_Five)           .. Digit 5
        DB A.0(_Six)            .. Digit 6
        DB A.0(_Seven)          .. Digit 7
        DB A.0(_Eight)          .. Digit 8
        DB A.0(_Nine)           .. Digit 9

.. *****************************************************************************
..
..                               Number Graphic Data
..
.. *****************************************************************************

_One:
        DB  #060                .. .xx.....
        DB  #020                .. ..x.....
        DB  #020                .. ..x.....
        DB  #020                .. ..x.....
        DB  #070                .. .xxx....
_Three:
        DB  #0F0                .. xxxx....
        DB  #010                .. ...x....
        DB  #070                .. .xxx....
        DB  #010                .. ...x....
_Five:
        DB  #0F0                .. xxxx....
        DB  #080                .. x.......
_Two:
        DB  #0F0                .. xxxx....
        DB  #010                .. ...x....
_Six:
        DB  #0F0                .. xxxx....
        DB  #080                .. x.......
_Eight:
        DB  #0F0                .. xxxx....
        DB  #090                .. x..x....
_Nine:
        DB  #0F0                .. xxxx....
        DB  #090                .. x..x....
        DB  #0F0                .. xxxx....
        DB  #010                .. ...x....
_Zero:
        DB  #0F0                .. xxxx....
        DB  #090                .. x..x....
        DB  #090                .. x..x....
        DB  #090                .. x..x....
_Seven:
        DB  #0F0                .. xxxx....
        DB  #010                .. ...x....
        DB  #010                .. ...x....
        DB  #010                .. ...x....
        DB  #010                .. ...x....
_Four:
        DB  #0A0                .. x.x.....
        DB  #0A0                .. x.x.....
        DB  #0F0                .. xxxx....
        DB  #020                .. ..x.....
        DB  #020                .. ..x.....

.. *****************************************************************************
..
..                   Lots of other interpreting routines
..
.. *****************************************************************************
..
..       7rxx Add, (except for 70xx, Decrement and jump if not zero)
..
Cmd_Add:
        glo r6                  .. Look at register number
        ani #00F
        bz _Cmd70
        sex r6                  .. Access register r
        lda r5                  .. Read constant
        add                     .. Add to r
        str r6                  .. Update result
        sep r4                  .. Next instruction
_Cmd70:
        ldn r6                  .. Read value
        smi #001                .. Decrement
        bnz _UpdateR            .. if not 0, then store & jump
        inc r5                  .. skip address
_Next0:
        sep r4                  .. Next instruction
..
..       5rnn    Skip if Register <> Constant
..
Cmd_SkipN:
        lda r5                  .. Read the constant
_Compare:
        sex r6                  .. Access the first register
        xor                     .. Compare them
        bz  _Next0              .. If the same, do next instruction
        inc r5                  .. Skip next instruction
        inc r5
        sep r4                  .. the *next* instruction
..
..       9xyf Memory Operations [9xy0 is skip if RX <> RY]
..
Cmd_Mem:
        ghi r6                  .. RC := Memory page.Register y
        phi rc                  .. i.e. it is indirection.
        ldn r7                  .. if R0 = $32 then 90xx points
        plo rc                  .. RC to $0832
        lda r5                  .. Get the operation code & skip it
        shr                     .. Bit 0 set ?
        bdf _CopyReg            .. it is RX = RY
        shr                     .. Bit 1 set ?
        bdf _ReadMem            .. it is RX = Mem[RY]
        shr                     .. Bit 2 set ?
        bdf _WriteMem           .. it is Mem[RY] = RX
        shr                     .. Bit 3 set ?
        bdf _BCDConv            .. it is RX->Mem[RY] as BCD
        ldn r7                  .. Restore RY
        br  _Compare

_ReadMem:
        ldn rc                  .. Read Memory location
        str r6                  .. Store in Rx
        sep r4
_WriteMem:
        ldn r6                  .. Read Rx
        str rc                  .. Store in Memory location Mem[Ry]
        sep r4
_BCDConv:
        sex r6                  .. R[X] is Rx
        ldn r6                  .. Save Rx value in RF.1
        phi rf
        ghi r1
        phi re
        ldi A.0(Power10)        .. Set RE := Powers of 10 table
        plo re
        dec rc                  .. Fix first check ?

_AnotherDigit:
        inc rc                  .. Look at next Mem[RY] byte
        ghi r1                  .. Set it to zero
        str rc
_RepSubtract:
        ldn re                  .. Read first power of 10
        sd                      .. subtract that value
        bnf _NextDigit          .. if borrow then done this one
        str r6                  .. update the result
        ldn rc                  .. increment Mem[RY]
        adi #001
        str rc
        br  _RepSubtract        .. And go around again
_NextDigit:
        lda re                  .. Get the next value
        shr                     .. is bit 0 set (e.g. it is 1)
        bnf _AnotherDigit       .. if not, do another digit
        ghi rf                  .. Restore RX value
        str r6
        glo rc                  .. Restore RY value *updated*
        str r7
        sep r4                  .. and exit
..
_PRFExit:
        sex rf
        sep r3
..
..       Sprite utility function
..
PointRFTo:
        ghi r6                  .. RF.1 := $08
        phi rf
        glo r7                  .. D := R7.0 + Next Byte
        sex r3
        add
        plo rf                  .. RF = $08:(R7.0+Next Byte)
        inc r3                  .. Skip over the added byte
        br  _PRFExit

_CopyReg:
        ldn r6                  .. Read x
        str r7                  .. Store in Y
        sep r4

        dec r3
        b3 _ShortBranch
        ret
        ghi re
        DB #0C0
..
..       Frnn    Extended functions, execute via jump to $02nn
..
Cmd_F:
        lda r5                  .. Get the byte
        plo r3                  .. and jump there ($02R3)
..
..       FrA6    Read byte at Memory[Index] to Register
..
CmdF_ReadMemIndex:
        ldn ra                  .. Read byte pointed to by index
        str r6                  .. Store in Rx
        sep r4                  .. Return to caller
..
..       FrA9    Write Register to Memory[Index]
..
CmdF_WriteMemIndex:
        ldn r6                  .. Read byte
        str ra                  .. Write to Memory[index]
        sep r4                  .. Return
..
..       FrAC    Read byte at Memory[Index] to Register and Increment
..
CmdF_ReadMemIndexBump:
        lda ra                  .. Read byte of mem, bump pointer
        str r6                  .. Save in register
        sep r4                  .. Return
..
..       FrAF    Write byte to Memory[Index] and Increment
..
        ldn r6                  .. Read byte from register
        str ra                  .. Store at Mem[Index]
        inc ra                  .. Inc Index
        sep r4                  .. Next instruction
..
..       FrB3    Set LSB of index to Register
..
        ldn r6                  .. Get value in Rx
        plo ra                  .. Put in index low
        sep r4                  .. Next instruction
..
..       FrB6    And Register with 15, or with Index LSB
..
        ldn r6                  .. Get value in Rx
        ani #00F                .. only 4 bits relevant
        str r6                  .. Write it back
        sex r6                  .. access via index
        glo ra                  .. Or with Index LSB
        or
        plo ra
        sep r4                  .. Next instruction
..
..       Dkaa Scan Keyboard. k = 0..9 the key, F key in RB. If successful
..       RB is set to the key number and the short branch is taken
..
Cmd_Key:
        ldi A.0(Carry)          .. R7 = $08CB (RB the carry flag)
        plo r7
        glo r6                  .. Get k (x register number)
        ani #00F
        plo rf                  .. RF.0 = k
        xri #00F                .. Check k = $0F ?
        bnz _NotInd             .. if not RF then not direction
        ldn r7                  .. If F, read key in RB
        plo rf
_NotInd:
        sex r2                  .. Make space on the stack
        dec r2
        glo rf                  .. Get the key, normally the k value
        str r2                  .. Save it at that location
        out 2                   .. Output it and fix R2
        ldi A.0(Player)         .. R6 = $08CA (RA)
        plo r6
        ldn r6                  .. Read $08CA
        bz  _KeyPad1            .. if zero read keypad 1
        b3  _KeyPressed         .. if key pressed then handle it
_NoPress:
        inc r5                  .. skip 2nd part
        sep r4                  .. and exit
_KeyPad1:
        bn4 _NoPress            .. if not pressed then skip
_KeyPressed:
        glo rf                  .. Get actual key number
        str r7                  .. Copy to RB
        br  _ShortBranch

_UpdateR:
        str r6                  .. Write value back to register
_ShortBranch:
        ldn r5                  .. Read second byte
        plo r5                  .. Update Value
        sep r4                  .. Next instruction
..
..       Bcnn Store constant at index and increment
..
Cmd_LCI:
        lda r5                  .. Get LSB of instruction
        str ra                  .. Store at Index register
        dec r2                  .. Make space on stack
        sex r2                  .. use stack as index
        glo r6                  .. get address of Rc
        ani #00F                .. get the register number (c)
        str r2                  .. store at the stack space
        glo ra                  .. Add c to Index, LSB only
        add
        plo ra
        inc r2                  .. Fix stack space
        sep r4                  .. Next instruction
..
..       $FxF2 : Short Delay using LSB of Index, Index 0 on exit.
..
CmdF_ShortDelay:
        ghi r1                  .. D = 0
        str ra                  .. Index High = 0
        glo ra                  .. Get Index Low
        dec ra                  .. Decrement it
        bnz CmdF_ShortDelay     .. If non-zero loop back
        sep r4

.. *****************************************************************************
..
..       Interpreted code subroutine. Does a short beep, and delays
..       for approximately 1/5 second
..
.. *****************************************************************************

        DB #6D,#02              .. RD := $02 (Sound Timer)   [6D02]
        DB #6E,#0A              .. RE := $0A (General Timer) [6E0A]
_IntWait:
        DB #3E,A.0(_IntWait)    .. JZ RE,_IntWait            [3EFD]
        DB #C0                  .. RET [C0xx]

END
