.. The CHIP-8 interpreter as part of the COSMAC VIP
.. recreated from a disassembly reference by Laurence Scotford, 2013
.. source recreation and bug fixes by Steffen "Gulrak" Schümann, 2025

ROM_FONT:       EQU     #8100

        ORG     #0000

        GHI     R1              .. R1 starts at end of RAM page.
        PHI     RB              .. RB points to display page.
        SMI     #01             .. Select page below display RAM.
        PHI     R2              .. Set stack page in R2.1.
        PHI     R6              .. Set VX page in R6.1.
        LDI     #CF             .. Init stack low byte.
        PLO     R2
        LDI     #81             .. Set R1 to VIP interrupt routine.
        PHI     R1
        LDI     #46
        PLO     R1
        GHI     R0              .. Prepare R4 as interpreter PC.
        PHI     R4
        LDI     A.0(FETCH)
        PLO     R4
        LDI     A.1(CHIPSTRT)   .. Set R5 to CHIP-8 PC start.
        PHI     R5
        LDI     A.0(CHIPSTRT)
        PLO     R5
        SEP     R4              .. Start running with R4 as interpreter PC.
FETCH:
        GHI     R6              .. Start fetch/decode; copy VX page.
        PHI     R7              .. Copy VX page to VY page.
        SEX     R2              .. Use stack pointer as X.
        GHI     R4              .. Copy dispatch table page.
        PHI     RC              .. RC will index dispatch tables.
        LDA     R5              .. Fetch opcode high byte; advance PC.
        PLO     RF              .. Save opcode high byte in RF.0.
        SHR                     .. Shift high nibble into low nibble.
        SHR
        SHR
        SHR
        BZ      OP_0MMM         .. Opcode 0: machine-code call.
        ORI     #50             .. Form dispatch high-byte table index.
        PLO     RC              .. RC points at handler high-byte entry.
        GLO     RF              .. Restore opcode high byte.
        ANI     #0F             .. Keep X nibble.
        ORI     #F0             .. Make VX pointer low byte.
        PLO     R6              .. R6 points to VX.
        LDN     R5              .. Read opcode low byte without advancing PC.
        SHR                     .. Shift Y nibble into low nibble.
        SHR
        SHR
        SHR
        ORI     #F0             .. Make VY pointer low byte.
        PLO     R7              .. R7 points to VY.
        LDA     RC              .. Fetch handler high byte.
        PHI     R3              .. Store handler page in R3.1.
        GLO     RC              .. Adjust RC from high-byte table to low-byte table.
        ADI     #0F             .. Low-byte table is 16 entries later. Allways resets DF!!!
        PLO     RC              .. RC now indexes low-byte table.
        LDN     RC              .. Fetch handler low byte.
CALLSUB:
        PLO     R3              .. Store handler low byte in R3.0.
        SEP     R3              .. Dispatch to handler.
        BR      FETCH           .. Fetch next opcode after handler returns.

OP_0MMM:
        GLO     RF              .. 0MMM: call a VIP machine-code routine.
        ANI     #0F             .. Keep the high address nibble.
        PHI     R3              .. Put it in R3.1 for the machine-code PC.
        LDA     R5              .. Fetch the low address byte and advance PC.
        BR      CALLSUB         .. Dispatch through the common subroutine call path.
        DEC     R2              .. Subroutine to turn on display.
        INP     9               .. Turn display on through the VIP I/O port.
        INC     R2              .. Pop stack byte.
        SEP     R4              .. Return to 0x42.

        DB      0,0             .. Align the dispatch high-byte table to opcode digits.

        .. Dispatch table: handler high bytes for opcode groups 1..F.
        DC      A.1(OP_1MMM), A.1(OP_2MMM), A.1(OP_3XNN), A.1(OP_4XNN)
        DC      A.1(OP_5XY0), A.1(OP_6XNN), A.1(OP_7XNN), A.1(OP_8XYN)
        DC      A.1(OP_9XY0), A.1(OP_AMMM), A.1(OP_BMMM), A.1(OP_CXNN)
        DC      A.1(OP_DXYN), A.1(OP_KEY), A.1(OP_FXNN)

        DB      0               .. Align the dispatch low-byte table to opcode digits.

        .. Dispatch table: handler low bytes for opcode groups 1..F.
        DC      A.0(OP_1MMM), A.0(OP_2MMM), A.0(OP_3XNN), A.0(OP_4XNN)
        DC      A.0(OP_5XY0), A.0(OP_6XNN), A.0(OP_7XNN), A.0(OP_8XYN)
        DC      A.0(OP_9XY0), A.0(OP_AMMM), A.0(OP_BMMM), A.0(OP_CXNN)
        DC      A.0(OP_DXYN), A.0(OP_KEY), A.0(OP_FXNN)

