#include <stdlib.h>
#include <emulator.h>
#include <assert.h>
#include <elf.h>
#include <string.h>
#include <util.h>

static void show_help(FILE* file) 
{
    const char *help_text = 
        "Usage: emulator [HEX FILE]\n"
    ;
    fprintf(file, "%s", help_text);

}

#define PACK(x, y) ((((x) & 0xf) << 4) | (((y) & 0xf) << 0))

#define VA_BIT_PAGE 16
#define VA_BIT_WORD 16
#define VA_BIT_LENGTH (VA_BIT_PAGE + VA_BIT_WROD)
#define VA_PAGE_SIZE (1 << VA_BIT_WORD)
#define VA_PAGE_COUNT (1 << VA_BIT_PAGE)

#define VA_PAGE(addr) (((addr) >> VA_BIT_PAGE) & ((1 << VA_BIT_PAGE) - 1))
#define VA_WORD(addr) ((addr) & ((1 << VA_BIT_WORD) - 1))
#define ASM_INSTR_SIZE 0x4

//#define PC 15
//#define SP 14
//#define STATUS 0
//#define HANDLE 1
//#define CAUSE  2

#define Asm_Byte  unsigned char
#define Asm_Short unsigned short
#define Asm_Word  unsigned int
#define Asm_Reg   int
#define Asm_Addr  Asm_Word

#define ASM_INSTR_WIDTH 4

#define ASM_NO_SAVE_HELPER_REG 0
#define ASM_SAVE_HELPER_REG    1


enum {
    REG0 = 0,
    REG1,
    REG2,
    REG3,
    REG4,
    REG5,
    REG6,
    REG7,
    REG8,
    REG9,
    REG10,
    REG11,
    REG12,
    REG13,
    REG14,
    REG15,

    ADR = 12,
    IDX = 13,
    SP  = 14,
    PC  = 15,

    STATUS = 0,
    HANDLE,
    CAUSE,
};

Asm_Reg GPR[16] = { 0 };
Asm_Reg CSR[3]  = { 0 };
unsigned char *PAGES[VA_PAGE_SIZE] = { 0 };

// Set byte in the memory
void Mem_WriteByte(Asm_Addr addr, Asm_Byte value)
{
    int page = VA_PAGE(addr);
    int word = VA_WORD(addr);
    if (PAGES[page] == NULL) {
        PAGES[page] = malloc(VA_PAGE_SIZE);
        assert(PAGES[page] != NULL);
    }
    PAGES[page][word] = value & 0xff;
}

// Get byte from the memory
unsigned char Mem_ReadByte(Asm_Addr addr)
{
    int page = VA_PAGE(addr);
    int word = VA_WORD(addr);
    if (PAGES[page] == NULL) {
        return 0;
    }
    return PAGES[page][word];
}

void Mem_WriteWord(Asm_Addr addr, Asm_Word value)
{
    Mem_WriteByte(addr + 0, (value >> 0)  & 0xff);
    Mem_WriteByte(addr + 1, (value >> 8)  & 0xff);
    Mem_WriteByte(addr + 2, (value >> 16) & 0xff);
    Mem_WriteByte(addr + 3, (value >> 24) & 0xff);
}

unsigned int Mem_ReadWord(int addr)
{
    return 
          Mem_ReadByte(addr + 0) << 0
        | Mem_ReadByte(addr + 1) << 8
        | Mem_ReadByte(addr + 2) << 16
        | Mem_ReadByte(addr + 3) << 24
    ;
}


// Read bytes
void Mem_ReadBytes(void *data, int startAddr, int size)
{
    unsigned char *bytes = (unsigned char*)data;
    int i;
    for (i = 0; i < size; i++) {
        bytes[i] = Mem_ReadByte(startAddr + i);
    }
}

// Write bytes
void Mem_WriteBytes(void *data, int startAddr, int size)
{
    unsigned char *bytes = (unsigned char*)data;
    int i;
    for (i = 0; i < size; i++) {
        Mem_WriteByte(startAddr + i, bytes[i]);
    }
}

// Clear memory
void Mem_Clear(void)
{
    int page;
    for (page = 0; page < VA_PAGE_COUNT; page++) {
        if (PAGES[page])  {
            free(PAGES[page]);
            PAGES[page] = NULL;
        }
    }
}

