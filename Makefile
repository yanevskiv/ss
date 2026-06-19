CC = gcc
SRC_DIR = src
INC_DIR = h
OBJ_DIR = dist

ASM = asembler
LD  = linker
EMU = emulator

CFLAGS = -fdiagnostics-color=always -I$(INC_DIR) -O2

ASM_CFLAGS =
LD_CFLAGS  =
EMU_CFLAGS = -pthread

ASM_OBJ_LIST = asembler.o util.o elf.o
LD_OBJ_LIST  = linker.o util.o elf.o
EMU_OBJ_LIST = emulator.o util.o elf.o

ASM_OBJS = $(addprefix $(OBJ_DIR)/,$(ASM_OBJ_LIST))
LD_OBJS  = $(addprefix $(OBJ_DIR)/,$(LD_OBJ_LIST))
EMU_OBJS = $(addprefix $(OBJ_DIR)/,$(EMU_OBJ_LIST))


all: $(ASM) $(LD) $(EMU)

$(ASM): $(ASM_OBJS) | Makefile
	$(CC) $(CFLAGS) $(ASM_CFLAGS) -o $(ASM) $^

$(LD): $(LD_OBJS) | Makefile
	$(CC) $(CFLAGS) $(LD_CFLAGS) -o $(LD) $^

$(EMU): $(EMU_OBJS) | Makefile
	$(CC) $(CFLAGS) $(EMU_CFLAGS) -o $(EMU) $^

$(OBJ_DIR)/%.o: $(SRC_DIR)/%.c | Makefile $(OBJ_DIR)
	$(CC) -c $(CFLAGS) -o $@ $<

$(OBJ_DIR):
	mkdir -p $(OBJ_DIR)

clean:
	rm -f $(ASM)
	rm -f $(LD)
	rm -f $(EMU)
	rm -rf $(OBJ_DIR)

.PHONY: clean
