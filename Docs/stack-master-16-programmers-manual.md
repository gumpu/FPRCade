---
title: Stackmaster-16 Programmer's Manual
author: Frans Slothouber
---


# Introduction

Stackmaster-16 is a 16 bit stack based processor. It was designed to make
it easy to build a Forth system.

The processor is little endian.  All instructions are 16 Bit.  Bits 15 to 12
indicate the instruction category, the remaining bits are used to encode the
function, stack id, signed/unsigned, and/or values.

The processor has four stacks. One for data, return addresses, control data,
and temporary values.  These are indicated by the letters:  D, R, C, and T.
The stack depths are:

- Data, 8 deep
- Return, 64 deep
- Control, 16 deep
- Temp, 4 deep

These stacks do not reside in memory.

It supports the following arithmetic operations (signed and unsigned): `+ - *`

The following bit operations (unsigned): `>> << | & ^`

The following bit operations (signed): `>>`

The following comparison operations: `< > <= >= == !=`

Stack: drop swap dup pop push

Memory (byte / word / long)

    read
    store

Values

    load   (push immediate)

Branches: enter, leave, branch if false

Control: nop halt reset

## Opcodes

BIF ENTER NOP LEAVE HALT DROP DUP SWAP MOV LDL LDH ADDU ADD MUL MULU EQ ASRU
ASR LTU LT GTU GT LTEU LTE GTEU GTE LSL LSR STO AND OR XOR NEG NOT RD
RESET

## Memory Map

    Page 0
    0x0000  value to load into PC after warm reset
    0x0002  pointer to stack overflow exception code
    0x0004  pointer to stack underflow exception code
    0x0006  pointer to illegal instruction exception code
    0x0008
    0x00FF

    0x0100  User RAM (61184 bytes)
    0xEFFF  End of user RAM

    0xF000  Boot ROM  (4K)
    0xFFFF  End of Boot ROM

## IO Map

    0x0000  Serial output
    0x0001  Serial output status
    0x0002  Serial input
    0x0003  Serial input status
    0x0100  Graphics RAM
    0xFFFF

## Assembly format

An assembly file contains five type of lines:

1. A remark, this is a line that starts with a ';'
2. A label, this a line that starts with a word ending with a ':'
3. A directive, this is a line that starts with a '.'
4. An empty line, a line with containing only spaces.
5. An instruction, a line that start with a word that matches one the
  possible instructions of the Stackmaster-16.


The following is an example

    ; Test

    .code
        nop
        ldl d 0x0000
    label1:
        ldl d 0x0001
        add d
        dup d
        ldl d 0x000A
        gt
        bif label1
        halt
    end:

    .str "some string"

### Directives

The following directives are supported:
    .align
    .b
    .w
    .l
    .str
    .def
    .set

### .align

Align the next address to be word or long aligned.
Examples:

    .align w
    .align l

### .b .w .l

### .str

### .def

Define a new symbol

### .set

Set an existing symbol to a new value


### Numbers

When converting input numbers, the text interpreter shall recognize
integer numbers in the form <anynum>.

    <anynum> := { <decnum> | <hexnum> | <binnum> | <cnum> }
    <BASEnum> := [-]<bdigit><bdigit>*
    <decnum> := #[-]<decdigit><decdigit>*
    <hexnum> := $[-]<hexdigit><hexdigit>*
    <binnum> := %[-]<bindigit><bindigit>*
    <cnum> := ’<char>’
    <bindigit> := { 0 | 1 }
    <decdigit> := { 0 | 1 | 2 | 3 | 4 | 5 | 6 | 7 | 8 | 9 }
    <hexdigit> := { <decdigit> | a | b | c | d | e | f | A | B | C | D | E | F }

    For <hexdigit>, the digits a .. f have the values 10 .. 15.

    <char> represents any printable character.

    The radix used for number conversion is:
    <decnum>       10
    <hexnum>       16
    <binnum>       2
    <cnum>         the number is the value of <char>



## Terms and definitions

Cell

Instruction

Opcode

Value

Stack

## Opcode encoding