OP_DXYN:
        LDN     R6              .. DXYN: draw sprite at VX,VY; set VF on collision.
        ANI     #07             .. Keep X bit offset within display byte.
        PHI     RE              .. Save X bit offset in RE.1.
        LDN     R6              .. Get VX.
        ANI     #3F             .. Keep X position in 0..63.
        SHR                     .. Divide X by 8 for byte column.
        SHR
        SHR
        DEC     R2              .. Make room on stack.
        STR     R2              .. Push byte column.
        LDN     R7              .. Get VY.
        ANI     #1F             .. Keep Y position in 0..31.
        SHL                     .. Multiply Y by 8 for row offset.
        SHL
        SHL
        OR                      .. Combine row and byte column.
        PLO     RC              .. RC.0 = display offset.
        GHI     RB              .. Get display page.
        PHI     RC              .. RC points at target display byte.
        LDA     R5              .. Fetch sprite height N.
        ANI     #0F             .. Keep sprite height N.
        PLO     RD              .. RD counts display rows.
        PLO     R7              .. R7 counts rows while building sprite.
        LDI     #D0             .. Work buffer starts at V0-0x20.
        PLO     R6              .. R6 points into the work buffer.
NXTSROW:
        GHI     R3              .. Use R3.1 as zero.
        PLO     RF              .. Clear shifted right-byte accumulator.
        GLO     R7              .. Rows left to build?
        BZ      RSTIPTR         .. Done building rows?
        DEC     R7              .. Count one sprite row.
        LDA     RA              .. Fetch sprite byte through I; advance I.
        PHI     RD              .. RD.1 holds source sprite byte.
        GHI     RE              .. Load saved X bit offset.
        PLO     RE              .. RE.0 is shift counter.
SPLTROW:
        GLO     RE              .. Any shift left?
        BZ      STORROW         .. Done splitting this row?
        GHI     RD              .. Get left byte.
        SHR                     .. Shift left byte right, bit into DF.
        PHI     RD              .. Save shifted left byte.
        GLO     RF              .. Get right-byte accumulator.
        SHRC                    .. Shift DF into right byte.
        PLO     RF              .. Save shifted right byte.
        DEC     RE              .. One shift done.
        BR      SPLTROW         .. Repeat loop.
STORROW:
        GHI     RD              .. Left display byte.
        STR     R6              .. Store in work buffer.
        INC     R6              .. Advance work pointer.
        GLO     RF              .. Right display byte.
        STR     R6              .. Store in work buffer.
        INC     R6              .. Advance work pointer.
        BR      NXTSROW         .. Build next sprite row.
DRAW:
        IDL                     .. Wait for display interrupt.
        SEX     RC              .. Use RC for display RAM accesses.
        LDI     #D0             .. Point R6 at work buffer.
        PLO     R6              .. R6 points at shifted sprite rows.
        GHI     R3              .. Use R3.1 as zero.
        PLO     R7              .. Clear collision accumulator.
DRAWLOOP:
        GLO     RD              .. Rows left to draw?
        BZ      SAVEVF          .. All rows drawn?
        LDN     R6              .. Read left sprite byte.
        AND                     .. Collision if sprite bits overlap display bits.
        DEC     RD              .. Count one display row.
        BZ      DRAWLFT         .. No collision?
        LDI     #01             .. Set collision flag.
        PLO     R7              .. Save collision flag.
