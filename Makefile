CC = gcc
LD = 
SRC_DIR = src/

asembler : src/asembler.c
	$(CC) $(<) -o $(@)
