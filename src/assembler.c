#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <assert.h>
#include <regex.h>
#include <ctype.h>
#include <assembler.h>
#include <elf.h>
#include <stdarg.h>
#include <util.h>

// Show assembly error
#define Show_AsmError(...)  \
    do { \
        if (Asm_LineNo > 0) { \
            fprintf(stderr, "\e[31mError\e[0m: Line %d: ", Asm_LineNo); \
        } else { \
            fprintf(stderr, "\e[31mError\e[0m: "); \
        } \
        fprintf(stderr, __VA_ARGS__); \
        fprintf(stderr, ":"); \
        if (Asm_Line) { \
            fprintf(stderr, "\n%s", Asm_Line); \
        } \
        fprintf(stderr, "\n"); \
        fflush(stderr); \
    } while (0)

// Show equ error
#define Show_EquError(...) \
    do { \
        fprintf(stderr, "\e[31mError\e[0m: Equ error: "); \
        fprintf(stderr, __VA_ARGS__); \
        fprintf(stderr, "\n"); \
        fflush(stderr); \
    } while (0)

// List of instructions
static Asm_InstrInfo Asm_InstrList[] = {
    { "halt",  I_HALT,  0x00,       0, I_ARGS_NONE },
    { "int",   I_INT,   0x10,       0, I_ARGS_NONE },
    { "iret",  I_IRET,  0x00,       0, I_ARGS_NONE },
    { "call",  I_CALL,  0x00,       1, I_ARGS_BOP },
    { "ret",   I_RET,   0x00,       0, I_ARGS_NONE },
    { "jmp",   I_JMP,   BRANCH_JMP, 1, I_ARGS_BOP },
    { "beq",   I_BEQ,   BRANCH_BEQ, 3, I_ARGS_REG_REG_BOP },
    { "bne",   I_BNE,   BRANCH_BNE, 3, I_ARGS_REG_REG_BOP },
    { "bgt",   I_BGT,   BRANCH_BGT, 3, I_ARGS_REG_REG_BOP },
    { "push",  I_PUSH,  0x81,       1, I_ARGS_REG },
    { "pop",   I_POP,   0x93,       1, I_ARGS_REG },
    { "xchg",  I_XCHG,  0x40,       2, I_ARGS_REG_REG },
    { "add",   I_ADD,   ALU_ADD,    2, I_ARGS_REG_REG },
    { "sub",   I_SUB,   ALU_SUB,    2, I_ARGS_REG_REG },
    { "mul",   I_MUL,   ALU_MUL,    2, I_ARGS_REG_REG },
    { "div",   I_DIV,   ALU_DIV,    2, I_ARGS_REG_REG },
    { "not",   I_NOT,   ALU_NOT,    1, I_ARGS_REG },
    { "and",   I_AND,   ALU_AND,    2, I_ARGS_REG_REG },
    { "or",    I_OR,    ALU_OR ,    2, I_ARGS_REG_REG },
    { "xor",   I_XOR,   ALU_XOR,    2, I_ARGS_REG_REG },
    { "shl",   I_SHL,   ALU_SHL,    2, I_ARGS_REG_REG },
    { "shr",   I_SHR,   ALU_SHR,    2, I_ARGS_REG_REG },
    { "ld",    I_LD,    0x00,       2, I_ARGS_DOP_REG },
    { "st",    I_ST,    0x00,       2, I_ARGS_REG_DOP },
    { "csrrd", I_CSRRD, 0x90,       2, I_ARGS_CSR_REG },
    { "csrwr", I_CSRWR, 0x94,       2, I_ARGS_REG_CSR },
    { "probe", I_PROBE, 0x00,       1, I_ARGS_DOP },
};

