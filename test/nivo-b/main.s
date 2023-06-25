# file: main.s

.global handler, my_start, my_counter

.section my_code
my_start:
    ld $0xcafebabe, %r0
    ld $0xff00ff00, %r1
    st %r0, [%r1]
    ld $'\n', %r1
    probe %r1
    halt
loop:
    jmp loop
    ld $0xFFFFFEFE, %sp
    ld $handler, %r1
    csrwr %r1, %handler
    
    ld $0x1, %r1
    st %r1, 0xFFFFFF10 # tim_cfg
wait:
    ld my_counter, %r1
    ld $20, %r2
    bne %r1, %r2, wait
    halt

func:
    ld $isr_terminal, %r1
    ret

.section my_data
my_counter:
.word 0
msg:
    .ascii "fd Hello safdsa"

.end
