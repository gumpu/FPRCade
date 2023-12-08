## FCPU

FCPU instruction set Version 0.001

All instructions are 16 Bit.

Bits 15 to 12 indicate the instruction category, the remaining bits encode the
function, stack id, signed/unsigned, and values.

The processor has four stacks for: data, return addresses, control data, and temporary values.

They are indicated by:  D, R, C, and T.

It supports the following arithmetic operations (signed and unsigned): + - / * %


The following bit operations (unsigned): >> << | & ^

The following bit operations (signed): >>

The following logical operations: < > <= >= == !=

Stack: drop swap dup over pop push pick

Memory (byte / word / long)

    load
    store

Values

    load   (push immediate)

Branches: enter, leave, branch if false

Control: nop halt

## Opcodes

BIF ENTER NOP LEAVE HALT DROP DUP SWAP MOV LDL LDH ADDU ADD MUL MULU EQ ASRU
ASR LTU LT GTU GT LTEU LTE GTEU GTE LSL LSR STO AND OR XOR NEG NOT RD

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
    .   0000         DROP Drop first cell on ss
    .   0001         DUP  Duplicate first cell on ss
    .   0010         SWAP Swap first two cells on ss
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
    1111ffffxxxxxxxx Operates on data stack (1 values)
    .   0000xxxxxxxx NEG
    .   0001xxxxxxxx NOT
    .   0010xxxxxzzz RD   Store one/two/four bytes

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
vi: spell spl=en