DRAWLFT:
        LDA     R6              .. Read left byte and advance work pointer.
        XOR                     .. Toggle display bits.
        STR     RC              .. Write right display byte.
        LDN     R2              .. Recover display byte column.
        XRI     #07             .. Test right-edge byte column.
        BZ      DRAWNEXT        .. Skip clipped right byte at edge.
        INC     RC              .. Move to right display byte.
        LDN     R6              .. Read right sprite byte.
        AND                     .. Collision if sprite bits overlap display bits.
        BZ      DRAWRGHT        .. No collision?
        LDI     #01             .. Set collision flag.
        PLO     R7              .. Save collision flag.
DRAWRGHT:
        LDN     R6              .. Read right sprite byte.
        XOR                     .. Toggle right display bits.
        STR     RC              .. Write right display byte.
        DEC     RC              .. Return RC to row start.
DRAWNEXT:
        INC     R6              .. Advance work pointer.
        GLO     RC              .. Current display offset.
        ADI     #08             .. Move one display row down.
        PLO     RC              .. Store new display offset.
        BNF     DRAWLOOP                .. Stop if row step crossed page.
SAVEVF:
        LDI     #FF             .. VF low byte.
        PLO     R6              .. R6 points to VF.
        GLO     R7              .. Get collision flag.
        STR     R6              .. Store VF.
        INC     R2              .. Pop stack byte.
RETURN:
        SEP     R4              .. Return to fetch/decode.

OP_CLS: GHI     RB              .. 00E0: clear the VIP display page.
        PHI     RF              .. RF.1 = display page.
        LDI     #FF             .. RF points to last display byte.
        PLO     RF              .. RF points to last display byte.
CLRLOOP:
        GHI     R3              .. Clear display bytes from the end of the page downward.
        STR     RF              .. Clear byte at RF.
        GLO     RF              .. Check low byte of RF.
        BZ      RETURN          .. Done after clearing offset 0.
        DEC     RF              .. Move backward through display RAM.
        BR      CLRLOOP         .. Loop.

        DB      0               .. Padding.

OP_RET: LDA     R2              .. 00EE: pop return address into the CHIP-8 PC.
        PHI     R5              .. Set CHIP-8 PC high byte.
        LDA     R2              .. Pop return low byte.
        PLO     R5              .. Set CHIP-8 PC low byte.
        SEP     R4              .. Resume at return address.
RSTIPTR:
        GLO     RD              .. Restore I to the start of the sprite before drawing.
        PLO     R7              .. R7.0 counts rows to rewind.
RSTILOP:
        GLO     R7              .. Walk I back by the sprite height.
        BZ      DRAW            .. I reset complete?
        DEC     RA              .. Rewind I one byte.
        DEC     R7              .. Count one rewind step.
        BR      RSTILOP         .. Continue rewinding I.

        DC      0,0,0,0         .. Filler to end of RAM page.

        DC      0,0,0,0,0       .. Filler at start of next page.

OP_FXNN:
        LDA     R5              .. FXNN: second byte selects the FX handler.
        PLO     R3              .. Jump within this page to the selected FX handler.

OP_FX07:
        GHI     R8              .. FX07: copy delay timer to VX.
        STR     R6              .. Store it in VX.
        SEP     R4              .. Return to fetch/decode.

OP_FX0A:
        LDI     #81             .. FX0A: wait for a key and store it in VX.
        PHI     RC              .. Store this in RC.1.
        LDI     #95             .. 0x95 is the low byte of the address of the keyboard routine.
        PLO     RC              .. Put this in RC.0.
        DEC     R2              .. Decrement stack pointer.
        SEP     RC              .. Call the routine to read the keyboard.
        INC     R2              .. Increment stack pointer.
        STR     R6              .. Store the result in VX.
        SEP     R4              .. Return to fetch/decode.

OP_FX15:
        LDN     R6              .. FX15: set delay timer from VX.
        PHI     R8              .. Copy VX into the delay timer.
        SEP     R4              .. Return to fetch/decode.

OP_FX18:
        LDN     R6              .. FX18: set sound timer from VX.
        PLO     R8              .. Copy it into the sound timer (R8.0).
        SEP     R4              .. Return to fetch/decode.