// Load sections into memory
void Mem_LoadElf(Elf_Builder *elf)
{
    /* 
     * Load into memory
     */
    Asm_Addr offset = 0;
    for (int i = 1; i <= Elf_GetSectionCount(elf); i++) {
        /*
         * Load section into memory
         */
        Elf_PushSection(elf);
        Elf_SetSection(elf, i);
        Elf_Shdr *shdr = Elf_GetSection(elf);
        if (shdr->sh_type != SHT_SYMTAB && shdr->sh_type != SHT_RELA) {
            Elf_Addr addr = shdr->sh_addr;
            Elf_Xword size = shdr->sh_size;
            unsigned char *data = Elf_GetSectionData(elf);

            // If address is zero, push it at the beginning
            // Otherwise, place it at the desired location
            if (addr == 0) {
                Mem_WriteBytes(data, offset, size);
                offset += size;
            } else {
                Mem_WriteBytes(data, addr, size);
            }

            /*
             * Rewrite relas
             */
            char *relaName = Str_Concat(".rela", Elf_GetSectionName(elf));
            if (Elf_SectionExists(elf, relaName)) {
                Elf_PushSection(elf);
                Elf_UseSection(elf, relaName);
                int i, count = Elf_GetSectionSize(elf) / sizeof(Elf_Rela);
                for (i = 0; i < count; i++) {
                    Elf_Rela *rela  = (Elf_Rela*)Elf_GetSectionEntry(elf, i);
                    Elf_Half type   = ELF_R_TYPE(rela->r_info);
                    Elf_Word symndx = ELF_R_SYM(rela->r_info);
                    Elf_Sym *symb   = Elf_GetSymbol(elf, symndx);
                    Elf_Addr value  = symb->st_value;
                    value += rela->r_addend;
                    if (symb->st_shndx != SHN_ABS) {
                        Elf_PushSection(elf);
                        Elf_Shdr *sym_shdr = Elf_SetSection(elf, symb->st_shndx);
                        value += sym_shdr->sh_addr;
                        Elf_PopSection(elf);
                    }
                    Elf_Addr start = shdr->sh_addr + rela->r_offset;
                    switch (type) {
                        case R_SS_D12: {
                            /*
                             * Sets instruction displacement
                             */
                            Asm_Byte byte = Mem_ReadByte(start + 2);
                            Mem_WriteByte(start + 2, PACK(byte >> 4, value >> 8));
                            Mem_WriteByte(start + 3, value & 0xff);
                        } break;

                        case R_SS_LD32: {
                            /*
                             * Replaces the word value generated by `Asm_PushLoadWord()`
                             */
                             Mem_WriteByte(start + 3 * ASM_INSTR_WIDTH + 3, (value >> 24));
                             Mem_WriteByte(start + 5 * ASM_INSTR_WIDTH + 3, (value >> 16));
                             Mem_WriteByte(start + 7 * ASM_INSTR_WIDTH + 3, (value >> 8));
                             Mem_WriteByte(start + 9 * ASM_INSTR_WIDTH + 3, (value >> 0));
                        } break;
                        default: {
                            Show_Error("Unsupported rela type");
                        } break;
                    }
                }
                Elf_PopSection(elf);
            }
            free(relaName);
        }
        Elf_PopSection(elf);
    }
}

// Opcodes
enum {
    OC_HALT   = 0x0,
    OC_INT    = 0x1,
    OC_CALL   = 0x2,
    OC_BRANCH = 0x3,
    OC_XCHG   = 0x4,
    OC_ARITH  = 0x5,
    OC_LOGIC  = 0x6,
    OC_SHIFT  = 0x7,
    OC_STORE  = 0x8,
    OC_LOAD   = 0x9
};

enum {
    MOD_NONE = 0x0,

    MOD_CALL_REG = 0x0,
    MOD_CALL_MEM = 0x1,

    MOD_BRANCH_0  =  0x0,
    MOD_BRANCH_1  =  0x1,
    MOD_BRANCH_2  =  0x2,
    MOD_BRANCH_3  =  0x3,
    MOD_BRANCH_4  =  0x8,
    MOD_BRANCH_5  =  0x9,
    MOD_BRANCH_6  =  0xa,
    MOD_BRANCH_7  =  0xb,

    MOD_ARITH_ADD = 0x0,
    MOD_ARITH_SUB = 0x1,
    MOD_ARITH_MUL = 0x2,
    MOD_ARITH_DIV = 0x3,

    MOD_LOGIC_NOT = 0x0,
    MOD_LOGIC_AND = 0x1,
    MOD_LOGIC_OR  = 0x2,
    MOD_LOGIC_XOR = 0x3,

    MOD_SHIFT_LEFT  = 0x0,
    MOD_SHIFT_RIGHT = 0x1,

    MOD_STORE_0 = 0x0,
    MOD_STORE_1 = 0x2,
    MOD_STORE_2 = 0x1,

    MOD_LOAD_0 = 0x0,
    MOD_LOAD_1 = 0x1,
    MOD_LOAD_2 = 0x2,
    MOD_LOAD_3 = 0x3,
    MOD_LOAD_4 = 0x4,
    MOD_LOAD_5 = 0x5,
    MOD_LOAD_6 = 0x6,
    MOD_LOAD_7 = 0x7,

};

