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

// Push `push gprS` instruction
// The stack is assumed to grow downwards (SP -= 4)
void Asm_PushStackPush(Elf_Builder *elf, Asm_RegType gprS)
{
    // SP <= SP - 4; mem32[SP] <= gprS
    Elf_PushByte(elf, PACK(OC_STORE, MOD_STORE_2));
    Elf_PushByte(elf, PACK(SP, SP));
    Elf_PushByte(elf, PACK(gprS, 0xf));
    Elf_PushByte(elf, 0xfc);
}

// Push `pop %gprD` instruction (SP += 4)
void Asm_PushStackPop(Elf_Builder *elf, Asm_RegType gprD)
{
    // gprD <= mem32[SP]; SP <= SP + 4
    Elf_PushByte(elf, PACK(OC_LOAD, MOD_LOAD_3));
    Elf_PushByte(elf, PACK(gprD, SP));
    Elf_PushByte(elf, 0x00);
    Elf_PushByte(elf, 0x04);
}

// Push binary `xchg` operation (swap two register values)
void Asm_PushXchg(Elf_Builder *elf, Asm_RegType gpr1, Asm_RegType gpr2)
{
    // tmp <= gpr1; gpr1 <= gpr2; gpr2 <= tmp
    Elf_PushByte(elf, PACK(OC_XCHG, 0x0));
    Elf_PushByte(elf, PACK(0x0, gpr1));
    Elf_PushByte(elf, PACK(gpr2, 0x0));
    Elf_PushByte(elf, 0x00);
}

// Perform a binary ALU operation (+ - * / & | ^ << >>)
void Asm_PushALU(Elf_Builder *elf, Asm_AluType op, Asm_RegType gprS, Asm_RegType gprD)
{
    // gprD <= gprD OP gprS
    Elf_PushByte(elf, op);
    Elf_PushByte(elf, PACK(gprD, gprD));
    Elf_PushByte(elf, PACK(gprS, 0x0));
    Elf_PushByte(elf, 0x00);
}

// Push binary `add` operation
void Asm_PushAdd(Elf_Builder *elf, Asm_RegType gprS, Asm_RegType gprD)
{
    // gprD <= gprD + gprS
    Elf_PushByte(elf, PACK(OC_ARITH, MOD_ARITH_ADD));
    Elf_PushByte(elf, PACK(gprD, gprD));
    Elf_PushByte(elf, PACK(gprS, 0x0));
    Elf_PushByte(elf, PACK(0x0, 0x0));
}

// Push binary `sub` operation
void Asm_PushSub(Elf_Builder *elf, Asm_RegType gprS, Asm_RegType gprD)
{
    // gprD <= gprD - gprS
    Elf_PushByte(elf, PACK(OC_ARITH, MOD_ARITH_SUB));
    Elf_PushByte(elf, PACK(gprD, gprD));
    Elf_PushByte(elf, PACK(gprS, 0x0));
    Elf_PushByte(elf, PACK(0x0, 0x0));
}

// Push binary `mul` operation
void Asm_PushMul(Elf_Builder *elf, Asm_RegType gprS, Asm_RegType gprD)
{
    // gprD <= gprD * gprS
    Elf_PushByte(elf, PACK(OC_ARITH, MOD_ARITH_MUL));
    Elf_PushByte(elf, PACK(gprD, gprD));
    Elf_PushByte(elf, PACK(gprS, 0x0));
    Elf_PushByte(elf, PACK(0x0, 0x0));
}

// Push binary `div` operation
void Asm_PushDiv(Elf_Builder *elf, Asm_RegType gprS, Asm_RegType gprD)
{
    // gprD <= gprD / gprS
    Elf_PushByte(elf, PACK(OC_ARITH, MOD_ARITH_DIV));
    Elf_PushByte(elf, PACK(gprD, gprD));
    Elf_PushByte(elf, PACK(gprS, 0x0));
    Elf_PushByte(elf, PACK(0x0, 0x0));
}

// Push unary `not` operation
void Asm_PushNot(Elf_Builder *elf, Asm_RegType gprD)
{
    // gprD <= ~gprD
    Elf_PushByte(elf, PACK(OC_LOGIC, MOD_LOGIC_NOT));
    Elf_PushByte(elf, PACK(gprD, gprD));
    Elf_PushByte(elf, 0x00);
    Elf_PushByte(elf, 0x00);
}