BCDDIVS:
        DC      100,10,1        .. Divisors used by the BCD conversion loop.

OP_FX1E:SEX     R6              .. FX1E: add VX to I.
        GLO     RA              .. Get the low byte of I (stored in RA.0).
        ADD                     .. Add value in VX to low byte of I.
        PLO     RA              .. Store the new low byte of I.
        BNF     SAMEPAGE        .. No carry means I stayed in the same page.
        GHI     RA              .. Get I high byte.
        ADI     #01             .. Add 1 to it (to point to next page).
        PHI     RA              .. Store the updated I high byte.
SAMEPAGE:
        SEP     R4              .. Return when I stayed on the same page.

OP_FX29:
        LDI     A.1(ROM_FONT)   .. FX29: point I at the font sprite for digit VX.
        PHI     RA              .. Store this in the high byte of I (RA.1).
        LDN     R6              .. Get the value in VX (R6).
        ANI     #0F             .. Keep the low digit.
        PLO     RA              .. I now points to the matching font table entry.
        LDN     RA              .. Get the low byte of the sprite address from the look-up table.
        PLO     RA              .. I now points to the start of the data for the correct sprite.
        SEP     R4              .. Return to fetch/decode.

OP_FX33:
        SEX     R6              .. FX33: store BCD(VX) at I..I+2.
        LDN     R6              .. Get the value to be converted from VX.
        PHI     RF              .. Preserve the original value by temporarily storing it in RF.1.
        GHI     R3              .. Get BCD divisor table page.
        PHI     RE              .. Store this in RE.1.
        LDI     A.0(BCDDIVS)            .. Low byte of the BCD divisor table.
        PLO     RE              .. RE now points to first BCD denominator constant.
        DEC     RA              .. Back I up so the loop can pre-increment to the first digit.
BCDLOOP:
        INC     RA              .. Convert one decimal digit.
        LDI     #00             .. Start digit at zero.
        STR     RA              .. Use this to initialise the BCD digit.
DIVLOOP:
        LDN     RE              .. Repeated subtraction forms the current BCD digit.
        SD                      .. Subtract current divisor from VX.
        BNF     NXTDIG          .. Negative result means this digit is complete.
        STR     R6              .. Store the remainder back in VX.
        LDN     RA              .. Get the value of the current BCD digit.
        ADI     #01             .. Add 1 to it.
        STR     RA              .. Store the incremented BCD digit.
        BR      DIVLOOP         .. Continue to divide VX by current denominator.
NXTDIG: LDA     RE              .. Move to the next BCD denominator.
        SHR                     .. test the least significant bit.
        BNF     BCDLOOP         .. Continue until the divisor table reaches 1.
        GHI     RF              .. Get the preserved original value of VX.
        STR     R6              .. Restore this to VX.
        DEC     RA              .. Restore I to the first stored BCD digit.
        DEC     RA
        SEP     R4              .. Return to fetch/decode.

        DC      0               .. Padding.

OP_FX55:
        DEC     R2              .. FX55: store V0..VX starting at I.
        GLO     R6              .. Get the low byte of the VX pointer
        STR     R2              .. Push the saved VX pointer low byte.
        LDI     #F0             .. 0xF0 is the low byte of the address of the first variable (V0).
        PLO     R7              .. Set this as the low byte of the VY pointer (R7).
STORLOOP:
        LDN     R7              .. Store vars until the saved VX pointer is reached.
        STR     RA              .. Store it at I.
        GLO     R7              .. Get the low byte of the address in I.
        XOR                     .. Zero when VY has reached the saved VX pointer.
        INC     R7              .. Point VY to the next variable.
        INC     RA              .. Point I to the next address in memory.
        BNZ     STORLOOP                .. Loop until V0..VX have been stored.
        INC     R2              .. Pop the low byte of VX off the stack.
        SEP     R4              .. Return to fetch/decode.

OP_FX65:
        DEC     R2              .. FX65: load V0..VX starting at I.
        GLO     R6              .. Get the low byte of the VX pointer
        STR     R2              .. Push the saved VX pointer low byte.
        LDI     #F0             .. 0xF0 is the low byte of the address of the first variable (V0).
        PLO     R7              .. Set this as the low byte of the VY pointer (R7).
