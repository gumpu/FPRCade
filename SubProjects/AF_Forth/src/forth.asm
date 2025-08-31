; vi: ft=smasm ts=2

; Stack Master 16 FORTH system
;

.def serial_out $0000

hello:
.str "hello world"
.b   $0a $0d
hello_end:

; Boot ROM
.org $F000
    ldl   d hello
loop:
    dup   d
    rd    b
    ldl   d serial_out
    swap  d
    isto  b
    ldl   d 1
    add
    dup   d
    ldl   d hello_end
    eq
    bif   loop
    drop  d
    halt

; (address value - )
to_hex:
    leave

; -------------------- end of file -----------------------------------------