// Push binary `and` operation
void Asm_PushAnd(Elf_Builder *elf, Asm_RegType gprS, Asm_RegType gprD)
{
    // gprD <= gprD & gprS
    Elf_PushByte(elf, PACK(OC_LOGIC, MOD_LOGIC_AND));
    Elf_PushByte(elf, PACK(gprD, gprD));
    Elf_PushByte(elf, PACK(gprS, 0x0));
    Elf_PushByte(elf, PACK(0x0, 0x0));
}

// Push binary `or` operation
void Asm_PushOr(Elf_Builder *elf, Asm_RegType gprS, Asm_RegType gprD)
{
    // gprD <= gprD | gprS
    Elf_PushByte(elf, PACK(OC_LOGIC, MOD_LOGIC_OR));
    Elf_PushByte(elf, PACK(gprD, gprD));
    Elf_PushByte(elf, PACK(gprS, 0x0));
    Elf_PushByte(elf, PACK(0x0, 0x0));
}

// Push binary `xor` operation
void Asm_PushXor(Elf_Builder *elf, Asm_RegType gprS, Asm_RegType gprD)
{
    // gprD <= gprD ^ gprS
    Elf_PushByte(elf, PACK(OC_LOGIC, MOD_LOGIC_XOR));
    Elf_PushByte(elf, PACK(gprD, gprD));
    Elf_PushByte(elf, PACK(gprS, 0x0));
    Elf_PushByte(elf, PACK(0x0, 0x0));
}

// Push binary `shl` operation
void Asm_PushShl(Elf_Builder *elf, Asm_RegType gprS, Asm_RegType gprD)
{
    // gprD <= gprD << gprS
    Elf_PushByte(elf, PACK(OC_SHIFT, MOD_SHIFT_LEFT));
    Elf_PushByte(elf, PACK(gprD, gprD));
    Elf_PushByte(elf, PACK(gprS, 0x0));
    Elf_PushByte(elf, PACK(0x0, 0x0));
}

// Push binary `shr` operation
void Asm_PushShr(Elf_Builder *elf, Asm_RegType gprS, Asm_RegType gprD)
{
    // gprD <= gprD >> gprS
    Elf_PushByte(elf, PACK(OC_SHIFT, MOD_SHIFT_RIGHT));
    Elf_PushByte(elf, PACK(gprD, gprD));
    Elf_PushByte(elf, PACK(gprS, 0x0));
    Elf_PushByte(elf, PACK(0x0, 0x0));
}

// Push `csrrd` instruction
// (Load a control register value into a general purpose register)
void Asm_PushCsrrd(Elf_Builder *elf, Asm_RegType csrS, Asm_RegType gprD)
{
    // gprD <= csrS
    Elf_PushByte(elf, PACK(OC_LOAD, MOD_LOAD_0));
    Elf_PushByte(elf, PACK(gprD, csrS));
    Elf_PushByte(elf, 0x00);
    Elf_PushByte(elf, 0x00);
}

// Push `csrwr` instruction
// (Store a general purpse register value into a control register)
void Asm_PushCsrwr(Elf_Builder *elf, Asm_RegType gprS, Asm_RegType csrD)
{
    // csrD <= gprS
    Elf_PushByte(elf, PACK(OC_LOAD, MOD_LOAD_4));
    Elf_PushByte(elf, PACK(csrD, gprS));
    Elf_PushByte(elf, 0x00);
    Elf_PushByte(elf, 0x00);
}