// List of directives
static Asm_DirecInfo Asm_DirecList[] = {
    { ".global",  D_GLOBAL,  D_ARGS_SYM_LIST },
    { ".globl",   D_GLOBAL,  D_ARGS_SYM_LIST },
    { ".extern",  D_EXTERN,  D_ARGS_SYM_LIST },
    { ".section", D_SECTION, D_ARGS_SYM      },
    { ".byte",    D_BYTE,    D_ARGS_SYM_LIT  },
    { ".half",    D_HALF,    D_ARGS_SYM_LIT  },
    { ".word",    D_WORD,    D_ARGS_SYM_LIT  },
    { ".skip",    D_SKIP,    D_ARGS_LIT      },
    { ".ascii",   D_ASCII,   D_ARGS_STR      },
    { ".equ",     D_EQU,     D_ARGS_SYM_EQU  },
    { ".end",     D_END,     D_ARGS_NONE     },
};


// List of equ operators
Equ_OperInfo Equ_OperList[] = {
    { "pos", O_POS, 2,  1, O_ASOC_RTL }, // +
    { "neg", O_NEG, 2,  1, O_ASOC_RTL }, // -
    { "~",   O_NOT, 2,  1, O_ASOC_LTR },
    { "*",   O_MUL, 3,  2, O_ASOC_LTR },
    { "/",   O_DIV, 3,  2, O_ASOC_LTR },
    { "%",   O_MOD, 3,  2, O_ASOC_LTR },
    { "+",   O_ADD, 4,  2, O_ASOC_LTR },
    { "-",   O_SUB, 4,  2, O_ASOC_LTR },
    { "<<",  O_SHL, 5,  2, O_ASOC_LTR },
    { ">>",  O_SHR, 5,  2, O_ASOC_LTR },
    { "&",   O_AND, 8,  2, O_ASOC_LTR },
    { "^",   O_XOR, 9,  2, O_ASOC_LTR },
    { "|",   O_OR,  10, 2, O_ASOC_LTR },
    { "(",   O_LBR, 15, 2, O_ASOC_LTR },
    { ")",   O_RBR, 15, 2, O_ASOC_LTR },
};

// Equ count
int Asm_EquCount = 0;

// Equ list 
Asm_EquType Asm_EquList[MAX_ASM_EQU];

// Current line number
int Asm_LineNo = 0;

// Current line being parsed
char *Asm_Line = NULL;

// Current line capacity
size_t Asm_LineCap = 0;

// Current asm mode
Asm_ModeType Asm_CurrentMode = ASM_MODE_START;

// Find instruction
int Asm_FindInstrIdx(const char *instr)
{
    int i;
    for (i = 0; i < ARR_SIZE(Asm_InstrList); i++) {
        if (Str_Equals(instr, Asm_InstrList[i].i_name))
            return i;
    }
    return -1;
}