LOADLOOP:
        LDN     RA              .. Load vars until the saved VX pointer is reached.
        STR     R7              .. Store it in the current variable slot.
        GLO     R7              .. Get the low byte of the address in I.
        XOR                     .. Zero when VY has reached the saved VX pointer.
        INC     R7              .. Point VY to the next variable.
        INC     RA              .. Point I to the next address in memory.
        BNZ     LOADLOOP                .. Loop until V0..VX have been loaded.
        INC     R2              .. Pop the low byte of VX off the stack.
        SEP     R4              .. Return to fetch/decode.

OP_2MMM:
        INC     R5              .. 2MMM: call 0MMM, pushing the next CHIP-8 PC.
        GLO     R5              .. Get the low byte of the address.
        DEC     R2              .. Decrement the stack pointer (R2).
        STXD                    .. Push return low byte and pre-decrement the stack.
        GHI     R5              .. Get the high byte of the address.
        STR     R2              .. Push it onto the stack.
        DEC     R5              .. Rewind PC to the low address byte for OP_1MMM.

OP_1MMM:
        LDA     R5              .. 1MMM: jump to 0MMM.
        PLO     R5              .. Load it into the low byte of the CHIP-8 PC.
        GLO     R6              .. Get the low byte of the VX pointer (R6).
        ANI     #0F             .. We just need to mask it off to get it.
        PHI     R5              .. Set the high byte of the CHIP-8 PC.
NOSKIP: SEP     R4              .. Return without skipping.

OP_3XNN:
        LDA     R5              .. 3XNN: skip next instruction if VX == NN.
TSTEQ:  SEX     R6              .. Compare D with VX.
        XOR                     .. XOR NN with the contents of VX.
        BNZ     NOSKIP          .. Non-zero means VX != NN, so do not skip.
SKIP:   INC     R5              .. Skip first byte of next instruction.
        INC     R5              .. Skip second byte of next instruction.
        SEP     R4              .. Return to fetch/decode.

OP_4XNN:
        LDA     R5              .. 4XNN: skip next instruction if VX != NN.
TSTNE:  SEX     R6              .. Compare D with VX for inequality.
        XOR                     .. XOR NN with the contents of VX.
        BNZ     SKIP            .. Non-zero means VX != NN, so skip.
        SEP     R4              .. Return to fetch/decode.

OP_9XY0:
        LDA     R5              .. 9XY0: skip if VX != VY.
        LDN     R7              .. Load VY into D.
        BR      TSTNE           .. Branch to the test for inequality.

OP_5XY0:
        LDA     R5              .. 5XY0: skip if VX == VY.
        LDN     R7              .. Load VY into D.
        BR      TSTEQ           .. Branch to the test for inequality.

OP_KEY: SEX     R6              .. EX9E/EXA1: skip based on the current key state.
        OUT     2               .. Output VX to the keyboard latch.
        DEC     R6              .. Undo OUT's auto-increment of the selected register.
        LDA     R5              .. Fetch the second opcode byte and advance PC.
        PLO     R3              .. Use 9E/A1 as the next handler address.
        B3      SKIP            .. If selected key is pressed, skip.
        SEP     R4              .. Return to fetch/decode.
        BN3     SKIP            .. If selected key is not pressed, skip.
        SEP     R4              .. Return to fetch/decode.

OP_BMMM:
        LDI     #F0             .. BMMM: jump to 0MMM + V0.
        PLO     R7              .. Load this into the VY pointer (R7).
        SEX     R7              .. Set the VY pointer to be used for indirect addressing.
        LDA     R5              .. Get the low byte of the current CHIP-8 instruction.
        ADD                     .. Add V0 to the low address byte.
        PLO     R5              .. Load this into the CHIP-8 PC.
        GLO     R6              .. VX pointer has the branch high byte.
        ANI     #0F             .. Clear the top nibble.
        BNF     STORHI          .. No carry means the high byte is unchanged.
        ADI     #01             .. Carry means advance the high address byte.
