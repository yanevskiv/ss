cd "test"
ASSEMBLER=../asembler
LINKER=../linker
EMULATOR=../emulator

${ASSEMBLER} -hex -o main.o main.s
${ASSEMBLER} -hex -o math.o math.s
${ASSEMBLER} -hex -o ivt.o ivt.s
${ASSEMBLER} -hex -o isr_reset.o isr_reset.s
${ASSEMBLER} -hex -o isr_terminal.o isr_terminal.s
${ASSEMBLER} -hex -o isr_timer.o isr_timer.s
${ASSEMBLER} -hex -o isr_user0.o isr_user0.s
${LINKER} -hex -place=ivt@0x000 -o program.hex ivt.o math.o main.o isr_reset.o isr_terminal.o isr_timer.o isr_user0.o
${EMULATOR} program.hex
