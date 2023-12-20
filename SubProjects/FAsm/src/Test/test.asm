; Test
;
;
;

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

.str "Dit is een test"

; --------------- end of file ----------------------