// Find directive
int Asm_FindDirecIdx(const char *direct)
{
    int i;
    for (i = 0; i < ARR_SIZE(Asm_DirecList); i++) {
        if (Str_Equals(direct, Asm_DirecList[i].d_name))
            return i;
    }
    return -1;
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

// Load zero value into register
// gprD <= 0
void Asm_PushXorZero(Elf_Builder *elf, Asm_RegType gprD)
{
    // gprD <= gprD ^ gprD
    Elf_PushByte(elf, PACK(OC_LOGIC, MOD_LOGIC_XOR));
    Elf_PushByte(elf, PACK(gprD, gprD));
    Elf_PushByte(elf, PACK(gprD, 0x0));
    Elf_PushByte(elf, 0x00);
}

// Load a byte into a register
void Asm_PushLoadByte(Elf_Builder *elf, Asm_RegType rDest, Asm_Byte value)
{
    // rDest = 0
    Asm_PushXorZero(elf, rDest);

    // rDest += value
    Elf_PushByte(elf, PACK(OC_LOAD, MOD_LOAD_1));
    Elf_PushByte(elf, PACK(rDest, rDest));
    Elf_PushByte(elf, PACK(0x0, 0x0));
    Elf_PushByte(elf, value & 0xff);
}

// Load a half into register
// This operation requires assistance from another 'helper' register (recommended: IDX (13) or ADR (12))
void Asm_PushLoadHalf(Elf_Builder *elf, Asm_RegType rDest, Asm_Half value, Asm_RegType rHelp)
{
    int d = rDest;
    int s = rHelp;
    unsigned char code[] = {
        0x63, PACK(s, s),     PACK(s, 0),   0x00,                   // rHelp = 0x0
        0x91, PACK(s, s),     0x00,         0x08,                   // rHelp = 0x8
        0x63, PACK(d, d),     PACK(d, 0),   0x00,                   // rDest = 0
        0x91, PACK(d, d),     0x00,         ((value >> 8) & 0xff),  // rDest += (value >> 8) & 0xff
        0x70, PACK(d, d),     PACK(s, 0),   0x00,                   // rDest <<= rHelp
        0x91, PACK(d, d),     0x00,         (value & 0xff),         // rDest += value & 0xff
        0x63, PACK(s, s),     PACK(s, 0),   0x00,                   // rHelp = 0
    };
    Elf_PushBytes(elf, code, sizeof(code));
}

// Load a word into register
// This operation requires assistance from another 'helper' register (recommended: IDX (13) or ADR (12))
void Asm_PushLoadWord(Elf_Builder *elf, Asm_RegType rDest, Asm_Word value, Asm_RegType rHelp)
{

    int d = rDest;
    int s = rHelp;
    unsigned char code[] = {
        //0xa0, 0x00,           0x00,         0x00,                   // lock
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
        //0xa1, 0x00,           0x00,         0x00,                   // unlock
    };
    Elf_PushBytes(elf, code, sizeof(code));
}

// Push any type of instruction
void Asm_PushInstr(Elf_Builder *elf, Asm_OcType oc, Asm_ModType mod, Asm_RegType ra, Asm_RegType rb, Asm_RegType rc, Asm_DispType disp)
{
    Elf_PushByte(elf, ((oc & 0xf) << 4) | ((mod & 0xf) << 0));
    Elf_PushByte(elf, ((ra & 0xf) << 4) | ((rb  & 0xf) << 0));
    Elf_PushByte(elf, ((rc & 0xf) << 4) | (((disp >> 8) & 0xf) << 0));
    Elf_PushByte(elf, disp & 0xff);
}

// Push `halt` instruction
// Halts simulation
void Asm_PushHalt(Elf_Builder *elf)
{
    Elf_PushByte(elf, PACK(OC_HALT, 0x0));
    Elf_PushByte(elf, 0x00);
    Elf_PushByte(elf, 0x00);
    Elf_PushByte(elf, 0x00);
}

// Push `int` instruction
// Causes a software interrupt
void Asm_PushInt(Elf_Builder *elf)
{
    Elf_PushByte(elf, PACK(OC_INT, 0x0));
    Elf_PushByte(elf, 0x00);
    Elf_PushByte(elf, 0x00);
    Elf_PushByte(elf, 0x00);
}


// Push `iret` instruction
// push status; push pc;
static void Asm_PushBasicIret(Elf_Builder *elf)
{
    // status <= mem32[SP]; SP <= SP + 4
    Elf_PushByte(elf, PACK(OC_LOAD, MOD_LOAD_7));
    Elf_PushByte(elf, PACK(STATUS, SP));
    Elf_PushByte(elf, 0x00);
    Elf_PushByte(elf, 0x04);

    // pc <= mem32[SP]; SP <= SP + 4
    Elf_PushByte(elf, PACK(OC_LOAD, MOD_LOAD_3));
    Elf_PushByte(elf, PACK(PC, SP));
    Elf_PushByte(elf, 0x00);
    Elf_PushByte(elf, 0x04);
}

static void Asm_PushExtendedIret(Elf_Builder *elf)
{
    // Extended iret
    // idx <= mem32[SP]; SP <= SP + 4
    Elf_PushByte(elf, PACK(OC_LOAD, MOD_LOAD_3));
    Elf_PushByte(elf, PACK(IDX, SP));
    Elf_PushByte(elf, 0x00);
    Elf_PushByte(elf, 0x04);

    // adr <= mem32[SP]; SP <= SP + 4
    Elf_PushByte(elf, PACK(OC_LOAD, MOD_LOAD_3));
    Elf_PushByte(elf, PACK(ADR, SP));
    Elf_PushByte(elf, 0x00);
    Elf_PushByte(elf, 0x04);

    // status <= mem32[SP]; SP <= SP + 4
    Elf_PushByte(elf, PACK(OC_LOAD, MOD_LOAD_7));
    Elf_PushByte(elf, PACK(STATUS, SP));
    Elf_PushByte(elf, 0x00);
    Elf_PushByte(elf, 0x04);

    // pc <= mem32[SP]; SP <= SP + 4
    Elf_PushByte(elf, PACK(OC_LOAD, MOD_LOAD_3));
    Elf_PushByte(elf, PACK(PC, SP));
    Elf_PushByte(elf, 0x00);
    Elf_PushByte(elf, 0x04);
}

// Push `iret` instruction
void Asm_PushIret(Elf_Builder *elf)
{
    Asm_PushExtendedIret(elf);
}

// Push `ret` instruction
// pop pc;
void Asm_PushRet(Elf_Builder *elf)
{
    // pc <= mem32[SP]; SP <= SP + 4
    Elf_PushByte(elf, PACK(OC_LOAD, MOD_LOAD_3));
    Elf_PushByte(elf, PACK(PC, SP));
    Elf_PushByte(elf, 0x00);
    Elf_PushByte(elf, 0x04);
}

static void show_help() 
{
    const char *help = 
        "Usage: asembler [OPTIONS] <INPUT_FILE> \n"
        "\n"
        "Available options:\n"
        "  -o OUTPUT_FILE - Set output file\n"
        "  -h             - Show help and exist\n"
        "  -v             - Show version and exit\n"
    ;
    printf("%s\n", help);
    exit(EXIT_SUCCESS);
}


int main(int argc, char *argv[])
{

    // Input file
    FILE *input = NULL;

    // Output file
    FILE *output = NULL;

    // Flags
    Asm_FlagsType flags = 0;

    // Parse options
    int i;
    for (i = 1; i < argc; i++) {
        char *option = argv[i];
        if (Str_Equals(option, "-hex")) {
            // -hex option
            flags |= F_ASM_HEX;
        } else if (Str_Equals(option, "-o")) {
            // -o output option
            if (i + 1 == argc) {
                Show_Error("-o Option requires an argument");
                break;
            }
            char *output_path = argv[i + 1];
            i += 1;
            if (Str_Equals(output_path, "-")) {
                output = stdout;
            } else {
                output = fopen(output_path, "w");
                if (output == NULL) {
                    Show_Error("Failed to open '%s' for writing", output_path);
                    break;
                }
            }
        } else {
            // Input option (no argument)
            if (input) {
                Show_Warning("Multiple input files set. Only the first will be used.");
            } else {
                char *input_path = option;
                if (Str_Equals(input_path, "-")) {
                    input = stdin;
                } else {
                    input = fopen(input_path, "r");
                    if (input == NULL) {
                        Show_Error("Failed to open file '%s' for reading", input_path);
                        break;
                    }
                }
            }
        }
    }

    // Check validity
    if (input == NULL || output == NULL) {
        show_help();
    } else if (input == NULL) {
        Show_Error("No input");
    } else if (output == NULL) {
        Show_Error("No output");
    } else {
        // Create ELF
        Elf_Builder elf;
        Elf_Init(&elf);

        // Compile input
        Asm_Compile(&elf, input, F_ASM_NONE);

        // Testing only
        //Elf_WriteDump(&elf, stdout);

        // Write elf to output
        if (flags & F_ASM_HEX)  {
            Elf_WriteHex(&elf, output);
        } else {
            Elf_WriteDump(&elf, output);
        }

        // Destroy ELF
        Elf_Destroy(&elf);
    }

    // Close files
    if (input && input != stdin) 
        fclose(input);
    if (output && output != stdout)
        fclose(output);
    return 0;
}

