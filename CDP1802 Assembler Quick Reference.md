
# CDP1802 Assembly Quick Reference

A compact reference for the RCA-style **COSMAC Level I Assembly Language**, as used by the **COSMAC Resident Assembler (CRA)** / RCA 1802 development-system tooling.

This is based on the gathered references around RCA MPM-216 / COSMAC Development System II material and the reconstructed source examples.

---

## 1. Source layout

A source line may contain one or more statements.

```asm
LABEL:  OPCODE OPERAND   .. comment
````

Multiple statements may appear on one line, separated by semicolons:

```asm
        LDI #00 ; PLO R2 ; PHI R2
```

Comments begin with:

```asm
..
```

Example:

```asm
START:  LDI #05      .. load immediate value 5
        PLO R1       .. put D into low byte of R1
```

---

## 2. Labels and symbols

Labels are symbols placed at the beginning of a statement and followed by a colon:

```asm
START:
LOOP:
HANDLER:
```

Example:

```asm
START:  LDI #05
LOOP:   DEC R2
        BNZ LOOP
```

Symbols may also be defined with equates:

```asm
COUNT = 1
WRAM  = #8C1F
```

Example from RCA-style code:

```asm
PTER=#00
AUX=#0E
CHAR=#0F
WRAM=#8C1F
LOADER=#8400
```

### Note on allowed label characters

The gathered summary does **not** give a definitive character set or maximum symbol length.

For maximum historical compatibility, use uppercase alphanumeric symbols beginning with a letter:

```text
[A-Z][A-Z0-9]*
```

Avoid `_`, `.`, `$`, and other punctuation unless testing confirms your assembler accepts them.

---

## 3. Numeric constants

Several constant forms are supported.

### Hexadecimal

```asm
#1234
X'1234'
```

Examples:

```asm
        LDI #20
WRAM = #8C1F
```

### Decimal

```asm
D'1234'
```

Example:

```asm
COUNT = D'10'
```

### Binary

```asm
B'01010101'
```

Example:

```asm
MASK = B'11110000'
```

### Text

```asm
T'text'
```

Example:

```asm
MSG:    DC T'HELLO'
```

---

## 4. Expressions

Expressions are simple. The known supported forms include:

```asm
symbol
constant
symbol + constant
symbol - constant
```

Examples:

```asm
WRAM-1
UT20A+2
```

---

## 5. Address-byte extraction

The assembler supports operators for extracting the high or low byte of an address expression.

### High byte

```asm
A.1(label)
```

Returns the most significant byte of the expression.

Example:

```asm
        LDI A.1(UT20)
```

### Low byte

```asm
A.0(label)
```

Returns the least significant byte of the expression.

Example:

```asm
        LDI A.0(UT20)
```

### Full address

```asm
A(label)
```

Forces a full 16-bit address value.

Example:

```asm
ADDR:   DC A(HANDLER)
```

### Jump-table style example

High-byte first:

```asm
JTAB:   DC A.1(HAND0), A.0(HAND0)
        DC A.1(HAND1), A.0(HAND1)
        DC A.1(HAND2), A.0(HAND2)
```

Low-byte first:

```asm
JTAB:   DC A.0(HAND0), A.1(HAND0)
        DC A.0(HAND1), A.1(HAND1)
        DC A.0(HAND2), A.1(HAND2)
```

---

## 6. Directives

### `ORG`

Sets the location counter.

```asm
        ORG #8000
```

Example:

```asm
        ORG #8000
START:  LDI #00
```

`ORG` may also use the current location counter `*`:

```asm
        ORG *+#01
```

---

### `DC`

Defines constant data bytes.

```asm
LABEL:  DC expression, expression, expression
```

Examples:

```asm
DATA:   DC #01, #02, #03
MSG:    DC T'HELLO'
ADDR:   DC A.1(HANDLER), A.0(HANDLER)
```

Constants greater than 255 may generate two bytes.

Use `A(expr)` when you explicitly want a 16-bit value:

```asm
PTR:    DC A(HANDLER)
```

Use `A.1(expr)` and `A.0(expr)` when you want explicit byte control:

```asm
PTR:    DC A.1(HANDLER), A.0(HANDLER)
```

---

### `PAGE`

Starts or controls page formatting in listings.

```asm
        PAGE
