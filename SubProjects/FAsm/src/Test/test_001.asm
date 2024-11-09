; Test
;
    ; Space should be skipped
    ; Also for directives

    .str "123"
    .str "test"

; Label inside a string
.str "label1:"

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

; Remark inside a string
.str "; should not be lost"


    nop
    nop

; --------------- end of file ----------------------