STORHI:
        PHI     R5              .. Load the high byte of the address into the CHIP-8 PC.
        SEP     R4              .. Return; the next fetch uses the branched PC.

OP_6XNN:
        LDA     R5              .. 6XNN: set VX = NN.
        STR     R6              .. Store the value in VX.
        SEP     R4              .. Return to fetch/decode.

OP_7XNN:
        LDA     R5              .. 7XNN: add NN to VX.
        SEX     R6              .. Use VX pointer for indirect addressing.
        ADD                     .. Add immediate NN to VX.
        STR     R6              .. Store the result back in VX.
        SEP     R4              .. Return to fetch/decode.

OP_8XYN:
        LDA     R5              .. 8XYN: ALU ops on VX and VY, DF is 0 on entry (see call)
        ANI     #0F             .. Mask the byte to save just the second hex digit.
        BNZ     #C4             .. Non-zero N selects an ALU operation.
        LDN     R7              .. N=0 is 8XY0, so copy VY into VX.
        STR     R6              .. Copy this into VX.
        SEP     R4              .. Return to fetch/decode.
DECODEAL:
        PLO     RF              .. Temporarily save the last digit of the instruction in RF.0.
        DEC     R2              .. Decrement the stack pointer, ready for a push operation.
        LDI     #D3             .. Put a SEP 3 return instruction in D.
        STXD                    .. Push this onto the stack and decrement the stack pointer.
        GLO     RF              .. Restore the CHIP-8 ALU sub-op to D.
        ORI     #F0             .. Build a 1802 ALU opcode from CHIP-8 N.
        STR     R2              .. Push this onto the stack
        SEX     R6              .. Use VX pointer for indirect addressing.
        LDN     R7              .. Load VY into D.
        SEP     R2              .. Execute generated ALU opcode, then SEP 3 returns here.
        STR     R6              .. Save the result of the operation in VX.
        LDI     #FF             .. 0xFF is the low byte of the address of CHIP-8 variable VF.
        PLO     R6              .. The VX pointer now points to VF.
        LDI     #00             .. Clear D.
        SHLC                    .. Move carry into bit 0 of D.
        STR     R6              .. Save this in VF. (VF will be 0 for 8xy1/2/3 with unchanged DF)
        SEP     R4              .. Return to fetch/decode.

OP_CXNN:
        INC     R9              .. CXNN: pseudo-random byte AND NN into VX.
        GLO     R9              .. Get the low byte of the random number seed.
        PLO     RE              .. Save this in RE.0.
        GHI     R3              .. Get the high byte of the interpreter PC (This will be 0x01).
        PHI     RE              .. Put this in RE.1.
        GHI     R9              .. Get the high byte of the random number seed.
        SEX     RE              .. Use RE for indirect addressing.
        ADD                     .. Mix in a byte from interpreter code.
        STR     R6              .. Store this in VX.
        SHRC                    .. Shift the result one bit to the right.
        SEX     R6              .. Use VX pointer for indirect addressing.
        ADD                     .. Add VX to the shifted value in D.
        PHI     R9              .. Save this as the new high byte of the random number seed.
        STR     R6              .. Put this value in VX.
        LDA     R5              .. Get second byte of CHIP-8 instruction and advance PC.
        AND                     .. Use this to mask the random number in VX.
        STR     R6              .. Put final value in VX.
        SEP     R4              .. Return to fetch/decode.

OP_AMMM:
        LDA     R5              .. AMMM: set I = 0MMM.
        PLO     RA              .. Set this as the low byte of the address in I (RA).
        GLO     R6              .. VX pointer still carries the high address nibble.
        ANI     #0F             .. Set the first hex digit to 0x0.
        PHI     RA              .. Set this as the high byte of the address in I.
        SEP     R4              .. Return to fetch/decode.

        DC      0,0,0,0         .. Padding.
        DC      0,0,0,0
        DC      0,0

CHIPSTRT:
        DC      #00, #E0                .. Startup CHIP-8 code: clear display.
        DC      #00, #4B                .. Call VIP display-on routine.
