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

regmatch_t matches[MAX_REGEX_MATCHES];

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
void Asm_PushBasicIret(Elf_Builder *elf)
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

void Asm_PushExtendedIret(Elf_Builder *elf)
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
    // gprD <= 0
    Asm_PushXorZero(elf, gprD);

    // gprD <= mem32[gprD + gprS + disp]; (note: gprD = 0)
    Elf_PushByte(elf, PACK(OC_LOAD, MOD_LOAD_2));
    Elf_PushByte(elf, PACK(gprD, gprD));
    Elf_PushByte(elf, PACK(gprS, disp >> 8));
    Elf_PushByte(elf, disp & 0xff);
}

// Push `ld [%gprS + symbol], %gprD` instruction 
// gprD <= gprS + symDisp
void Asm_PushLoadMemRegSymbolDisp(Elf_Builder *elf, Asm_RegType gprS, const char *symName, Asm_RegType gprD)
{
    // Zero out the gprD register
    Asm_PushXorZero(elf, gprD);

    // Add displacement relocation for next instruction
    Asm_AddRela(elf, symName, R_SS_D12, 0);

    // gprD <= mem32[gprD + gprS + disp]
    Elf_PushByte(elf, PACK(OC_LOAD, MOD_LOAD_2));
    Elf_PushByte(elf, PACK(gprD, gprD));
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

// Parses general purpose registers:
// 1. %sp (same as %r14)
// 2. %pc (same as %r15)
// 3. %r0-r15
int Asm_ParseRegOperand(const char *str)
{
    if (Str_Equals(str, "%sp")) {
        return 14;
    } else if (Str_Equals(str, "%pc")) {
        return 15;
    } else if (Str_CheckMatch(str, "^%r([0-9]+)$"))  {
        int reg = strtol(str + 2, NULL, 10);
        if (reg > 15)  {
            return -1;
        }
        return reg;
    } 
    return -1;
}

// Parses control registers:
// 1. %status
// 2. %handler
// 3. %cause
int Asm_ParseCsrOperand(const char *str)
{
    if (Str_Equals(str, "%status")) {
        return 0;
    } else if (Str_Equals(str, "%handler")) {
        return 1;
    } else if (Str_Equals(str, "%cause")) {
        return 2;
    }
    return -1;
}

// Extracts arguments and returns argc
int Asm_ExtractArguments(char *other, char **args, int max_args)
{
    int argc = 0;
    char *token = strtok(other, ",");
    while (token) {
        args[argc] = strdup(token);
        Str_Trim(args[argc]);
        argc += 1;
        token = strtok(NULL, ",");
    }
    return argc;
}

// Parses symbols / literals / registers / reg offsets:
// 1.  $literal
// 2.  $symbol
// 3.  literal
// 4.  symbol
// 5.  %reg
// 6.  [ %reg ]
// 7.  [ %reg + literal ]
// 8.  [ %reg + symbol ]
Asm_OperandType *Asm_ParseDataOperand(const char *str)
{
    char *extract1 = NULL, *extract2 = NULL;
    Asm_OperandType *ao = calloc(1, sizeof(Asm_OperandType));
    assert(ao != NULL);
    ao->ao_type = AO_INVALID;
    if (Str_RegexMatch(str, "^\\$'(\\\\?.)'$", ARR_SIZE(matches), matches)) {
        extract1 = Str_Substr(str, matches[1].rm_so, matches[1].rm_eo);
        ao->ao_type = AO_LIT;
        ao->ao_lit = Str_UnescapeChar(extract1);
    } else if (Str_RegexMatch(str, "^\\$([-+0-9a-fA-F][xXa-fA-F0-9]*)$", ARR_SIZE(matches), matches)) {
        extract1 = Str_Substr(str, matches[1].rm_so, matches[1].rm_eo);
        ao->ao_type = AO_LIT;
        ao->ao_lit = Str_ParseInt(extract1);
    } else if (Str_RegexMatch(str, "^\\$([_a-zA-Z][_a-zA-Z0-9]*)$", ARR_SIZE(matches), matches)) {
        extract1 = Str_Substr(str, matches[1].rm_so, matches[1].rm_eo);
        ao->ao_type = AO_SYM;
        ao->ao_sym = strdup(extract1);
    } else if (Str_RegexMatch(str, "^'(\\?.)'$", ARR_SIZE(matches), matches)) {
        extract1 = Str_Substr(str, matches[1].rm_so, matches[1].rm_eo);
        ao->ao_type = AO_MEM_LIT;
        ao->ao_lit = Str_UnescapeChar(extract1);
    } else if (Str_RegexMatch(str, "^([-+0-9a-fA-F][xXa-fA-F0-9]*)$", ARR_SIZE(matches), matches)) {
        extract1 = Str_Substr(str, matches[1].rm_so, matches[1].rm_eo);
        ao->ao_type = AO_MEM_LIT;
        ao->ao_lit = Str_ParseInt(extract1);
    } else if (Str_RegexMatch(str, "^([_a-zA-Z][_a-zA-Z0-9]*)$", ARR_SIZE(matches), matches)) {
        extract1 = Str_Substr(str, matches[1].rm_so, matches[1].rm_eo);
        ao->ao_type = AO_MEM_SYM;
        ao->ao_sym = strdup(extract1);
    } else if (Str_RegexMatch(str, "^(%sp|%pc|%r[0-9]|%r1[0-5])$", ARR_SIZE(matches), matches)) {
        extract1 = Str_Substr(str, matches[1].rm_so, matches[1].rm_eo);
        ao->ao_type = AO_REG;
        ao->ao_reg = Asm_ParseRegOperand(extract1);
    } else if (Str_RegexMatch(str, "^\\[[ \t]*(%sp|%pc|%r[0-9]|%r1[0-5])[ \t]*\\]$", ARR_SIZE(matches), matches)) {
        extract1 = Str_Substr(str, matches[1].rm_so, matches[1].rm_eo);
        ao->ao_type = AO_MEM_REG;
        ao->ao_reg = Asm_ParseRegOperand(extract1);
    } else if (Str_RegexMatch(str, "^\\[[ \t]*(%sp|%pc|%r[0-9]|%r1[0-5])[ \t]*\\+[ \t]*([-+0-9a-fA-F][xXa-fA-F0-9]*)\\]$", ARR_SIZE(matches), matches)) {
        extract1 = Str_Substr(str, matches[1].rm_so, matches[1].rm_eo);
        extract2 = Str_Substr(str, matches[2].rm_so, matches[2].rm_eo);
        ao->ao_type = AO_MEM_REG_LIT;
        ao->ao_reg = Asm_ParseRegOperand(extract1);
        ao->ao_lit = Str_ParseInt(extract2);
    } else if (Str_RegexMatch(str, "^\\[[ \t]*(%sp|%pc|%r[0-9]|%r1[0-5])[ \t]*\\+[ \t]*([_a-zA-Z][_a-zA-Z0-9]*)\\]$", ARR_SIZE(matches), matches)) {
        extract1 = Str_Substr(str, matches[1].rm_so, matches[1].rm_eo);
        extract2 = Str_Substr(str, matches[2].rm_so, matches[2].rm_eo);
        ao->ao_type = AO_MEM_REG_SYM;
        ao->ao_reg = Asm_ParseRegOperand(extract1);
        ao->ao_sym = strdup(extract2);
    }
    if (extract1)
        free(extract1);
    if (extract2)
        free(extract2);
    return ao;
}

// Parses symbols / literals:
// 1. literal
// 2. symbol
Asm_OperandType *Asm_ParseBranchOperand(const char *str)
{
    char *extract = NULL;
    Asm_OperandType *ao = calloc(1, sizeof(Asm_OperandType));
    assert(ao != NULL);
    ao->ao_type = AO_INVALID;
    if (Str_RegexMatch(str, "^'(\\?.)'$", ARR_SIZE(matches), matches)) {
        extract = Str_Substr(str, matches[1].rm_so, matches[1].rm_eo);
        ao->ao_type = AO_LIT;
        ao->ao_lit = Str_UnescapeChar(extract);
    } else if (Str_RegexMatch(str, "^([-+0-9a-fA-F][xX]?[a-f0-9A-F]*)$", ARR_SIZE(matches), matches)) {
        extract = Str_Substr(str, matches[1].rm_so, matches[1].rm_eo);
        ao->ao_type = AO_LIT;
        ao->ao_lit = Str_ParseInt(extract);
    } else if (Str_RegexMatch(str, "^([_a-zA-Z][_a-zA-Z0-9]*)$", ARR_SIZE(matches), matches)) {
        extract = Str_Substr(str, matches[1].rm_so, matches[1].rm_eo);
        ao->ao_type = AO_SYM;
        ao->ao_sym = strdup(extract);
    } 
    if (extract)
        free(extract);
    return ao;
}

// Destroy parsed operand 
void Asm_OperandDestroy(Asm_OperandType *ao)
{
    if (ao) {
        ao->ao_type = AO_INVALID;
        ao->ao_reg = 0;
        ao->ao_lit = 0;
        if (ao->ao_sym) {
            free(ao->ao_sym);
            ao->ao_sym = NULL;
        }
        free(ao);
    }
}

// Asm split args
int Asm_SplitArgs(const char *str, int size, char **args)
{
    // Check empty
    int empty = 1;
    int index;
    for (index = 0; str[index]; index++) {
        if (str[index] == '#')
            return 0;
        if (! isspace(str[index])) {
            empty = 0;
            break;
        }
    }
    if (empty)
        return 0;
    enum {
        MODE_NORMAL,
        MODE_STR,
    } mode = MODE_NORMAL;
    int comment = 0;
    int count = 0;
    int flush = 0;
    int start = 0;
    for (index = 0; str[index] && ! comment; index++) {
        char here = str[index];
        char next = str[index + 1];

        switch (mode) {
            case MODE_NORMAL: {
                switch (here) {
                    case ',': {
                        flush = 1;
                    } break;
                    case '"': {
                        mode = MODE_STR;
                    } break;
                    case '#': {
                        comment = 1;
                        flush = 1;
                    } break;
                };
            } break;

            case MODE_STR: {
                switch (here) {
                    case '\\': {
                        if (next == '"') {
                            index += 1;
                        }
                    } break;
                    case '"': {
                        mode = MODE_NORMAL;
                    } break;
                }
            } break;
        }

        if (flush) {
            args[count] = Str_Substr(str, start, index);
            Str_Trim(args[count]);
            count += 1;
            start = index + 1;
            flush = 0;
        }
    }
    if (mode == MODE_NORMAL && ! comment) {
        args[count] = Str_Substr(str, start, index);
        Str_Trim(args[count]);
        count += 1;
    }
    return count;
    /*
        Test
        char *args[MAX_ASM_ARGS] = { 0 };
        int i, argc = Asm_SplitArgs("   \t 432, 43242, \"fdsafdsa, fdsa\", fds # ffdsafdsa", ARR_SIZE(args), args);
        printf("Count: %d\n", argc);
        for (i = 0; i < argc; i++) {
            printf("'%s'\n", args[i]);
        }

    */
}

// Check if string is an operator
int Asm_IsOperator(const char *str)
{
    char *ops[] = { "+" };
}

// Push directive
void Asm_PushDirective(Elf_Builder *elf, const char *direc, const char *argsLine)
{
}

// Push instruction
void Asm_PushInstruction(Elf_Builder *elf, const char *instr, const char *argsLine)
{
}

// Compile assembly into ELF
void Asm_Compile(Elf_Builder *elf, FILE *input, int flags)
{
    enum {
        MODE_OK,
        MODE_QUIT
    };
    int linenum = 0;
    int nread = 0;
    int mode = MODE_OK;
    ssize_t linelen;
    char *line = NULL;  // Whole line

    // Equ expression type
    typedef struct {
        int ei_line;
        char *ei_sym;
        char *ei_expr;
    } Asm_EquType;

    // Equ expressions
    int Asm_EquCount = 0;
    Asm_EquType Asm_EquList[MAX_ASM_EQU];

    while (mode != MODE_QUIT && (nread = getline(&line, &linelen, input)) != -1) {
        //  Read line
        linenum += 1;
        line[strlen(line) - 1] = '\0';
        char *label = NULL; // Label
        char *instr = NULL; // Instruction
        char *direc = NULL; // Directive
        char *other = NULL; // Arguments
        char *linebuf = strdup(line);

        // Extracted arguments (separated by ,)
        int argc = 0;
        char *args[MAX_ASM_ARGS];
        
        
        do {
            // Skip comments and empty lines
            if (Str_RegexMatch(linebuf, "^[ \t]*$", ARR_SIZE(matches), matches))
                continue;
            if (Str_RegexMatch(linebuf, "^[ \t]*#.*$", ARR_SIZE(matches), matches))
                continue;

            
            // Remove comments and extract label
            if (Str_RegexMatch(linebuf, "^[ \t]*([^:]*)[ \t]*:(.*)$", ARR_SIZE(matches), matches))  {
                // Extract label
                label = Str_Substr(linebuf, matches[1].rm_so, matches[1].rm_eo);
                other = Str_Substr(linebuf, matches[2].rm_so, matches[2].rm_eo);

                // Cut line
                free(linebuf);
                linebuf = other;
            }

            // Handle label
            if (label) {
                // Equivalent to `Asm_AddLabel()` with checks to whether the symbol exists already
                Elf_Sym *sym = Elf_UseSymbol(elf, label);
                if (sym->st_shndx == SHN_UNDEF)  {
                    sym->st_value = Elf_GetSectionSize(elf);
                    sym->st_shndx = Elf_GetCurrentSection(elf);
                } else if (sym->st_value != Elf_GetSectionSize(elf)) {
                   Show_Error("Line %d: Multiple symbol definitions with name '%s'", linenum, label, line);
                }
            }


            // If directive
            if (Str_RegexMatch(linebuf, "^[ \t]*(\\.[a-zA_Z0-9]+)[ \t]*(.*)[ \t]*(.*)$", ARR_SIZE(matches), matches) > 0) {
                // Extaract directive and directive arguments, remove comments
                direc = Str_Substr(linebuf, matches[1].rm_so, matches[1].rm_eo);
                other = Str_Substr(linebuf, matches[2].rm_so, matches[2].rm_eo);
                if (Str_RegexMatch(other, "(#.*)$", ARR_SIZE(matches), matches)) {
                    Str_Cut(&other, matches[1].rm_so, matches[1].rm_eo);
                }

                // Find directive
                int isOk = TRUE;
                int idx = Asm_FindDirecIdx(direc);
                if (idx == -1) {
                    Show_Error("Line %d: Invalid directive '%s':\n %s", linenum, direc, line);
                    isOk = FALSE;
                }

                // Extract directive argumetns and check validity
                if (isOk)  {
                    argc = Asm_ExtractArguments(other, args, ARR_SIZE(args));
                    // TODO: Check arguments
                }

                // Parse directives
                if (isOk) {
                    int direcId = Asm_DirecList[idx].d_direc_id;
                    switch (direcId) {
                        // .extern
                        // .global
                        case D_EXTERN:
                        case D_GLOBAL:
                        {
                            int i;
                            for (i = 0; i < argc; i++) {
                                Elf_Sym *sym = Elf_UseSymbol(elf, args[i]);
                                sym->st_info = ELF_ST_INFO(STB_GLOBAL, ELF_ST_TYPE(sym->st_info));
                            }
                        } break;

                        // .section name
                        case D_SECTION:
                        {
                            // .section name
                            if (argc == 1) {
                                Elf_UseSection(elf, args[0]);
                            } else {
                                Show_Error("Line %d: '%s' directive requires exactly 1 argument:\n %s", linenum, direc, line);
                            }
                        } break;

                        // .byte
                        // .half
                        // .word
                        case D_BYTE:
                        case D_HALF:
                        case D_WORD:
                        {
                            // .byte lit|sym, ...
                            // .half lit|sym, ...
                            // .word lit|sym, ...
                            int i;
                            for (i = 0; i < argc; i++) {
                                Asm_OperandType *ao = Asm_ParseBranchOperand(args[i]);
                                if (ao->ao_type == AO_SYM) {
                                    int relaType = 
                                          direcId == D_BYTE ? R_SS_8
                                        : direcId == D_HALF ? R_SS_16
                                        : direcId == D_WORD ? R_SS_32
                                        : R_SS_NONE;
                                    Asm_AddRela(elf, args[i], relaType, 0);
                                    Elf_PushWord(elf, 0);
                                } else if (ao->ao_type == AO_LIT) {
                                    if (direcId == D_WORD) 
                                        Elf_PushWord(elf, ao->ao_lit);
                                    else if (direcId == D_HALF) 
                                        Elf_PushHalf(elf, ao->ao_lit);
                                    else if (direcId == D_BYTE) 
                                        Elf_PushByte(elf, ao->ao_lit);
                                } else if (ao->ao_type == AO_INVALID) {
                                    Show_Error("Line %d: Invalid symbol '%s':\n %s", linenum, args[i], line);
                                }
                                Asm_OperandDestroy(ao);
                            }
                        } break;

                        // .skip num
                        case D_SKIP:
                        {
                            if (argc == 1) {
                                int value = Str_ParseInt(args[0]);
                                Elf_PushSkip(elf, value, 0x00);
                            } else {
                                Show_Error("Line %d: '%s' directive requires exactly 1 argument:\n %s", linenum, direc, line);
                            }
                        } break;

                        // .ascii str
                        case D_ASCII:
                        {
                            if (argc == 1)  {
                                Str_RmQuotes(args[0]);
                                Str_UnescapeStr(args[0]);
                                Elf_PushString(elf, args[0]);
                            } else {
                                Show_Error("Line %d: '%s' directive requires exactly 2 arguments:\n %s", linenum, direc, line);
                            }
                        } break;

                        // .equ sym, expr
                        case D_EQU:
                        {
                            if (argc == 2) {
                                Asm_EquType equ = {
                                    .ei_line = linenum,
                                    .ei_sym = strdup(args[0]),
                                    .ei_expr = strdup(args[1])
                                };
                                Asm_EquList[Asm_EquCount] = equ;
                                Asm_EquCount += 1;
                                //const char *symName = args[0];
                                //Asm_Word value = Str_ParseInt(args[1]);
                                //Asm_AddAbsSymbol(elf, symName, value);
                            } else {
                                Show_Error("Line %d: '%s' directive requires exactly 2 arguments:\n %s", linenum, direc, line);
                            }
                        } break;

                        // .end
                        case D_END:
                        {
                            mode = MODE_QUIT;
                        } break;
                    }
                }

            } else if (Str_RegexMatch(linebuf, "^[ \t]*([a-zA-Z0-9]+)[ \t]*(.*)[ \t]*$", ARR_SIZE(matches), matches)) {
                // Extract instruction, arguments and remove comments
                instr = Str_Substr(linebuf, matches[1].rm_so, matches[1].rm_eo);
                other = Str_Substr(linebuf, matches[2].rm_so, matches[2].rm_eo);
                if (Str_RegexMatch(other, "(#.*)$", ARR_SIZE(matches), matches)) {
                    Str_Cut(&other, matches[1].rm_so, matches[1].rm_eo);
                }
                argc = Asm_ExtractArguments(other, args, ARR_SIZE(args));

                
                // Find instruction index in the ISA map
                int opOK = TRUE;
                int idx = Asm_FindInstrIdx(instr);
                if (idx == -1)  {
                    Show_Error("Line %d: Invalid instruction '%s':\n %s", linenum, instr, line);
                    opOK = FALSE;
                }

                // Check if the number of arguments given are proper
                if (opOK) {
                    int argcNeeded = Asm_InstrList[idx].i_argc;
                    if (argc != argcNeeded) {
                        Show_Error("Line %d: Instruction '%s' requires exactly %d argument:\n %s", linenum, instr, argcNeeded, line);
                        opOK = FALSE;
                    }
                }

                /*
                 * Extract necessary information from the arguments 
                 * depending on the instruction arguments format
                 *   gprS - source register
                 *   gprD - destination register; also used when there is only 1 REG argument.
                 *   csr  - status/handler/cause
                 *   ao   - operand (branch / data)
                 */
                Asm_RegType gprS = NOREG;
                Asm_RegType gprD = NOREG;
                Asm_RegType csr  = NOREG;
                Asm_OperandType *ao = NULL;
                if (opOK) {
                    int argsType = Asm_InstrList[idx].i_args_type;

                    switch (argsType) {
                        default: {
                            // DO NOTHING
                        } break;

                        case I_ARGS_BOP: {
                            ao = Asm_ParseBranchOperand(args[0]);
                            if (ao->ao_type == AO_INVALID) {
                                opOK = FALSE;
                            }
                        } break;

                        case I_ARGS_REG_REG_BOP: {
                            gprS = Asm_ParseRegOperand(args[0]);
                            gprD = Asm_ParseRegOperand(args[1]);
                            ao = Asm_ParseBranchOperand(args[2]);
                            if (gprS == -1 || gprD == -1 || ao->ao_type == AO_INVALID)  {
                                opOK = FALSE;
                            }
                        } break;

                        case I_ARGS_REG_REG: {
                            gprS = Asm_ParseRegOperand(args[0]);
                            gprD = Asm_ParseRegOperand(args[1]);
                            if (gprS == -1 || gprD == -1)  {
                                opOK = FALSE;
                            }
                        } break;

                        case I_ARGS_REG: {
                            gprD = Asm_ParseRegOperand(args[0]);
                            if (gprD == -1)  {
                                opOK = FALSE;
                            }
                        } break;

                        case I_ARGS_DOP_REG: {
                            ao = Asm_ParseDataOperand(args[0]);
                            gprD = Asm_ParseRegOperand(args[1]);
                            if (gprD == -1 || ao->ao_type == AO_INVALID) {
                                opOK = FALSE;
                            }
                        } break;

                        case I_ARGS_REG_DOP: {
                            gprS = Asm_ParseRegOperand(args[0]);
                            ao = Asm_ParseDataOperand(args[1]);
                            if (gprS == -1 || ao->ao_type == AO_INVALID) {
                                opOK = FALSE;
                            }
                        } break;

                        case I_ARGS_CSR_REG: {
                            csr = Asm_ParseCsrOperand(args[0]);
                            gprD = Asm_ParseRegOperand(args[1]);
                            if (csr == -1 || gprD == -1) {
                                opOK = FALSE;
                            }
                        } break;

                        case I_ARGS_REG_CSR: {
                            gprS = Asm_ParseRegOperand(args[0]);
                            csr = Asm_ParseCsrOperand(args[1]);
                            if (csr == -1 || gprS == -1) {
                                opOK = FALSE;
                            }
                        } break;

                        case I_ARGS_DOP: {
                            ao = Asm_ParseDataOperand(args[0]);
                            if (ao->ao_type == AO_INVALID) {
                                opOK = FALSE;
                            }
                        } break;
                    }
                    if (opOK == FALSE) {
                        Show_Error("Line %d: Invalid arguments for instruction '%s':\n %s", linenum, instr, line);
                    }
                }
     
                /*
                 * Parse instructions
                 * [x] halt
                 * [x] int
                 * [x] iret
                 * [x] call
                 * [x] ret
                 * [x] jmp
                 * [x] beq
                 * [x] bne
                 * [x] bgt
                 * [x] push
                 * [x] pop
                 * [x] xchg
                 * [x] add
                 * [x] sub
                 * [x] mul
                 * [x] div
                 * [x] not
                 * [x] and 
                 * [x] or
                 * [x] xor
                 * [x] shl
                 * [x] shr
                 * [x] ld
                 * [x] st
                 * [x] csrrd
                 * [x] csrwr
                 * [x] probe
                 */
                if (opOK) {
                    int opCode = Asm_InstrList[idx].i_op_code;
                    int instrId = Asm_InstrList[idx].i_instr_id;
                    switch (instrId) {
                        // Unknown instruction
                        default: {
                            Show_Error("Line %d: Unknown instruction '%s':\n %s", linenum, instr, line);
                        } break;

                        // Halt instruction
                        case I_HALT: {
                            Asm_PushHalt(elf);
                        } break;

                        // Software interrupt
                        case I_INT: {
                            Asm_PushInt(elf);
                        } break;

                        // Return from interrupt routine
                        case I_IRET: {
                            Asm_PushIret(elf);
                        } break;

                        // jmp instruction
                        case I_JMP: {
                            switch (ao->ao_type) {
                                // jmp lit
                                case AO_LIT: {
                                    Asm_PushJmpLiteral(elf, ao->ao_lit);
                                } break;

                                // jmp sym
                                case AO_SYM: {
                                    Asm_PushJmpSymbol(elf, ao->ao_sym);
                                } break;
                            } 
                        } break;

                        // beq instruction
                        case I_BEQ: {
                            switch (ao->ao_type) {
                                // beq lit
                                case AO_LIT: {
                                    Asm_PushBeqLiteral(elf, gprS, gprD, ao->ao_lit);
                                } break;

                                // beq sym
                                case AO_SYM: {
                                    Asm_PushBeqSymbol(elf, gprS, gprD, ao->ao_sym);
                                } break;
                            } 
                        } break;

                        // bne instruction
                        case I_BNE: {
                            switch (ao->ao_type) {
                                // bne lit
                                case AO_LIT: {
                                    Asm_PushBneLiteral(elf, gprS, gprD, ao->ao_lit);
                                } break;

                                // bne sym
                                case AO_SYM: {
                                    Asm_PushBneSymbol(elf, gprS, gprD, ao->ao_sym);
                                } break;
                            } 
                        } break;

                        // bgt instruction
                        case I_BGT: {
                            switch (ao->ao_type) {
                                // bgt lit
                                case AO_LIT: {
                                    Asm_PushBgtLiteral(elf, gprS, gprD, ao->ao_lit);
                                } break;

                                // bgt sym
                                case AO_SYM: {
                                    Asm_PushBgtSymbol(elf, gprS, gprD, ao->ao_sym);
                                } break;
                            } 
                        } break;

                        // call instruction
                        case I_CALL: {
                            switch (ao->ao_type) {
                                // call lit
                                case AO_LIT: {
                                    Asm_PushCallLiteral(elf, ao->ao_lit);
                                } break;

                                // call sym
                                case AO_SYM: {
                                    Asm_PushCallSymbol(elf, ao->ao_sym);
                                } break;
                            } 
                        } break;


                        // ret
                        case I_RET: {
                            Asm_PushRet(elf);
                        } break;

                        // push %gprD
                        case I_PUSH: {
                            Asm_PushStackPush(elf, gprD);
                        } break;

                        // pop %gprD
                        case I_POP: {
                            Asm_PushStackPop(elf, gprD);
                        } break;

                        // xchg %gprS, %gprD
                        case I_XCHG: {
                            Asm_PushXchg(elf, gprS, gprD);
                        } break;

                        // not %gprD
                        case I_NOT: {
                            Asm_PushNot(elf, gprD);
                        } break;

                        // ALU instructions
                        // alu %gprS, %gprD
                        case I_ADD:
                        case I_SUB:
                        case I_MUL:
                        case I_DIV:
                        case I_AND:
                        case I_OR:
                        case I_XOR:
                        {
                            Asm_PushALU(elf, opCode, gprS, gprD);
                        } break;

                        // Load instruction
                        case I_LD: {
                            switch (ao->ao_type) {
                                // ld $lit, %gprD
                                case AO_LIT: {
                                    Asm_PushLoadLiteralValue(elf, ao->ao_lit, gprD);
                                } break;

                                // ld $sym, %gprD
                                case AO_SYM: {
                                    Asm_PushLoadSymbolValue(elf, ao->ao_sym, gprD);
                                } break;

                                // ld lit, %gprD
                                case AO_MEM_LIT: {
                                    Asm_PushLoadMemLiteralValue(elf, ao->ao_lit, gprD);
                                } break;

                                // ld sym, %gprD
                                case AO_MEM_SYM: {
                                    Asm_PushLoadMemSymbolValue(elf, ao->ao_sym, gprD);
                                } break;

                                // ld %reg, %gprD
                                case AO_REG: {
                                    Asm_PushLoadReg(elf, ao->ao_reg, gprD);
                                } break;

                                // ld [%gprS], %gprD
                                case AO_MEM_REG: {
                                    Asm_PushLoadMemRegDisp(elf, ao->ao_reg, 0, gprD);
                                } break;

                                // ld [%gprS + lit], %gprD
                                case AO_MEM_REG_LIT: {
                                    Asm_PushLoadMemRegDisp(elf, ao->ao_reg, ao->ao_lit, gprD);
                                } break;

                                // ld [%gprS + sym], %gprD
                                case AO_MEM_REG_SYM: {
                                    Asm_PushLoadMemRegSymbolDisp(elf, ao->ao_reg, ao->ao_sym, gprD);
                                } break;
                            }
                        } break;

                        // Store instruction
                        case I_ST: {
                            switch (ao->ao_type) {
                                // st %gprS, $lit
                                case AO_LIT: {
                                    Asm_PushStoreMemAddr(elf, gprS, ao->ao_lit);
                                } break;

                                // st %gprS, $sym
                                case AO_SYM: {
                                    Asm_PushStoreSymbolMemAddr(elf, gprS, ao->ao_sym);
                                } break;

                                // st %gprS, lit
                                case AO_MEM_LIT: {
                                    Asm_PushStoreAddr(elf, gprS, ao->ao_lit);
                                } break;

                                // st %gprS, sym
                                case AO_MEM_SYM: {
                                    Asm_PushStoreSymbolAddr(elf, gprS, ao->ao_sym);
                                } break;

                                // st %gprS, %gprD
                                case AO_REG: {
                                    Asm_PushStoreReg(elf, gprS, ao->ao_reg);
                                } break;

                                // st %gprS, [%gprD]
                                case AO_MEM_REG: {
                                    Asm_PushStoreMemRegDisp(elf, gprS, ao->ao_reg, 0);
                                } break;

                                // st %gprS, [%gprD + lit]
                                case AO_MEM_REG_LIT: {
                                    Asm_PushStoreMemRegDisp(elf, gprS, ao->ao_reg, ao->ao_lit);
                                } break;

                                // st %gprS, [%gprD + sym]
                                case AO_MEM_REG_SYM: {
                                    Asm_PushStoreMemRegSymbolDisp(elf, gprS, ao->ao_reg, ao->ao_sym);
                                } break;
                            }
                        } break;

                        case I_PROBE: {
                            switch (ao->ao_type) {
                                // probe $lit
                                case AO_MEM_LIT:
                                case AO_LIT: {
                                    Asm_PushProbeLit(elf, ao->ao_lit);
                                } break;

                                // probe %reg
                                case AO_REG: {
                                    Asm_PushProbeReg(elf, ao->ao_reg);
                                } break;

                                // probe [%reg]
                                case AO_MEM_REG: {
                                    Asm_PushProbeMemRegDisp(elf, ao->ao_reg, 0);
                                } break;

                                // probe [%reg + disp]
                                case AO_MEM_REG_LIT: {
                                    Asm_PushProbeMemRegDisp(elf, ao->ao_reg, ao->ao_lit);
                                } break;

                                // probe all
                                case AO_MEM_SYM: {
                                    if (Str_Equals(ao->ao_sym, "all")) {
                                        Asm_PushProbeAll(elf);
                                    } else {
                                        Show_Error("Line %d: Unknown option '%s' for instruction:\n %s", linenum, ao->ao_sym, instr, line);
                                    }
                                } break;
                            }
                        } break;

                        // csrrd %csr, %gprD
                        case I_CSRRD: {
                            Asm_PushCsrrd(elf, csr, gprD);
                        } break;

                        // csrwr %gprS, %csr
                        case I_CSRWR: {
                            Asm_PushCsrwr(elf, gprS, csr);
                        } break;
                    }
                }

                //Free memory
                if (ao) {
                    Asm_OperandDestroy(ao);
                }
            }
        } while (0);

        // Free memory
        if (label)
            free(label);
        if (direc)
            free(direc);
        if (instr)
            free(instr);
        if (other) 
            free(other);
        for (int i = 0; i < argc; i++)
            free(args[i]);
        argc = 0;
    }
    // Free memory
    if (line)
        free(line);

    // Parse Equ expressions
    int idx;
    char *matches[MAX_REGEX_MATCHES] = { 0 };
    char *rpn[MAX_REGEX_MATCHES] = { 0 }; // Reverse polish notation
    char *opStack[MAX_REGEX_MATCHES] = { 0 };
    int rpnCount = 0;
    int opStackHead = 0;
    for (idx = 0; idx < Asm_EquCount; idx++) {
        // Trim symbol and expression
        Asm_EquType ei = Asm_EquList[idx];
        int equLine = ei.ei_line;
        char *equSymName = ei.ei_sym;
        char *equExpression = ei.ei_expr;
        Str_Trim(equSymName);
        Str_Trim(equExpression);
        
        // Split expression
        int count;
        if ((count = Str_RegexSplit(equExpression, X_OPERATOR, ARR_SIZE(matches), matches)) > 0) {
            if (count == 1) {
                // Trivial expression
                Asm_OperandType *ao = Asm_ParseBranchOperand(equExpression);
                switch (ao->ao_type) {
                    // Literal value
                    case AO_LIT: {
                        Asm_Word value = ao->ao_lit;
                        Asm_AddAbsSymbol(elf, equSymName, value);
                    } break;

                    // Symbol value (symbol must be defined)
                    case AO_SYM: {
                        char *symName = equExpression;
                        if (Elf_SymbolExists(elf, symName)) {
                            Elf_Sym *sym = Elf_UseSymbol(elf, symName);
                            if (sym->st_shndx != SHN_UNDEF)  {
                                Asm_AddAbsSymbol(elf, equSymName, sym->st_value);
                            } else {
                                Show_Error(" Line %d: Symbol '%s' is undefined:\n\t.equ %s, %s", equLine, symName, equSymName, symName);
                            }
                        } else {
                            Show_Error(" Line %d: Symbol '%s' doesn't exist:\n\t.equ %s, %s", equLine, symName, equSymName, symName);
                        }
                    } break;
                }
                Asm_OperandDestroy(ao);
            } else if (count > 1) {
                // Composite expression.
                // Shunting-Yard algorithm creating reverse polish notation
                int i;
                for (i = 0; i < count; i++) {
                    char *elem = matches[i];
                    Str_Trim(elem);
                    //printf("'%s'\n", elem);
                }
            }
        }
    }
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
                Show_Error("Assembler error: -o Option requires an argument");
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
        Asm_Compile(&elf, input, F_ASM_DEBUG);

        // Testing only
        //Elf_WriteDump(&elf, stdout);

        // Write elf to output
        if (flags | F_ASM_HEX)  {
            Elf_WriteHex(&elf, output);
        } else {
            Elf_WriteDump(&elf, output);
        }

        // Destroy ELF
        Elf_Destroy(&elf);
    }

    // Close files
    if (input != stdin) 
        fclose(input);
    if (output != stdout)
        fclose(output);
    return 0;
}

