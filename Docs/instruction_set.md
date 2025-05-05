Title

Stackmaster-16 instruction set Version 0.001

## Stackmaster-16


Stackmaster-16 is a 16 bit stack based processor. It was designed to make
it easy to build a Forth system with it.

The processor is little endian.

All instructions are 16 Bit.  Bits 15 to 12 indicate the instruction category,
the remaining bits are used to encode the function, stack id, signed/unsigned,
and/or values.

The processor has four stacks for: data, return addresses, control data, and
temporary values.  They are indicated by the letters:  D, R, C, and T.

The stack depths are:

- Data, 8 deep
- Return, 64 deep
- Control, 16 deep
- Temp, 4 deep

These stacks do not reside in memory.


It supports the following arithmetic operations (signed and unsigned): + - / * %

The following bit operations (unsigned): >> << | & ^

The following bit operations (signed): >>

The following logical operations: < > <= >= == !=

Stack: drop swap dup over pop push pick

Memory (byte / word / long)

    read
    store

Values

    load   (push immediate)

Branches: enter, leave, branch if false

Control: nop halt

## Opcodes

BIF ENTER NOP LEAVE HALT DROP DUP SWAP MOV LDL LDH ADDU ADD MUL MULU EQ ASRU
ASR LTU LT GTU GT LTEU LTE GTEU GTE LSL LSR STO AND OR XOR NEG NOT RD


## Assembly format

An assembly file contains five type of lines:

-1 A remark, this is a line that starts with a ';'
-2 A label, this a line that starts with a word ending with a ':'
-3 A directive, this is a line that starts with a '.'
-4 An empty line, a line with containing only spaces.
-5 An instruction, a line that start with a word that matches one the
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



## Terms and definitions

Cell

Instruction

Opcode

Value

Stack

## Opcode encoding

List of instructions and opcodes

    Bit
    1111110000000000
    5432109876543210
    ||||||||||||||||
    0000xxxxxxxxxxxx unused
    0001dddddddddddd BIF     branch if false
    0010xxxxxxxxxxxx unused
    0011xxxxxxxxxxxx unused
    01dddddddddddddd ENTER
    1000ffffxxxxxxxx
    .   0000xxxxxxxx NOP
    .   0001xxxxxxxx LEAVE
    .   0010xxxxxxxx HALT
    1001xxxxxxxxxxxx unused
    1010xxxxxxxxxxxx unused
    1011ffffssttxxxx Stack operations
    .   0000         DROP Drop first cell of ss
    .   0001         DUP  Duplicate first cell of ss
    .   0010         SWAP Swap first two cells of ss
    .   0011         MOV  Move from ss to tt
    1100ssdddddddddd LDL  Load immediate low 10 bits to ss
    1101ssddddddxxxx LDH  Load immediate high 6 bits to ss
    1110fffffxxpxxxx Operates on data stack (2 values)
    .   00000xx0xxxx ADDU Unsigned 16 bit addition
    .   00000xx1xxxx ADD  Signed 16 bit addition
    .   00001xx0xxxx MULU Unsigned 16 bit multiplication
    .   00001xx1xxxx MUL  Signed 16 bit multiplication
    .   00010xx0xxxx
    .   00010xx1xxxx
    .   00011xxxxxxx EQ   Equal
    .   00100xx0xxxx ASRU Unsigned shift right
    .   00100xx1xxxx ASR  Signed shift right, extends sign
    .   00101xx0xxxx LTU  Unsigned less than
    .   00101xx1xxxx LT   Signed less than
    .   00110xx0xxxx GTU  Unsigned greater than
    .   00110xx1xxxx GT   Signed greater than
    .   00111xx0xxxx LTEU Unsigned less than or equal
    .   00111xx1xxxx LTE  Signed less than or equal
    .   01000xx0xxxx GTEU Unsigned greater than or equal
    .   01000xx1xxxx GTE  Signed greater than or equal
    .   01001xxxxxxx LSL  Shift left
    .   01010xxxxxxx LSR  shift right
    .   01011xxxxzzz STO  Store one/two/four bytes
    .   01100xxxxxxx
    .   01101xxxxxxx AND  16 bit and
    .   01110xxxxxxx OR   16 bit or
    .   01111xxxxxxx XOR  16 bit exclusive or
    1111ffffxxxxxxxx Operates on data stack (1 value)
    .   0000xxxxxxxx NEG
    .   0001xxxxxxxx NOT
    .   0010xxxxxzzz RD   Read one/two/four bytes

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

