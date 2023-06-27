#!/bin/bash

ASSEMBLER=../../assembler
LINKER=../../linker
EMULATOR=../../emulator

${ASSEMBLER} -o main.o main.s
${ASSEMBLER} -o handler.o handler.s
${LINKER} -hex \
  -place=my_code@0x40000000 \
  -place=math@0xF0000000 \
  -o program.hex \
  handler.o main.o
${EMULATOR} -timer -terminal program.hex