// Load a word into register using literal or symbol
// gprD <= literal | symbol
// Side effects:
//  - ADR register will be invalidated if gprD = { IDX, SP, PC }
//  - IDX register will be invalidated in all cases
void Asm_PushLoadValue(Elf_Builder *elf, Asm_RegType gprD, Elf_Word literal, const char *symName)
{
    if (symName != NULL) {
        // Add relocation for the next instruction if symbol name is present
        Asm_AddRela(elf, symName, R_SS_LD32, 0);
    }
    switch (gprD) {
        case SP:
        case PC: {
            // Load word into ADR register with the help of IDX
            Asm_PushLoadWord(elf, ADR, literal, IDX);

            // Exchange ADR with PC or SP
            Asm_PushXchg(elf, ADR, gprD);
        } break;
        case IDX: {
            // Load word into IDX with the help of ADR
            Asm_PushLoadWord(elf, IDX, literal, ADR);
        } break;
        default: {
            // Load word into gprD with the help of IDX
            Asm_PushLoadWord(elf, gprD, literal, IDX);
        } break;
    }
}

// Load a word from memory into register using literal or symbol
// gprD <= mem32[literal | symbol];
// Side effects:
//  - IDX register will be invalidated (zeroed out, unless gprD is IDX)
//  - ADR register will be invalidated (containing the address, unless gprD is ADR)
void Asm_PushLoadMemValue(Elf_Builder *elf, Asm_RegType gprD, Elf_Addr addr, const char *symName)
{
    if (symName != NULL) {
        // Add relocation for load word
        Asm_AddRela(elf, symName, R_SS_LD32, 0);
    }

    // Load address into ADR with the help of IDX
    Asm_PushLoadWord(elf, ADR, addr, IDX);

    // Zero out the IDX register (need a zero somewhere for the next instruction)
    Asm_PushXorZero(elf, IDX);

    // Load word into regigster gprD from address in regsiter ADR
    // gprD <= mem32[ADR + IDX + 0]
    Elf_PushByte(elf, PACK(OC_LOAD, MOD_LOAD_2));
    Elf_PushByte(elf, PACK(gprD, ADR));
    Elf_PushByte(elf, PACK(IDX, 0x0));
    Elf_PushByte(elf, 0x00);
}

// Push `ld $literal, %gprD` instruction
// gprD <= literal
void Asm_PushLoadLiteralValue(Elf_Builder *elf, Asm_Word literal, Asm_RegType gprD)
{
    Asm_PushLoadValue(elf, gprD, literal, NULL);
}

// Push `ld $symbol, %gprD` instruction
// gprD <= symbol
void Asm_PushLoadSymbolValue(Elf_Builder *elf, const char *symName, Asm_RegType gprD)
{
    Asm_PushLoadValue(elf, gprD, 0x0, symName);
}

// Push `ld literal, %gprD` instruction
// gprD <= mem32[literal]
void Asm_PushLoadMemLiteralValue(Elf_Builder *elf, Asm_Addr literal, Asm_RegType gprD)
{
    Asm_PushLoadMemValue(elf, gprD, literal, NULL);
}

// Push `ld symbol, %gprD` instruction
// gprD <= mem32[symbol]
void Asm_PushLoadMemSymbolValue(Elf_Builder *elf, const char *symName, Asm_RegType gprD)
{
    Asm_PushLoadMemValue(elf, gprD, 0x0, symName);
}

// Push `ld  %gprS, %gprD` instruction 
// gprD <= gprS
void Asm_PushLoadReg(Elf_Builder *elf, Asm_RegType gprS, Asm_RegType gprD)
{
    // gprD <= gprS
    Elf_PushByte(elf, PACK(OC_LOAD, MOD_LOAD_1));
    Elf_PushByte(elf, PACK(gprD, gprS));
    Elf_PushByte(elf, 0x00);
    Elf_PushByte(elf, 0x00);
}

// Push `ld [%gprS + literal], %gprD` instruction 
// gprD <= mem32[gprS + disp]
void Asm_PushLoadMemRegDisp(Elf_Builder *elf, Asm_RegType gprS, Asm_DispType disp, Asm_RegType gprD)
{
    // Zero out the IDX register
    Asm_PushXorZero(elf, IDX);

    // gprD <= mem32[IDX + gprS + disp]; (note: IDX = 0)
    Elf_PushByte(elf, PACK(OC_LOAD, MOD_LOAD_2));
    Elf_PushByte(elf, PACK(gprD, IDX));
    Elf_PushByte(elf, PACK(gprS, disp >> 8));
    Elf_PushByte(elf, disp & 0xff);
}