```

This is mainly a listing/output directive, not a machine-code operation.

---

### `END`

Marks the end of the assembly source.

```asm
        END
```

Example:

```asm
START:  BR START
        END
```

---

## 7. Location counter

The current assembly address is represented by:

```asm
*
```

Example:

```asm
        ORG *+#01
```

This advances the current location by one byte.

---

## 8. Register naming

The CDP1802 has sixteen 16-bit registers:

```asm
R0  R1  R2  R3
R4  R5  R6  R7
R8  R9  RA  RB
RC  RD  RE  RF
```

RCA-style source also often defines symbolic register names:

```asm
PC  = R3
SUB = R4
CHAR = RF
```

Or numeric aliases, depending on assembler syntax and local convention:

```asm
PTER=#00
AUX=#0E
CHAR=#0F
```

Example use:

```asm
        PHI R0
        PLO R1
        SEP R3
```

---

## 9. Common CDP1802 instruction examples

### Load immediate

```asm
        LDI #20
```

### Put D into register high/low byte

```asm
        PHI R1
        PLO R1
```

### Get register high/low byte into D

```asm
        GHI R1
        GLO R1
```

### Branch

```asm
        BR LOOP
        BNZ LOOP
        BZ DONE
```

### Long branch

```asm
        LBR TARGET
```

### Decrement register

```asm
        DEC R2
```

### Set program counter / call style

```asm
        SEP R3
```

---

## 10. Common address-loading idiom

To load a 16-bit address into an 1802 register:

```asm
        LDI A.1(TARGET)
        PHI R3
        LDI A.0(TARGET)
        PLO R3
```

Example:

```asm
        LDI A.1(MAIN)
        PHI R3
        LDI A.0(MAIN)
        PLO R3
        SEP R3
```

---

## 11. Data embedded after instructions

Some RCA-style 1802 code uses inline constants following an instruction sequence.

Example style:

```asm
        LDI #20
        PHI R1
        LDI #00
        PLO R1
```

Or data lists:

```asm
TABLE:  DC #10, #20, #30, #40
```

Text:

```asm
MSG:    DC T'READY'
```

Address bytes:

```asm
VECT:   DC A.1(ROUTINE), A.0(ROUTINE)
```

---

## 12. Minimal complete example

```asm
        ORG #0200

PC      = R3
COUNT   = R2

START:  LDI A.1(MAIN)
        PHI PC
        LDI A.0(MAIN)
        PLO PC
        SEP PC

MAIN:   LDI #05
        PLO COUNT

LOOP:   DEC COUNT
        GLO COUNT
        BNZ LOOP

DONE:   BR DONE

        END
```

---

## 13. Jump table example

```asm
        ORG #0300

JTAB:   DC A.1(HAND0), A.0(HAND0)
        DC A.1(HAND1), A.0(HAND1)
        DC A.1(HAND2), A.0(HAND2)

HAND0:  BR HAND0
HAND1:  BR HAND1
HAND2:  BR HAND2

        END
```

---

## 14. RCA-style vector table using long branches

A common pattern is a table of branch stubs:

```asm
        ORG #83F0

OSTRNG: LBR MSGE
INIT1:  LBR DSKGO1
INIT2:  LBR DSKGO2
GOUT20: LBR ENTER
CHHEX:  LBR CKHXE

        END
```

This style appears in the longer UT20 monitor listing.

---

## 15. Practical compatibility notes

Different 1802 assemblers use different syntax.

This RCA/COSMAC Level I style uses:

```asm
A.1(label)
A.0(label)
A(label)
#1234
T'text'
..
```

Many later cross-assemblers use other conventions, such as:

```asm
HIGH(label)
LOW(label)
>label
<label
.label
; comment
```

So do not assume RCA Level I source will assemble unchanged with a modern 1802 assembler unless that assembler explicitly supports the CRA / Level I dialect.

---

## 16. Known useful reference source

The most useful long example found so far is the UT20 monitor listing from the COSMAC Development System II material.

It demonstrates:

* `ORG`
* `END`
* equates
* labels
* comments with `..`
* `A.1(...)`
* `A.0(...)`
* `A(...)`
* text constants
* long branches
* monitor/vector-table style code
* real RCA-style formatting


