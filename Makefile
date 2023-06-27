CC = gcc
SRC_DIR = src
INC_DIR = inc
OBJ_DIR = dist

ASSEMBLER = assembler
LINKER    = linker
EMULATOR  = emulator

CFLAGS = -fdiagnostics-color=always -I$(INC_DIR)

ASM_CFLAGS =
LNK_CFLAGS =
EMU_CFLAGS = -pthread

ASM_OBJ_LIST = assembler.o util.o elf.o
LNK_OBJ_LIST = linker.o util.o elf.o
EMU_OBJ_LIST = emulator.o util.o elf.o

ASM_OBJS = $(addprefix $(OBJ_DIR)/,$(ASM_OBJ_LIST))
LNK_OBJS = $(addprefix $(OBJ_DIR)/,$(LNK_OBJ_LIST))
EMU_OBJS = $(addprefix $(OBJ_DIR)/,$(EMU_OBJ_LIST))


all: $(ASSEMBLER) $(LINKER) $(EMULATOR)

$(ASSEMBLER): $(ASM_OBJS) | Makefile
	$(CC) $(CFLAGS) $(ASM_CFLAGS) -o $(ASSEMBLER) $^

$(LINKER): $(LNK_OBJS) | Makefile
	$(CC) $(CFLAGS) $(LNK_CFLAGS) -o $(LINKER) $^

$(EMULATOR): $(EMU_OBJS) | Makefile
	$(CC) $(CFLAGS) $(EMU_CFLAGS) -o $(EMULATOR) $^

$(OBJ_DIR)/%.o: $(SRC_DIR)/%.c | Makefile $(OBJ_DIR)
	$(CC) -c $(CFLAGS) -o $@ $< 

$(OBJ_DIR):
	mkdir -p $(OBJ_DIR)

clean:
	rm -f $(ASSEMBLER)
	rm -f $(LINKER)
	rm -f $(EMULATOR)
	rm -rf $(OBJ_DIR)

.PHONY: clean