void Emu_RunElf(Elf_Builder *elf, FILE *output)
{
    /* 
     * Reset registers
     */
    GPR[PC] = 0x40000000;
    GPR[SP] = 0x80000000;

    /*
     * Read elf
     */
    Mem_LoadElf(elf);
    
    /*
     * Execute emulator
     */
    enum {
        M_RUNNING,
        M_STOP
    };
    int mode = M_RUNNING;

    while (mode == M_RUNNING)  {
        /*
         * Fetch instruction
         */
        unsigned char bytes[4];
        Mem_ReadBytes(bytes, GPR[PC], ASM_INSTR_SIZE);
        GPR[PC] += ASM_INSTR_SIZE;
        int oc   = (bytes[0] >> 4) & 0xf;
        int mod  = (bytes[0] >> 0) & 0xf;
        int ra   = (bytes[1] >> 4) & 0xf;
        int rb   = (bytes[1] >> 0) & 0xf;
        int rc   = (bytes[2] >> 4) & 0xf; 
        int disp = (((bytes[2] >> 0) & 0xf) << 8) | bytes[3];
        if (disp >= (1 << 11))  {
            disp -= (1 << 12);
        }

        /*
         * Execute instruction
         */
        switch (oc) {
            case OC_HALT: {
                mode = M_STOP;
            } break;

            case OC_INT: {
                // push status; 
                // push pc; 
                // cause <= 4; 
                // status <= status & (~0x1); 
                // pc <= handle;
                Mem_WriteWord(GPR[SP], CSR[STATUS]);
                GPR[SP] -= 4;
                Mem_WriteWord(GPR[SP], GPR[PC]);
                GPR[SP] -= 4;
                CSR[CAUSE] = 4;
                CSR[STATUS] = CSR[STATUS] & (~0x1);
                GPR[PC] <= CSR[HANDLE];
            } break;

            case OC_CALL: {
                switch (mod) {
                    case MOD_CALL_REG: {
                        Mem_WriteWord(GPR[SP], CSR[STATUS]);
                        GPR[SP] -= 4;
                        GPR[PC] = GPR[ra] + GPR[rb] + disp;
                    } break;

                    case MOD_CALL_MEM: {
                        Mem_WriteWord(GPR[SP], CSR[STATUS]);
                        GPR[SP] -= 4;
                        GPR[PC] = Mem_ReadWord(GPR[ra] + GPR[rb] + disp);
                    } break;
                }
            } break;

            case OC_BRANCH: {
                switch (mod) {
                    case MOD_BRANCH_0: {
                        GPR[PC] = GPR[ra] + disp;
                    } break;

                    case MOD_BRANCH_1: {
                        if (GPR[ra] == GPR[rb])
                            GPR[PC] = GPR[ra] + disp;
                    } break;

                    case MOD_BRANCH_2: {
                        if (GPR[ra] != GPR[rb])
                            GPR[PC] = GPR[ra] + disp;
                    } break;

                    case MOD_BRANCH_3: {
                        if ((int) GPR[ra] > (int)GPR[rb])
                            GPR[PC] = GPR[ra] + disp;
                    } break;

                    case MOD_BRANCH_4: {
                        GPR[PC] = Mem_ReadWord(GPR[ra] + disp);
                    } break;

                    case MOD_BRANCH_5: {
                        if (GPR[rb] == GPR[rc])
                            GPR[PC] = Mem_ReadWord(GPR[ra] + disp);
                    } break;

                    case MOD_BRANCH_6: {
                        if (GPR[rb] != GPR[rc])
                            GPR[PC] = Mem_ReadWord(GPR[ra] + disp);
                    } break;

                    case MOD_BRANCH_7: {
                        if ((int)GPR[rb] > (int)GPR[rc])
                            GPR[PC] = Mem_ReadWord(GPR[ra] + disp);
                    } break;
                }
            } break;

            case OC_XCHG: {
                Asm_Word temp = GPR[rb];
                GPR[rb] = GPR[rc];
                GPR[rc] = temp;
            } break;

            case OC_ARITH: {
                switch (mod) {
                    case MOD_ARITH_ADD: { 
                        GPR[ra] = GPR[rb] + GPR[rc]; 
                    } break;

                    case MOD_ARITH_SUB: { 
                        GPR[ra] = GPR[rb] - GPR[rc]; 
                    } break;

                    case MOD_ARITH_MUL: { 
                        GPR[ra] = GPR[rb] * GPR[rc]; 
                    } break;

                    case MOD_ARITH_DIV: { 
                        GPR[ra] = GPR[rb] / GPR[rc]; 
                    } break;
                }
            } break;

            case OC_LOGIC: {
                switch (mod) {
                    case MOD_LOGIC_NOT: { 
                        GPR[ra] = ~GPR[rb];
                    }  break;

                    case MOD_LOGIC_AND: { 
                        GPR[ra] = GPR[rb] & GPR[rc];
                    } break;

                    case MOD_LOGIC_OR:  { 
                        GPR[ra] = GPR[rb] | GPR[rc];
                    } break;

                    case MOD_LOGIC_XOR: { 
                        GPR[ra] = GPR[rb] ^ GPR[rc];
                    } break;
                }
            } break;

            case OC_SHIFT: {
                switch (mod) {
                    case MOD_SHIFT_LEFT:  {
                        GPR[ra] = GPR[rb] << GPR[rc];
                    } break;

                    case MOD_SHIFT_RIGHT: {
                        GPR[ra] = GPR[rb] >> GPR[rc];
                    } break;
                }
            } break;

            case OC_STORE: {
                switch (mod) {
                    case MOD_STORE_0: {
                        Mem_WriteWord(GPR[ra] + GPR[rb] + disp, GPR[rc]);
                    } break;

                    case MOD_STORE_1: {
                        Mem_WriteWord(Mem_ReadWord(GPR[ra] + GPR[rb] + disp), GPR[rc]);
                    } break;

                    case MOD_STORE_2: {
                        GPR[ra] = GPR[ra] + disp;
                        Mem_WriteWord(GPR[ra], GPR[rc]);
                    } break;
                }
            } break;

            case OC_LOAD: {
                switch (mod) {
                    case MOD_LOAD_0: {
                        GPR[ra] = CSR[rb];
                    } break;

                    case MOD_LOAD_1: {
                        GPR[ra] = GPR[rb] + disp;
                    } break;

                    case MOD_LOAD_2: {
                        GPR[ra] = Mem_ReadWord(GPR[rb] + GPR[rc] + disp);
                    } break;

                    case MOD_LOAD_3: {
                        GPR[ra] = Mem_ReadWord(GPR[rb]);
                        GPR[rb] = GPR[rb] + disp;
                    } break;


                    case MOD_LOAD_4: {
                        CSR[ra] = GPR[rb];
                    } break;

                    case MOD_LOAD_5: {
                        CSR[ra] = CSR[rb] | disp;
                    } break;

                    case MOD_LOAD_6: {
                        CSR[ra] = Mem_ReadWord(GPR[rb] + GPR[rc] + disp);
                    } break;

                    case MOD_LOAD_7: {
                        CSR[ra] = Mem_ReadWord(GPR[rb]);
                        GPR[rb] = GPR[rb] + disp;
                    } break;

                }
            } break;
        }
    }

    /*
     * Print result
     */
    fprintf(output, "-------------------------------------------------------\n");
    printf("Emulated processor executed halt instruction\n");
    printf("Emulated processor state:\n");
    fprintf(output, " r0=0x%08x  r1=0x%08x  r2=0x%08x  r3=0x%08x\n",  GPR[0],  GPR[1],  GPR[2],  GPR[3]);
    fprintf(output, " r4=0x%08x  r5=0x%08x  r6=0x%08x  r7=0x%08x\n",  GPR[4],  GPR[5],  GPR[6],  GPR[7]);
    fprintf(output, " r8=0x%08x  r9=0x%08x r10=0x%08x r11=0x%08x\n",  GPR[8],  GPR[9], GPR[10], GPR[11]);
    fprintf(output, "r12=0x%08x r13=0x%08x r14=0x%08x r15=0x%08x\n", GPR[12], GPR[13], GPR[14], GPR[15]);
    fprintf(output, "\nstatus = %08x\nhandle = %08x\ncause  = %08x", CSR[STATUS], CSR[HANDLE], CSR[CAUSE]);
}

