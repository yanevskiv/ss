#file: handler.s
.equ term_out, 0xffffff00
.equ term_in, 0xffffff04
.equ ascii_code, 84
.extern my_counter
.global handler
.section my_handler
handler:
    push %r1
    push %r2
    csrrd %cause, %r1
    ld $2, %r2
    beq %r1, %r2, my_isr_timer
    ld $3, %r2
    beq %r1, %r2, my_isr_terminal
    ld $4, %r2
    beq %r1, %r2, my_isr_software
my_isr_software:
    ld term_in, %r7
    st %r7, term_out
    jmp finish
my_isr_timer:
    ld $ascii_code, %r1
    st %r1, term_out
    jmp finish
my_isr_terminal:
    ld term_in, %r1
    ld $1, %r2
    add %r2, %r1
    st %r1, term_out
    ld my_counter, %r1
    add %r2, %r1
    st %r1, my_counter
    jmp finish
finish:
    pop %r2
    pop %r1
    iret
.end