## Details

Notation:

- s1, s2, .., sn  -- signed 16 bit numbers
- u1, u2, .., un  -- unsigned 16 bit numbers
- a1, a2, .., an  -- 16 bit address
- t1, t2, .., tn  -- 16 bit truth value
- n1, n2, .., nn  -- 16 bits uninterpreted

### ADD - Signed addition

Signed addition: s3 = s1 + s2

(s1 s2 -- s3)

### ADDU - Unsigned addition

Unsigned addition: u3 = u1 + u2

(u1 u2 -- u3)

### AND - Bitwise and

Bitwise and: n3 = n1 & n2

(n1 n2 -- n3)

### ASR - Arithmetical shift right

Arithmetical shift right.

(n1 -- n2)


### ASRU - Unsigned arithmetical shift right

ASRU

### Branch if false - BIF

Branch to a new address if the top value on the data stack has the value false
(0).  The new address is computed from the current PC and the offset given in
the instruction.

    BIF offset

Where offset is in the range [-2048, 2047]


### Drop value - DROP

(x: u1 -- x: )

Drop the top element from the given stack.


### Duplicate value - DUP

Duplicate the top element from the given stack.


### Enter a subroutine - ENTER

ENTER ( -- r: a1)

    ENTER offset

### EQ -- Are two top values equal

EQ   (u1 u2 -- t1)

### GT - signed greater than

GT   (u1 u2 -- t1)

### GT - signed greater than or equal

GTE  (u1 u2 -- t1)

GTEU (u1 u2 -- t1)

GTU  (u1 u2 -- t1)

### HALT -

LDH  (u2 -- u2)

LDL  (   -- u2)

### LEAVE - Leave a subroutine

LEAVE (r: a1 -- )


### LSL - Logical shift left

LSL (u1 -- u2)

### LSL - Logical shift right

LSR (u1 -- u2)

### LT - signed less than

LT  (s1 s2 -- tf)

### LTE - signed less than or equal

LTE   (s1 s2 -- tf)

### LTEU - unsigned less than or equal

LTEU  (u1 u2 -- tf)

### LTU - unsigned less than

LTU  (u1 u2 -- tf)

## MOV - Move value from one stack to the other

MOV  (x: x1 -- y: x1)

## MUL - Signed multiplication

MUL

## MULU - Unsigned multiplication

MULU

##

NEG (n1 -- n2)

## Do nothing -- NOP

NOP ( -- )


### NOT - Invert bits or truth value

NOT:

(n1 -- n2)

### OR - Bitwise OR

Bitwise or: n3 = n1 | n2

(n1 n2 -- n3)


### RD - Read n bytes from memory

RD.b  (a1 -- u1)

RD.w  (a1 -- u1)

RD.l  (a1 -- u1 u2)

## STO - Store n bytes in memory

STO.b (n1 a1 -- )

Stores the lower 8 bits of n1 at address a1.

STO.w (n1 a1 -- )

Stores n1 at address a1.

STO.l (n1 n2 a2 -- )

Stores n1 and n2 at addresses a1 and a1+2.

### SWAP - Swap the two top stack elements of the given stack

SWAP (n1 n2 -- n2 n1)


### XOR - Exclusive or on two top stack elements

XOR  (n1 n2 -- n3)



vi: spell spl=en