// Add a label at current section location
Elf_Sym *Asm_AddLabel(Elf_Builder *elf, const char *label)
{
    Elf_Sym *sym = Elf_UseSymbol(elf, label);
    sym->st_shndx = Elf_GetCurrentSection(elf);
    sym->st_value = Elf_GetSectionSize(elf);
    return sym;
}

// Add absolute symbol
Elf_Sym *Asm_AddAbsSymbol(Elf_Builder *elf, const char *name, Elf_Addr value)
{
    Elf_Sym *sym = Elf_UseSymbol(elf, name);
    sym->st_shndx = SHN_ABS;
    sym->st_value = value;
    return sym;
}

// Add relocation with respect to some symbol
Elf_Rela *Asm_AddRela(Elf_Builder *elf, const char *symbol, Elf_Half type, Elf_Sxword addend)
{
    Elf_Rela *rela = Elf_AddRelaSymb(elf, symbol);
    rela->r_offset = Elf_GetSectionSize(elf);
    rela->r_info   = ELF_R_INFO(ELF_R_SYM(rela->r_info), type);
    rela->r_addend = addend;
    return rela;
}

// Load zero value into register (gpr = gpr ^ gpr)
void Asm_PushXorZero(Elf_Builder *elf, int gpr)
{
    Elf_PushByte(elf, PACK(OC_LOGIC, MOD_LOGIC_XOR));
    Elf_PushByte(elf, PACK(gpr, gpr));
    Elf_PushByte(elf, PACK(gpr, 0x0));
    Elf_PushByte(elf, 0x00);
}