// Push `ld [%gprS + symbol], %gprD` instruction 
// gprD <= gprS + symDisp
void Asm_PushLoadMemRegSymbolDisp(Elf_Builder *elf, Asm_RegType gprS, const char *symName, Asm_RegType gprD)
{
    // Zero out the IDX register
    Asm_PushXorZero(elf, gprD);

    // Add displacement relocation for next instruction
    Asm_AddRela(elf, symName, R_SS_D12, 0);

    // gprD <= mem32[IDX + gprS + disp]; (Note: IDX = 0)
    Elf_PushByte(elf, PACK(OC_LOAD, MOD_LOAD_2));
    Elf_PushByte(elf, PACK(gprD, IDX));
    Elf_PushByte(elf, PACK(gprS, 0));
    Elf_PushByte(elf, 0);
}

// Push `st %gprS, $literal` instruction
// mem32[literal] <= gprS
void Asm_PushStoreAddr(Elf_Builder *elf, Asm_RegType gprS, Elf_Addr addr)
{
    // Load address into ADR with the help of IDX
    Asm_PushLoadWord(elf, ADR, addr, IDX);
    
    // Zero out the IDX register
    Asm_PushXorZero(elf, IDX);

    // mem32[ADR + IDX + 0] <= gprS
    Elf_PushByte(elf, PACK(OC_STORE, MOD_STORE_0));
    Elf_PushByte(elf, PACK(ADR, IDX));
    Elf_PushByte(elf, PACK(gprS, 0x0));
    Elf_PushByte(elf, 0x00);
}

// Push `st %gprS, $symbol` instruction
// mem32[symbol] <= gprS
void Asm_PushStoreSymbolAddr(Elf_Builder *elf, Asm_RegType gprS, const char *symName)
{
    // Add word loading relocation for the next set of instructions
    Asm_AddRela(elf, symName, R_SS_LD32, 0);

    // Load address into ADR with the help of IDX
    Asm_PushLoadWord(elf, ADR, 0x0, IDX);
    
    // Zero out the IDX register
    Asm_PushXorZero(elf, IDX);

    // mem32[ADR + IDX + 0] <= gprS
    Elf_PushByte(elf, PACK(OC_STORE, MOD_STORE_0));
    Elf_PushByte(elf, PACK(ADR, IDX));
    Elf_PushByte(elf, PACK(gprS, 0x0));
    Elf_PushByte(elf, 0x00);
}

// Push `st %gprS, literal` instrction
// mem32[mem32[literal]] <= gprS
void Asm_PushStoreMemAddr(Elf_Builder *elf, Asm_RegType gprS, Elf_Addr addr)
{
    // Load address into ADR with the help of IDX
    Asm_PushLoadWord(elf, ADR, 0x0, IDX);
    
    // Zero out the IDX register
    Asm_PushXorZero(elf, IDX);

    // mem32[mem32[ADR + IDX + 0]] <= gprS
    Elf_PushByte(elf, PACK(OC_STORE, MOD_STORE_1));
    Elf_PushByte(elf, PACK(ADR, IDX));
    Elf_PushByte(elf, PACK(gprS, 0x0));
    Elf_PushByte(elf, 0x00);
}

// Push `st %gprS, symbol` instrction
// mem32[mem32[symbol]] <= gprS
void Asm_PushStoreSymbolMemAddr(Elf_Builder *elf, Asm_RegType gprS, const char *symName)
{
    // Add word loading relocation for the next set of instructions
    Asm_AddRela(elf, symName, R_SS_LD32, 0);

    // Load address into ADR with the help of IDX
    Asm_PushLoadWord(elf, ADR, 0x0, IDX);
    
    // Zero out the IDX register
    Asm_PushXorZero(elf, IDX);

    // mem32[mem32[ADR + IDX + 0]] <= gprS
    Elf_PushByte(elf, PACK(OC_STORE, MOD_STORE_1));
    Elf_PushByte(elf, PACK(ADR, IDX));
    Elf_PushByte(elf, PACK(gprS, 0x0));
    Elf_PushByte(elf, 0x00);
}

// Push `st %gprS, %gprD` (equivalent to `ld %gprS, gprD`)
// gprD <= gprS
void Asm_PushStoreReg(Elf_Builder *elf, Asm_RegType gprS, Asm_RegType gprD)
{
    Asm_PushLoadReg(elf, gprS, gprD);
}

