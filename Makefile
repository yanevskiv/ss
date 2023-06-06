CC = gcc
LD = 
SRC_DIR = src/
DIST_DIR = dist/
INC_DIR = inc/
all: 
	gcc -c -fdiagnostics-color=always -Iinc src/elf.c -o src/elf.o
	gcc -fdiagnostics-color=always -Iinc src/asembler.c src/elf.o -o asembler
	gcc -fdiagnostics-color=always -Iinc src/linker.c src/elf.o -o linker
	gcc -fdiagnostics-color=always -Iinc src/emulator.c src/elf.o -o emulator 
	timeout 0.4 ./test/start.sh




