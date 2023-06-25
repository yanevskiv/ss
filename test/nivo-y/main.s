# file: main.s
.extern handler
.global my_counter
.equ tim_cfg, 0xFFFFFF10
.equ init_sp, 0xFFFFFF00
.section my_code

    ld $init_sp, %sp
    ld $handler, %r1
    csrwr %r1, %handler

    int
    ld $0x1, %r1
    st %r1, tim_cfg
    ld $0x0, %r0
    ld $0x1, %r1
    ld $10, %r2
wait:
    add %r1, %r0
    jmp wait
    add %r1, %r3
    ld my_counter, %r3
    int
    add %r1, %r3
    st %r3, my_counter
    bne %r3, %r2, wait
    halt
.section my_data
my_counter:
    .word 0
printStr:
    .ascii "Hello There!"
.end