List of instructions and opcodes

    Bit
    1111 1100 0000 0000
    5432 1098 7654 3210
    |||| |||| |||| ||||
    0000 xxxx xxxx xxxx unused
    0001 dddd dddd dddd BIF     branch if false
    0010 xxxx xxxx xxxx unused
    0011 xxxx xxxx xxxx unused
    01dd dddd dddd dddd ENTER   Enter a subroutine
    1000 ffff xxxx xxxx
    .    0000 xxxx xxxx NOP
    .    0001 xxxx xxxx LEAVE
    .    0010 xxxx xxxx HALT
    .    0011 xxxx xxxx RESET
    1001 xxxx xxxx xxxx unused
    1010 xxxx xxxx xxxx unused
    1011 ffff sstt xxxx Stack operations
    .    0000           DROP Drop first cell of ss
    .    0001           DUP  Duplicate first cell of ss
    .    0010           SWAP Swap first two cells of ss
    .    0011           MOV  Move from ss to tt
    1100 ssdd dddd dddd LDL  Load immediate low 10 bits to ss
    1101 ssxx xxdd dddd LDH  Load immediate high 6 bits to ss

    1111 1100 0000 0000
    5432 1098 7654 3210
    |||| |||| |||| ||||
    1110 ffff fixp xxxx Operates on data stack (2 values)
    .    0000 0xx0 xxxx ADDU Unsigned 16 bit addition
    .    0000 0xx1 xxxx ADD  Signed 16 bit addition
    .    0000 1xx0 xxxx MULU Unsigned 16 bit multiplication
    .    0000 1xx1 xxxx MUL  Signed 16 bit multiplication
    .    0001 0xx0 xxxx reserved for DIVU
    .    0001 0xx1 xxxx reserved for DIV
    .    0001 1xxx xxxx EQ   Equal
    .    0010 0xx0 xxxx illegal
    .    0010 0xx1 xxxx ASR  Signed shift right, extends sign
    .    0010 1xx0 xxxx LTU  Unsigned less than
    .    0010 1xx1 xxxx LT   Signed less than
    .    0011 0xx0 xxxx GTU  Unsigned greater than
    .    0011 0xx1 xxxx GT   Signed greater than
    .    0011 1xx0 xxxx LTEU Unsigned less than or equal
    .    0011 1xx1 xxxx LTE  Signed less than or equal
    .    0100 0xx0 xxxx GTEU Unsigned greater than or equal
    .    0100 0xx1 xxxx GTE  Signed greater than or equal
    .    0100 1xxx xxxx LSL  Shift left
    .    0101 0xxx xxxx LSR  shift right
    .    0101 10xx xzzz STO  Store one/two/four bytes
    .    0101 11xx xzzz ISTO Store one/two/four bytes to IO Port
    .    0110 0xxx xxxx
    .    0110 1xxx xxxx AND  16 bit and
    .    0111 0xxx xxxx OR   16 bit or
    .    0111 1xxx xxxx XOR  16 bit exclusive or
    1111 ffff xxxx xxxx Operates on data stack (1 value)
    .    0000 xxxx xxxx NEG
    .    0001 xxxx xxxx NOT
    .    0010 0xxx xzzz RD   Read one/two/four bytes
    .    0010 1xxx xzzz IRD  Read one/two/four bytes from IO Port

### Field names

The meaning of the field names is as follows:

    p   - 1/0  signed/unsigned
    ddd - data
    ss  - source stack 11/10/01/00
    tt  - target stack 11/10/01/00
       00 Data Stack
       01 Return Stack
       10 Control Stack
       11 Temp Stack
    ffff - function
    xxx  - don't care
    zzz  - size
      000 illegal
      001 byte
      010 2 bytes
      011 illegal
      100 4 bytes
    i   - 0/1  ram/io-port


## Instruction Details

### Notation:

The following notation is used do distinguish the various
value types:

- s1, s2, .., sn  -- signed 16 bit numbers
- u1, u2, .., un  -- unsigned 16 bit numbers
- a1, a2, .., an  -- 16 bit address
- t1, t2, .., tn  -- 16 bit truth value
- n1, n2, .., nn  -- 16 bits uninterpreted

### ADD - Signed addition

`ADD (s1 s2 -- s3)`

Signed addition: s3 = s1 + s2

Assembler:

    add

### ADDU - Unsigned addition

`ADDU (u1 u2 -- u3)`

