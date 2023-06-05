#!/bin/bash

ASSEMBLER=../../assembler
LINKER=../../linker
EMULATOR=../../emulator

${ASSEMBLER} -o main.o main.s
${ASSEMBLER} -o handler.o handler.s
${LINKER} -hex \
  -place=my_code@0x40000000 \
  -place=my_handler@0xF0000000 \
  -o program.hex \
  main.o handler.o
  #handler.o main.o
${EMULATOR} program.hex
