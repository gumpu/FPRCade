; vi: ft=smasm
; Test
;
    ; Space should be skipped
    ; Also for directives

    .str "123"
    .align l
    .str "test"

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

; --------------- end of file ----------------------
