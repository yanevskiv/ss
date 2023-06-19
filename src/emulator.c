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

#define VA_BIT_PAGE 16
#define VA_BIT_WORD 16
#define VA_BIT_LENGTH (VA_BIT_PAGE + VA_BIT_WROD)
#define VA_PAGE_SIZE (1 << VA_BIT_WORD)
#define VA_PAGE_COUNT (1 << VA_BIT_PAGE)

#define VA_PAGE(addr) (((addr) >> VA_BIT_PAGE) & ((1 << VA_BIT_PAGE) - 1))
#define VA_WORD(addr) ((addr) & ((1 << VA_BIT_WORD) - 1))
#define ASM_INSTR_SIZE 0x4

#define PC 15
#define SP 14
#define STATUS 0
#define HANDLE 1
#define CAUSE  2

#define Asm_Byte  unsigned char
#define Asm_Short unsigned short
#define Asm_Word  unsigned int
#define Asm_Reg   int
#define Asm_Addr  Asm_Word

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
                printf("Rela");
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
    GPR[PC] = 0x40000000;
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
                // push status; push pc; cause <= 4; status <= status & (~0x1); pc <= handle;
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
}



void Asm_PushLoadLiteral(Elf_Builder *elf, int rDest, int value, int rShift, int pushShift)
{

    #define byte(x, y) (((x) << 4) | ((y) << 0))
    int d = rDest;
    int s = rShift;
    unsigned char code[] = {
        0x81, byte(0xe, 0xe), byte(s, 0), 0x04,                     // push rShift
        0x63, byte(s, s),     byte(s, 0), 0x00,                     // rShift = 0x0
        0x91, byte(s, s),     0x00,       0x08,                     // rShift = 0x8
        0x63, byte(d, d),     byte(d, 0), 0x00,                     // rDest = 0
        0x91, byte(d, d),     0x00,       ((value >> 24) & 0xff),   // rDest += 0xff
        0x70, byte(d, d),     byte(s, 0), 0x00,                     // rDest <<= r1
        0x91, byte(d, d),     0x00,       ((value >> 16) & 0xff),   // rDest += 0xff
        0x70, byte(d, d),     byte(s, 0), 0x00,                     // rDest <<= r1
        0x91, byte(d, d),     0x00,       ((value >> 8) & 0xff),    // rDest += 0xff
        0x70, byte(d, d),     byte(s, 0), 0x00,                     // rDest <<= r1
        0x91, byte(d, d),     0x00,       (value & 0xff),           // rDest += 0xff
        0x63, byte(s, s),     byte(s, 0), 0x00,                     // rShift = 0
        0x93, byte(s, 0xe),   0xef,       0xfc,                     // pop rShift
    };
    if (pushShift) {
        Elf_PushBytes(elf, code, sizeof(code));
    } else {
        Elf_PushBytes(elf, code + 4, sizeof(code) - 8);
    }
    #undef byte
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
    int value = 0xcafebabe;
    unsigned char code[] = {
        0x91, 0x12, 0x00, 0x15,                     // r1 = 0x5

        // ld $cafebabe, %r0
        //0x81, 0xee, 0x10, 0x04,                     // push r1
        //0x63, 0x11, 0x10, 0x00,                     // r1 = 0x0
        //0x91, 0x11, 0x00, 0x08,                     // r1 = 0x8
        //0x63, 0x00, 0x00, 0x00,                     // r0 = 0
        //0x91, 0x00, 0x00, ((value >> 24) & 0xff),   // r0 += 0xff
        //0x70, 0x00, 0x10, 0x00,                     // r0 <<= r1
        //0x91, 0x00, 0x00, ((value >> 16) & 0xff),   // r0 += 0xff
        //0x70, 0x00, 0x10, 0x00,                     // r0 <<= r1
        //0x91, 0x00, 0x00, ((value >> 8) & 0xff),    // r0 += 0xff
        //0x70, 0x00, 0x10, 0x00,                     // r0 <<= r1
        //0x91, 0x00, 0x00, (value & 0xff),           // r0 += 0xff
        //0x93, 0x1e, 0xef, 0xfc,                     // pop r1

        // ld $cafebabe, %sp


        //0x00, 0x00, 0x00, 0x00,                     // halt

    };

    Elf_Builder elf;
    Elf_Init(&elf);
    Elf_PushSection(&elf);
    Elf_UseSection(&elf, "fdsa");
    Elf_Shdr *shdr = Elf_GetSection(&elf);
    shdr->sh_addr = 0x40000000;
    Elf_UseSymbol(&elf, Elf_GetSectionName(&elf))->st_value = shdr->sh_addr;

    Elf_PushBytes(&elf, code, sizeof(code));

    Asm_PushLoadLiteral(&elf, 0, value, 1, 0);

    Elf_PopSection(&elf);
    Elf_WriteDump(&elf, stdout);

    GPR[SP] = 0xffff0000;
    Emu_RunElf(&elf, stdout);
    Mem_Clear();
    return 0;
}