// Push `st %gprS, [%gprD + disp]`
// mem32[gprD + disp] <= gprS
void Asm_PushStoreMemRegDisp(Elf_Builder *elf, Asm_RegType gprS, Asm_RegType gprD, Asm_DispType disp)
{
    // Zero out the IDX register
    Asm_PushXorZero(elf, IDX);

    // mem32[gprD + IDX + disp] <= gprS
    Elf_PushByte(elf, PACK(OC_STORE, MOD_STORE_0));
    Elf_PushByte(elf, PACK(gprD, IDX));
    Elf_PushByte(elf, PACK(gprS, disp >> 8));
    Elf_PushByte(elf, disp & 0xff);
}

// Push `st %gprS, [%gprD + symDisp]`
// mem32[gprD + symDisp] <= gprS
void Asm_PushStoreMemRegSymbolDisp(Elf_Builder *elf, Asm_RegType gprS, Asm_RegType gprD, const char *symName)
{
    // Zero out the IDX register
    Asm_PushXorZero(elf, IDX);

    // Add displacement relocation for the next instruction
    Asm_AddRela(elf, symName, R_SS_D12, 0);

    // mem32[gprD + IDX + 0] <= gprS
    Elf_PushByte(elf, PACK(OC_STORE, MOD_STORE_0));
    Elf_PushByte(elf, PACK(gprD, IDX));
    Elf_PushByte(elf, PACK(gprS, 0x0));
    Elf_PushByte(elf, 0x00);
}


// Push branch reg
// if (gprB ?op gprC) pc <= regA + disp
void Asm_PushBranchReg(Elf_Builder *elf, Asm_BranchType op, Asm_RegType gprA, Asm_RegType gprB, Asm_RegType gprC, Asm_DispType disp)
{
    Elf_PushByte(elf, op);
    Elf_PushByte(elf, PACK(gprA, gprB));
    Elf_PushByte(elf, PACK(gprC, disp >> 8));
    Elf_PushByte(elf, disp & 0xff);
}

// Push branch addr/symbolAddr
// if (gpr1 ?op gpr2) pc <= (addr | symAddr) + disp
void Asm_PushBranch(Elf_Builder *elf, Asm_BranchType op, Asm_Addr addr, const char *symName, Asm_RegType gpr1, Asm_RegType gpr2, Asm_DispType disp)
{
    if (symName != NULL) {
        // Add relocation for load word
        Asm_AddRela(elf, symName, R_SS_LD32, 0);
    }

    // Load addr into ADR register with the help of IDX register
    Asm_PushLoadWord(elf, ADR, addr, IDX);


    // Prepare regsiter values
    switch (op) {
        case BRANCH_CALL:
        case BRANCH_MEM_CALL:
        {
            // Zero out IDX register
            Asm_PushXorZero(elf, IDX);
            gpr1 = IDX;
            gpr2 = 0;
        } break;
        case BRANCH_JMP:
        case BRANCH_MEM_JMP: 
        {
            gpr1 = 0;
            gpr2 = 0;
        } break;
    }


    // Push branch instruction
    Elf_PushByte(elf, op);
    Elf_PushByte(elf, PACK(ADR, gpr1));
    Elf_PushByte(elf, PACK(gpr2, disp >> 8));
    Elf_PushByte(elf, disp & 0xff);
}


// Push `jmp literal` 
// pc <= literal
void Asm_PushJmpLiteral(Elf_Builder *elf, Elf_Addr addr)
{
    Asm_PushBranch(elf, BRANCH_JMP, addr, NULL, 0, 0, 0);
}

// Push `beq %reg1, %reg2, literal`
// if (reg1 == reg2) pc <= literal
void Asm_PushBeqLiteral(Elf_Builder *elf, Asm_RegType gpr1, Asm_RegType gpr2, Elf_Addr addr)
{
    Asm_PushBranch(elf, BRANCH_BEQ, addr, NULL, gpr1, gpr2, 0);
}