// Load a byte into a register
void Asm_PushLoadByte(Elf_Builder *elf, int rDest, unsigned value)
{
    // rDest  = 0;
    Elf_PushByte(elf, PACK(OC_LOGIC, MOD_LOGIC_XOR));
    Elf_PushByte(elf, PACK(rDest, rDest));
    Elf_PushByte(elf, PACK(rDest, 0x0));
    Elf_PushByte(elf, 0x00);

    // rDest += value
    Elf_PushByte(elf, PACK(OC_LOAD, MOD_LOAD_1));
    Elf_PushByte(elf, PACK(rDest, rDest));
    Elf_PushByte(elf, PACK(0x0, 0x0));
    Elf_PushByte(elf, value &0xff);
}

// Load a half into register
// This operation requires assistance from another 'helper' register (recommended: IDX (13) or ADR (12))
// The helper register can be optionally saved on the stack
void Asm_PushLoadHalf(Elf_Builder *elf, int rDest, unsigned value, int rHelp, int saveHelp)
{
    int d = rDest;
    int s = rHelp;
    unsigned char code[] = {
        0x81, PACK(0xe, 0xe), PACK(s, 0xf), 0xfc,                   // push rHelp
        0x63, PACK(s, s),     PACK(s, 0),   0x00,                   // rHelp = 0x0
        0x91, PACK(s, s),     0x00,         0x08,                   // rHelp = 0x8
        0x63, PACK(d, d),     PACK(d, 0),   0x00,                   // rDest = 0
        0x91, PACK(d, d),     0x00,         ((value >> 8) & 0xff),  // rDest += (value >> 8) & 0xff
        0x70, PACK(d, d),     PACK(s, 0),   0x00,                   // rDest <<= rHelp
        0x91, PACK(d, d),     0x00,         (value & 0xff),         // rDest += value & 0xff
        0x63, PACK(s, s),     PACK(s, 0),   0x00,                   // rHelp = 0
        0x93, PACK(s, 0xe),   0xe0,         0x04,                   // pop rHelp
    };
    if (saveHelp) {
        Elf_PushBytes(elf, code, sizeof(code));
    } else {
        Elf_PushBytes(elf, code + 4, sizeof(code) - 8);
    }
}

// Load a word into register
// This operation requires assistance from another 'helper' register (recommended: IDX (13) or ADR (12))
// The helper register can be optionally saved on the stack
void Asm_PushLoadWord(Elf_Builder *elf, int rDest, unsigned value, int rHelp, int saveHelp)
{

    int d = rDest;
    int s = rHelp;
    unsigned char code[] = {
        0x81, PACK(0xe, 0xe), PACK(s, 0xf), 0xfc,                   // push rHelp
        0x63, PACK(s, s),     PACK(s, 0),   0x00,                   // rHelp = 0x0
        0x91, PACK(s, s),     0x00,         0x08,                   // rHelp = 0x8
        0x63, PACK(d, d),     PACK(d, 0),   0x00,                   // rDest = 0
        0x91, PACK(d, d),     0x00,         ((value >> 24) & 0xff), // rDest += (value >> 24) & 0xff
        0x70, PACK(d, d),     PACK(s, 0),   0x00,                   // rDest <<= r1
        0x91, PACK(d, d),     0x00,         ((value >> 16) & 0xff), // rDest += (value >> 16) & xff
        0x70, PACK(d, d),     PACK(s, 0),   0x00,                   // rDest <<= r1
        0x91, PACK(d, d),     0x00,         ((value >> 8) & 0xff),  // rDest += (value >> 8) & 0xff
        0x70, PACK(d, d),     PACK(s, 0),   0x00,                   // rDest <<= r1
        0x91, PACK(d, d),     0x00,         (value & 0xff),         // rDest += value & 0xff
        0x63, PACK(s, s),     PACK(s, 0),   0x00,                   // rHelp = 0
        0x93, PACK(s, 0xe),   0xe0,         0x04,                   // pop rHelp
    };
    if (saveHelp) {
        Elf_PushBytes(elf, code, sizeof(code));
    } else {
        Elf_PushBytes(elf, code + 4, sizeof(code) - 8);
    }
}



// Push any kind of instruction
void Asm_PushInstr(Elf_Builder *elf, int oc, int mod, int ra, int rb, int rc, int disp)
{
    Elf_PushByte(elf, ((oc & 0xf) << 4) | ((mod & 0xf) << 0));
    Elf_PushByte(elf, ((ra & 0xf) << 4) | ((rb  & 0xf) << 0));
    Elf_PushByte(elf, ((rc & 0xf) << 4) | (((disp >> 8) & 0xf) << 0));
    Elf_PushByte(elf, disp & 0xff);

}