Unsigned addition: u3 = u1 + u2

Assembler:

    addu

### AND - Bitwise and

`AND (n1 n2 -- n3)`

Bitwise and: n3 = n1 & n2

Assembler:

    and

### ASR - Arithmetical shift right

Arithmetical shift right.

`ASR (s1 u1 -- s2)`

Assembler:

    asr

### BIF - Branch if false

Encoding: `0001dddddddddddd`

`BIF (tf -- )`

Branch to a new address if the top value on the data stack has the value false
(0).  The new address is computed from the current PC and the offset given in
the instruction. Where offset is in the range [-2048, 2047]

Assembler:

    bif <label>
    bif <address>

### DROP - Drop value

`DROP (x: u1 -- x: )`

Drop the top element from the given stack.

Assembler:

    drop d
    drop r
    drop c
    drop t


### DUP - Duplicate value

`DUP (x: u1 -- x: u1 u1 )`

Duplicate the top element from the given stack.

Assembler

    dup d
    dup r
    dup c
    dur t

### ENTER - Enter a subroutine

`ENTER ( -- r: a1)`

The location encoded in the instruction is a 14 bit value. This value is
multiplied by 4 to compute the destination address.  The value of PC + 2 is
pushed to the return stack and destination address is loaded into the PC.

Assembler:

    enter <label>
    enter <address>

### EQ -- Are two top values equal

`EQ   (u1 u2 -- t1)`

Assembler:

    eq

### GT - signed greater than

`GT   (u1 u2 -- t1)`

Assembler:

    gt

### GTE - signed greater than or equal

`GTE  (u1 u2 -- t1)`

### GTE - unsigned greater than or equal

`GTEU (u1 u2 -- t1)`

### GTU - unsigned greater than

`GTU  (u1 u2 -- t1)`

### HALT -


### LDH -

`LDH  (u1 -- u2)`

### LDL -

`LDL  (   -- u1)`

### LEAVE - Leave a subroutine

`LEAVE (r: a1 -- )`

Pop the top value from the return stack and loads this value into the PC.

### LSL - Logical shift left

`LSL (n2 u1 -- n3)`

### LSL - Logical shift right

`LSR (n2 u1 -- n3)`

### LT - signed less than

`LT  (s1 s2 -- tf)`

### LTE - signed less than or equal

`LTE   (s1 s2 -- tf)`

### LTEU - unsigned less than or equal

`LTEU  (u1 u2 -- tf)`

### LTU - unsigned less than

`LTU  (u1 u2 -- tf)`

### MOV - Move value from one stack to the other

`MOV  (x: n1  y:  --  x: y: n1)`

Assembler:

    mov r d
    mov d r
    mov t c
    ...

### MUL - Signed multiplication

`MUL`

Assembler:

    mul

### MULU - Unsigned multiplication

`MULU`

Assembler:

    mulu

### NEG - 2 Complements

`NEG (n1 -- n2)`

### NOP - Do nothing

`NOP ( -- )`

### NOT - Invert bits

`NOT (n1 -- n2)`

### OR - Bitwise OR

`OR (n1 n2 -- n3)`

Bitwise or: n3 = n1 | n2

### IRD - Read n bytes from an IO port

`IRD b  (a1 -- n1)`

`IRD w  (a1 -- n1)`

`IRD l  (a1 -- n1 n2)`

### RD - Read n bytes from memory

Read a byte

`RD b  (a1 -- n1)`

Read a word

`RD w  (a1 -- n1)`

Read a long

`RD l  (a1 -- n1 n2)`

### STO - Store n bytes in memory

`STO b (n1 a1 -- )`

Stores the lower 8 bits of n1 at address a1.

`STO w (n1 a1 -- )`

Stores n1 at address locations a1 and a1+1.

`STO l (n1 n2 a1 -- )`

Stores n1 and n2 at addresses a1, a1+1, a1+2, and a1+3.

### ISTO - Store n bytes in an IO port


### SWAP - Swap the two top stack elements of the given stack

`SWAP (n1 n2 -- n2 n1)`

### RESET - Reset the processor state


### XOR - Exclusive Or

Perform an exclusive or on the two top stack elements of the given stack.

`XOR  (n1 n2 -- n3)`

vi: spell spl=en
