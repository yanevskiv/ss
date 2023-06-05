CC = gcc
LD = 
SRC_DIR = src/
DIST_DIR = dist/
INC_DIR = inc/

asembler: src/asembler.c
	$(CC) -I${INC_DIR} $(<) -o $(DIST_DIR)/$(@)