// Push `halt` instruction
void Asm_PushHalt(Elf_Builder *elf)
{
    Elf_PushByte(elf, PACK(OC_HALT, 0x0));
    Elf_PushByte(elf, 0x00);
    Elf_PushByte(elf, 0x00);
    Elf_PushByte(elf, 0x00);
}

// Push `int` instruction
void Asm_PushInt(Elf_Builder *elf)
{
    Elf_PushByte(elf, PACK(OC_INT, 0x0));
    Elf_PushByte(elf, 0x00);
    Elf_PushByte(elf, 0x00);
    Elf_PushByte(elf, 0x00);
}

// Push `iret` instruction
void Asm_PushIret(Elf_Builder *elf)
{
    // pop status
    Elf_PushByte(elf, PACK(OC_LOAD, MOD_LOAD_7));
    Elf_PushByte(elf, PACK(STATUS, SP));
    Elf_PushByte(elf, 0x00);
    Elf_PushByte(elf, 0x04);

    // pop pc
    Elf_PushByte(elf, PACK(OC_LOAD, MOD_LOAD_3));
    Elf_PushByte(elf, PACK(PC, SP));
    Elf_PushByte(elf, 0x00);
    Elf_PushByte(elf, 0x04);
}

// Push `ret` instruction
void Asm_PushRet(Elf_Builder *elf)
{
    // pop pc
    Elf_PushByte(elf, PACK(OC_LOAD, MOD_LOAD_3));
    Elf_PushByte(elf, PACK(PC, SP));
    Elf_PushByte(elf, 0x00);
    Elf_PushByte(elf, 0x04);
}

// Push `push %gpr` instruction
// The stack is assumed to grow downwards (SP -= 4)
void Asm_PushStackPush(Elf_Builder *elf, int gpr)
{
    // push %gpr
    Elf_PushByte(elf, PACK(OC_STORE, MOD_STORE_2));
    Elf_PushByte(elf, PACK(SP, SP));
    Elf_PushByte(elf, PACK(gpr, 0xf));
    Elf_PushByte(elf, 0xfc);
}

// Push `pop %gpr` instruction (SP += 4)
void Asm_PushStackPop(Elf_Builder *elf, int gpr)
{
    Elf_PushByte(elf, PACK(OC_LOAD, MOD_LOAD_3));
    Elf_PushByte(elf, PACK(gpr, SP));
    Elf_PushByte(elf, 0x00);
    Elf_PushByte(elf, 0x04);
}

// Push binary `xchg` operation (swap two register values)
void Asm_PushXchg(Elf_Builder *elf, int gpr1, int gpr2)
{
    // 
    Elf_PushByte(elf, PACK(OC_XCHG, 0x0));
    Elf_PushByte(elf, PACK(0x0, gpr1));
    Elf_PushByte(elf, PACK(gpr2, 0x0));
    Elf_PushByte(elf, 0x00);
}

// Perform a binary ALU operation (+ - * / & | ^ << >>)
void Asm_PushALU(Elf_Builder *elf, int oc, int mod, int gprS, int gprD)
{
    // ALU %gprS, %gprD
    Elf_PushByte(elf, PACK(oc, mod));
    Elf_PushByte(elf, PACK(gprD, gprD));
    Elf_PushByte(elf, PACK(gprS, 0x0));
    Elf_PushByte(elf, 0x00);
}

// Push binary `add` operation
void Asm_PushAdd(Elf_Builder *elf, int gprS, int gprD)
{
    Elf_PushByte(elf, PACK(OC_ARITH, MOD_ARITH_ADD));
    Elf_PushByte(elf, PACK(gprD, gprD));
    Elf_PushByte(elf, PACK(gprS, 0x0));
    Elf_PushByte(elf, PACK(0x0, 0x0));
}

// Push binary `sub` operation
void Asm_PushSub(Elf_Builder *elf, int gprS, int gprD)
{
    Elf_PushByte(elf, PACK(OC_ARITH, MOD_ARITH_SUB));
    Elf_PushByte(elf, PACK(gprD, gprD));
    Elf_PushByte(elf, PACK(gprS, 0x0));
    Elf_PushByte(elf, PACK(0x0, 0x0));
}

// Push binary `mul` operation
void Asm_PushMul(Elf_Builder *elf, int gprS, int gprD)
{
    Elf_PushByte(elf, PACK(OC_ARITH, MOD_ARITH_MUL));
    Elf_PushByte(elf, PACK(gprD, gprD));
    Elf_PushByte(elf, PACK(gprS, 0x0));
    Elf_PushByte(elf, PACK(0x0, 0x0));
}

// Push binary `div` operation
void Asm_PushDiv(Elf_Builder *elf, int gprS, int gprD)
{
    Elf_PushByte(elf, PACK(OC_ARITH, MOD_ARITH_DIV));
    Elf_PushByte(elf, PACK(gprD, gprD));
    Elf_PushByte(elf, PACK(gprS, 0x0));
    Elf_PushByte(elf, PACK(0x0, 0x0));
}

