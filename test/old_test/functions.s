#
#    .equ term_out, 0xFFFFFF00
#    .global printStr
#printStr:
#    ld $0, %r0
#    ld $1, %r1
#    pop %r2
#    ld $term_out, %r4
#    ld $0xff, %r5
#printStr_Loop:
#    ld [ %r2 ], %r3
#    and %r5, %r3
#    add %r1, %r2
#    beq %r3, %r0, printStr_Done
#    st %r3, [ %r4 ]
#    jmp printStr_Loop
#printStr_Done:
#    ret
