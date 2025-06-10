; vi: ft=smasm
; Test
;
    ; Space should be skipped
    ; Also for directives

    .str "123"
    .str "'test'"
    .str "0000" "1111"
    .align l
    .str "test"
    .def test1 42
    .b 'a' 255 -1 128 0 %1001

; Label inside a string
.str "label1:"

.org $0000
    nop
    ldl d %0000
label1:
    ldl d %0001
    add d
    dup d
    ldl d $000A
    gt
    bif label1
    enter routine2
    halt
end:

; Remark inside a string
.str "; should not be lost"

.align l
routine2:
    nop
    nop
    neg d
    leave

all_opcodes:
    rd b
    rd w
    rd l
    ird b
    ird w
    ird l
    sto b
    sto w
    sto l
    isto b
    isto w
    isto l
    ldl d %0001
    ldl r %0001
    ldl t %0001
    ldl c %0001
    ldl c test1
    or
    xor
    and
    halt
    eq
    gt
    gtu
    gteu
    gte
    reset
    mul
    mulu
    swap d
    swap t
    swap c
    swap r
    drop d
    drop t
    drop c
    drop r
    not
    neg
    leave

; --------------- end of file ----------------------