// Push unary `not` operation
void Asm_PushNot(Elf_Builder *elf, int gpr)
{
    Elf_PushByte(elf, PACK(OC_LOGIC, MOD_LOGIC_NOT));
    Elf_PushByte(elf, PACK(gpr, gpr));
    Elf_PushByte(elf, 0x00);
    Elf_PushByte(elf, 0x00);
}

// Push binary `and` operation
void Asm_PushAnd(Elf_Builder *elf, int gprS, int gprD)
{
    Elf_PushByte(elf, PACK(OC_LOGIC, MOD_LOGIC_AND));
    Elf_PushByte(elf, PACK(gprD, gprD));
    Elf_PushByte(elf, PACK(gprS, 0x0));
    Elf_PushByte(elf, PACK(0x0, 0x0));
}

// Push binary `or` operation
void Asm_PushOr(Elf_Builder *elf, int gprS, int gprD)
{
    Elf_PushByte(elf, PACK(OC_LOGIC, MOD_LOGIC_OR));
    Elf_PushByte(elf, PACK(gprD, gprD));
    Elf_PushByte(elf, PACK(gprS, 0x0));
    Elf_PushByte(elf, PACK(0x0, 0x0));
}

// Push binary `xor` operation
void Asm_PushXor(Elf_Builder *elf, int gprS, int gprD)
{
    Elf_PushByte(elf, PACK(OC_LOGIC, MOD_LOGIC_XOR));
    Elf_PushByte(elf, PACK(gprD, gprD));
    Elf_PushByte(elf, PACK(gprS, 0x0));
    Elf_PushByte(elf, PACK(0x0, 0x0));
}

// Push binary `shl` operation
void Asm_PushShl(Elf_Builder *elf, int gprS, int gprD)
{
    Elf_PushByte(elf, PACK(OC_SHIFT, MOD_SHIFT_LEFT));
    Elf_PushByte(elf, PACK(gprD, gprD));
    Elf_PushByte(elf, PACK(gprS, 0x0));
    Elf_PushByte(elf, PACK(0x0, 0x0));
}

// Push binary `shr` operation
void Asm_PushShr(Elf_Builder *elf, int gprS, int gprD)
{
    Elf_PushByte(elf, PACK(OC_SHIFT, MOD_SHIFT_RIGHT));
    Elf_PushByte(elf, PACK(gprD, gprD));
    Elf_PushByte(elf, PACK(gprS, 0x0));
    Elf_PushByte(elf, PACK(0x0, 0x0));
}


// Push `csrrd` instruction
// (Load a control register value into a general purpose register)
void Asm_PushCsrrd(Elf_Builder *elf, int csr, int gpr)
{
    Elf_PushByte(elf, PACK(OC_LOAD, MOD_LOAD_0));
    Elf_PushByte(elf, PACK(gpr, csr));
    Elf_PushByte(elf, 0x00);
    Elf_PushByte(elf, 0x00);
}

// Push `csrwr` instruction
// (Store a general purpse register value into a control register)
void Asm_PushCsrwr(Elf_Builder *elf, int gpr, int csr)
{
    Elf_PushByte(elf, PACK(OC_LOAD, MOD_LOAD_4));
    Elf_PushByte(elf, PACK(csr, gpr));
    Elf_PushByte(elf, 0x00);
    Elf_PushByte(elf, 0x00);
}


// Push loading into register from memory
void Asm_PushLoadMemWord(Elf_Builder *elf, int gpr, Elf_Addr addr)
{
    // Load address into ADR register
    Asm_PushLoadWord(elf, ADR, addr, IDX, 0);

    // Clear gpr regsiter
    Asm_PushXorZero(elf, gpr);

    // Set value from memory
    // gpr[a] <= mem32[gpr[b] + gpr[c] + disp];
    Elf_PushByte(elf, PACK(OC_LOAD, MOD_LOAD_3));
    Elf_PushByte(elf, PACK(gpr, gpr));
    Elf_PushByte(elf, PACK(ADR, 0x0));
    Elf_PushByte(elf, 0x00);
}

void Asm_PushLoadMemSymWord(Elf_Builder *elf, int gpr, const char *symName)
{
    // Add rela for the next set of instructions
    Asm_AddRela(elf, );

    // Load address into ADR regsiter (actual address will be loaded during relocation)

}



void Asm_PushStoreMemWord(Elf_Builder *elf, int gpr, Elf_Addr addr)
{

}


// Push `ld  %gprS, %gprD` instruction
void Asm_PushLoadReg(Elf_Builder *elf, int gprS, int gprD)
{
    // 
    Elf_PushByte(elf, PACK(OC_LOAD, MOD_LOAD_1));
    Elf_PushByte(elf, PACK(gprD, gprS));
    Elf_PushByte(elf, 0x00);
    Elf_PushByte(elf, 0x00);
}

