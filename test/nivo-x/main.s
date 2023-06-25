# file: main.s
.extern handler
.global my_counter
.equ tim_cfg, 0xFFFFFF10
.equ init_sp, 0xFFFFFF00
.section my_code
    ld $hello_world, %r8
    push %r8
    call printStr
    ld $0, %r0
    ld $1, %r1
    ld $0, %r2
    ld $10, %r3
repeat:
    call printStr
    add %r1, %r2
    beq %r2, %r3, repeat
    halt

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
hello_world:
    .ascii "Hello \e[32mworld\e[0m\n"
.section my_code

printStr:
    ld $0, %r0
    ld $1, %r1
    ld $0xff, %r2
    ld [%sp + 0x4], %r3
printStrLoop:
    ld [%r3], %r4
    and %r2, %r4
    beq %r4, %r0, printStrOver
    st %r4, 0xFFFFFF00
    add %r1, %r3
    jmp printStrLoop
printStrOver:
    ret
.end
