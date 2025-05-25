.code
    nop
    nop
    nop
    nop
    enter routine1
    halt

routine1:
    ldl d $0000
label1:
    ldl d $0001
    add d
    dup d
    ldl d $0003
    gt
    bif label1
    leave
end:

; --------------- end of file ----------------------