// Push `ld [%gprS + literal], %gprD` instruction
void Asm_PushLoadMemRegDisp(Elf_Builder *elf, int gprS, int gprD, int disp)
{
    // gprD <= 0
    Asm_PushXorZero(elf, gprD);

    // gprD <= mem32[gprD + gprS + disp]
    Elf_PushByte(elf, PACK(OC_LOAD, MOD_LOAD_2));
    Elf_PushByte(elf, PACK(gprD, gprD));
    Elf_PushByte(elf, PACK(gprS, disp >> 8));
    Elf_PushByte(elf, disp & 0xff);
}

// Push `ld [%gprS + symbol], %gprD` instruction
void Asm_PushLoadMemRegSymDisp(Elf_Builder *elf, int gprS, int gprD, const char *symName)
{
    // gprD <= 0
    Asm_PushXorZero(elf, gprD);

    // Add displacement relocation for next instruction
    Asm_AddRela(elf, symName, R_SS_D12, 0);

    // gprD <= mem32[gprD + gprS + disp]
    Elf_PushByte(elf, PACK(OC_LOAD, MOD_LOAD_2));
    Elf_PushByte(elf, PACK(gprD, gprD));
    Elf_PushByte(elf, PACK(gprS, 0));
    Elf_PushByte(elf, 0);
}

// Push `st %gprS, %gprD` instruction
void Asm_PushStoreReg(Elf_Builder *elf, int gprS, int gprD)
{
    Elf_PushByte(elf, PACK(OC_LOAD, MOD_LOAD_1));
    Elf_PushByte(elf, PACK(gprD, gprS));
    Elf_PushByte(elf, 0x00);
    Elf_PushByte(elf, 0x00);
}

// Push `st [%gprS + literal], %gprD`
void Asm_PushStoreMemRegDisp(Elf_Builder *elf, int gprS, int gprD, int disp)
{
    // TODO
}

// Push `st [%gprS + symbol], %gprD`
void Asm_PushStoreMemRegSymDisp(Elf_Builder *elf, int gprS, int gprD, const char *symName)
{
    // TODO
}

typedef enum {
    BRANCH_TYPE_CALL,
    BRANCH_TYPE_JMP, 
    BRANCH_TYPE_BEQ,
    BRANCH_TYPE_BNE,
    BRANCH_TYPE_BGT
} Asm_BranchType;

// Push `jmp` instruction 
void Asm_PushJmpReg(Elf_Builder *elf, int gpr, int disp)
{
    Elf_PushByte(elf, PACK(OC_BRANCH, MOD_BRANCH_0));
    Elf_PushByte(elf, PACK(gpr, 0x0));
    Elf_PushByte(elf, PACK(0x0, disp >> 8));
    Elf_PushByte(elf, disp & 0xff);
}


void Test3(Elf_Builder *elf) 
{
    // my_data
    {
        Elf_PushSection(elf);
        Asm_AddAbsSymbol(elf, "fuck", 0x40);
        Elf_Shdr *shdr = Elf_UseSection(elf, "my_data");
        shdr->sh_addr = 0xcccc0000;
        Asm_AddLabel(elf, "value1");
        Elf_PushWord(elf, 0xf0f0f0f0);
        Asm_AddLabel(elf, "value2");
        Elf_PushWord(elf, 0xf0f0f0f0);
        Asm_AddLabel(elf, "value3");
        Elf_PushWord(elf, 0xf0f0f0f0);
        Elf_PopSection(elf);
    }

    // my_start
    {
        Elf_PushSection(elf);
        Elf_Shdr *shdr = Elf_UseSection(elf, "my_start");
        shdr->sh_addr = 0x40000000;

        Asm_PushLoadWord(elf, REG0, 0xffff0000, REG13, 0);
        Asm_PushLoadByte(elf, REG1, 0x5);
        Asm_PushLoadReg(elf, REG0, REG3);
        Asm_PushStoreReg(elf, REG4, REG3);

        Elf_PopSection(elf);
    }
}




int main(int argc, char *argv[]) {
    if (argc != 2)  {
        show_help(stdout);
        return 0;
    }

    // Read elf
    FILE *input = NULL;
    const char *input_name = argv[1];
    if (strcmp(input_name, "-") == 0) {
        input = stdin;
    } else {
        input = fopen(input_name, "r");
        if (input == NULL)  {
            fprintf(stderr, "Error (Emulator): Failed to open file '%s' for reading\n", input_name);
            return 1;
        }
    }

    // Uncomment the following when done tesitng
    //Elf_ReadHexInit(&elf, input);


    // Testing 

    Elf_Builder elf;
    Elf_Init(&elf);
    Test3(&elf);
    Elf_WriteDump(&elf, stdout);
    Emu_RunElf(&elf, stdout);
    Mem_Clear();
    return 0;
}