// Push `bne %reg1, %reg2, literal` 
// if (reg1 != reg2) pc <= literal
void Asm_PushBneLiteral(Elf_Builder *elf, Asm_RegType gpr1, Asm_RegType gpr2, Elf_Addr addr)
{
    Asm_PushBranch(elf, BRANCH_BNE, addr, NULL, gpr1, gpr2, 0);
}

// Push `bgt %reg1, %reg2, literal`
// if (reg1 signed> reg2) pc <= literal
void Asm_PushBgtLiteral(Elf_Builder *elf, Asm_RegType gpr1, Asm_RegType gpr2, Elf_Addr addr)
{
    Asm_PushBranch(elf, BRANCH_BGT, addr, NULL, gpr1, gpr2, 0);
}

// Push `call literal`
// push pc; pc <= literal
void Asm_PushCallLiteral(Elf_Builder *elf, Elf_Addr addr)
{
    Asm_PushBranch(elf, BRANCH_CALL, addr, NULL, 0, 0, 0);
}

// Push `jmp symbol`
// pc <= symbol
void Asm_PushJmpSymbol(Elf_Builder *elf, const char *symName)
{
    Asm_PushBranch(elf, BRANCH_JMP, 0x0, symName, 0, 0, 0);
}

// Push `beq %reg1, %reg2, symbol` 
// if (reg1 == reg2) pc <= symbol
void Asm_PushBeqSymbol(Elf_Builder *elf, Asm_RegType gpr1, Asm_RegType gpr2, const char *symName)
{
    Asm_PushBranch(elf, BRANCH_BEQ, 0x0, symName, gpr1, gpr2, 0);
}

// Push `bne %reg1, %reg2, symbol` 
// if (reg1 != reg2) pc <= symbol
void Asm_PushBneSymbol(Elf_Builder *elf, Asm_RegType gpr1, Asm_RegType gpr2, const char *symName)
{
    Asm_PushBranch(elf, BRANCH_BNE, 0x0, symName, gpr1, gpr2, 0);
}

// Push `bgt %reg1, %reg2, symbol` 
// if (reg1 signed> reg2) pc <= symbol
void Asm_PushBgtSymbol(Elf_Builder *elf, Asm_RegType gpr1, Asm_RegType gpr2, const char *symName)
{
    Asm_PushBranch(elf, BRANCH_BGT, 0x0, symName, gpr1, gpr2, 0);
}

// Push `call symbol` 
// push pc; pc <= symbol
void Asm_PushCallSymbol(Elf_Builder *elf, const char *symName)
{
    Asm_PushBranch(elf, BRANCH_CALL, 0x0, symName, 0, 0, 0);
}


// Push `probe %reg`
void Asm_PushProbeReg(Elf_Builder *elf, Asm_RegType gprD)
{
    Elf_PushByte(elf, PACK(OC_PROBE, MOD_PROBE_REG));
    Elf_PushByte(elf, PACK(gprD, 0x0));
    Elf_PushByte(elf, 0x00);
    Elf_PushByte(elf, 0x00);
}

// Push `probe $lit`
void Asm_PushProbeLit(Elf_Builder *elf, Asm_DispType lit)
{
    Elf_PushByte(elf, PACK(OC_PROBE, MOD_PROBE_LIT));
    Elf_PushByte(elf, PACK(0x0, 0x0));
    Elf_PushByte(elf, PACK(0x0, lit >> 8));
    Elf_PushByte(elf, lit & 0xff);
}

// Push `probe [%reg + disp]`
void Asm_PushProbeMemRegDisp(Elf_Builder *elf, Asm_RegType gprS, Asm_DispType disp)
{
    Elf_PushByte(elf, PACK(OC_PROBE, MOD_PROBE_MEM_REG_DISP));
    Elf_PushByte(elf, PACK(gprS, 0x0));
    Elf_PushByte(elf, PACK(0x0, disp >> 8));
    Elf_PushByte(elf, disp & 0xff);
}

// Push `probe all`
void Asm_PushProbeAll(Elf_Builder *elf)
{
    Elf_PushByte(elf, PACK(OC_PROBE, MOD_PROBE_ALL));
    Elf_PushByte(elf, 0x00);
    Elf_PushByte(elf, 0x00);
    Elf_PushByte(elf, 0x00);
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

