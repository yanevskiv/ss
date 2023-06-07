#include <stdlib.h>
#include <emulator.h>
#include <assert.h>
#include <elf.h>
#include <string.h>

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
#define Asm_Reg   Asm_Word
#define Asm_Addr  Asm_Word

Asm_Reg GPR[16] = { 4, 2 };
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
void Mem_Read(void *data, int startAddr, int size)
{
    unsigned char *bytes = (unsigned char*)data;
    int i;
    for (i = 0; i < size; i++) {
        bytes[i] = Mem_ReadByte(startAddr + i);
    }
}

// Write bytes
void Mem_Write(void *data, int startAddr, int size)
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
    GPR[SP] = 0x8000ffff;
    /*
     * Read elf
     */
    
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
        Mem_Read(bytes, GPR[PC], ASM_INSTR_SIZE);
        GPR[PC] += ASM_INSTR_SIZE;
        int oc   = (bytes[0] >> 4) & 0xf;
        int mod  = (bytes[0] >> 0) & 0xf;
        int ra   = (bytes[1] >> 4) & 0xf;
        int rb   = (bytes[1] >> 0) & 0xf;
        int rc   = (bytes[2] >> 4) & 0xf; 
        int disp = (((bytes[2] >> 0) & 0xf) << 8) | bytes[3];

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



int main(int argc, char *argv[]) {
    if (argc != 2)  {
        show_help(stdout);
        return 0;
    }

    // Read elf
    FILE *input = NULL;
    const char *input_name = argv[1];
    Elf_Builder elf;
    if (strcmp(input_name, "-") == 0) {
        input = stdin;
    } else {
        input = fopen(input_name, "r");
        if (input == NULL)  {
            fprintf(stderr, "Error (Emulator): Failed to open file '%s' for reading\n", input_name);
            return 1;
        }
    }
    Elf_ReadHexInit(&elf, input);

    unsigned char code[] = {
        0x50, 0x00, 0x10, 0x00,
        0x90, 0x00, 0x10, 0x00,
        0x00, 0x00, 0x00, 0x00
    };
    Mem_Write(code, 0x40000000, sizeof(code));
    Emu_RunElf(&elf, stdout);
    Mem_Clear();
    return 0;
}
