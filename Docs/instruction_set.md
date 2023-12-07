16 Bit instructions

65536  possible instructions.

Stacks:  Data  Return   Control   Temp

|15|14|13|12|11|10|09|08|07|06|05|04|03|02|01|00|
|  |  |  |  |  |  |  |  |  |  |  |  |  |  |  |  |


Arithmetic Operations (signed and unsigned)

    +
    -
    /
    *
    %

Bit operations (unsigned)

    >>
    <<
    |
    &
    ^

Logical

    < > <= >= == !=

Stack

    drop
    swap
    dup
    over
    pop
    push
    pick

Memory (byte / word)

    load
    store

Values

    load   (push immediate)

Branches

    enter leave branch_if_false

Control

    nop
    halt
    trap



## Opcodes

    Opcodes:
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
       .   00100xx0xxxx SLU  Unsigned shift left
       .   00100xx1xxxx SL   Signed shift left, extends sign
       .   00101xx0xxxx LTU  Unsigned less than
       .   00101xx1xxxx LT   Signed less than
       .   00110xx0xxxx GTU  Unsigned greater than
       .   00110xx1xxxx GT   Signed greater than
       .   00111xx0xxxx LTEU Unsigned less than or equal
       .   00111xx1xxxx LTE  Signed less than or equal
       .   01000xx0xxxx GTEU Unsigned greater than or equal
       .   01000xx1xxxx GTE  Signed greater than or equal
       .   01001xxxxxxx SR   Shift right
       .   01010xxxxxxx STB  Store a byte
       .   01011xxxxxxx STW  Store two bytes
       .   01100xxxxxxx STL  Store four bytes
       .   01101xxxxxxx AND  16 bit and
       .   01110xxxxxxx OR   16 bit or
       .   01111xxxxxxx XOR  16 bit exclusive or
       1111ffffxxxxxxxx Operates on data stack (1 values)
       .   0000xxxxxxxx NEG
       .   0001xxxxxxxx NOT

### Field names

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

