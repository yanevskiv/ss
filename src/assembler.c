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
#define DEBUG
#define MAX_ASM_ARGS 100
#define MAX_ASM_SECTIONS 100
#define ARR_SIZE(arr) (sizeof((arr)) / sizeof((arr)[0]))
#define BUF_DEFAULT_SIZE 10
#define MAX_REGEX_MATCHES 100

regmatch_t matches[MAX_REGEX_MATCHES];

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


enum {
    F_DEBUG = 1
};

enum {
    AO_INVALID,
    AO_LIT,
    AO_SYM,
    AO_MEM_LIT,
    AO_MEM_SYM,
    AO_REG,
    AO_MEM_REG,
    AO_MEM_REG_LIT,
    AO_MEM_REG_SYM
};

typedef struct Asm_Operand Asm_Operand;

struct Asm_Operand {
    int ao_type;
    int ao_reg;
    int ao_value;
    char *ao_name;
};

// Parses general purpose registers:
// 1. %sp (same as %r14)
// 2. %pc (same as %r15)
// 3. %r0-r15
static int Asm_ParseRegOperand(const char *str)
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
static int Asm_ParseCsrOperand(const char *str)
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
static int Asm_ExtractArguments(char *other, char **args, int max_args)
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
static struct Asm_Operand *Asm_ParseDataOperand(const char *str)
{
    char *extract1 = NULL, *extract2 = NULL;
    struct Asm_Operand *ao = calloc(1, sizeof(struct Asm_Operand));
    assert(ao != NULL);
    ao->ao_type = AO_INVALID;
    if (Str_RegexMatch(str, "^\\$([-+0-9a-fA-F][xXa-fA-F0-9]*)$", ARR_SIZE(matches), matches)) {
        extract1 = Str_Substr(str, matches[1].rm_so, matches[1].rm_eo);
        ao->ao_type = AO_LIT;
        ao->ao_value = Str_ParseInt(extract1);
    } else if (Str_RegexMatch(str, "^\\$([_a-zA-Z][_a-zA-Z0-9]*)$", ARR_SIZE(matches), matches)) {
        extract1 = Str_Substr(str, matches[1].rm_so, matches[1].rm_eo);
        ao->ao_type = AO_SYM;
        ao->ao_name = strdup(extract1);
    } else if (Str_RegexMatch(str, "^([-+0-9a-fA-F][xXa-fA-F0-9]*)$", ARR_SIZE(matches), matches)) {
        extract1 = Str_Substr(str, matches[1].rm_so, matches[1].rm_eo);
        ao->ao_type = AO_MEM_LIT;
        ao->ao_value = Str_ParseInt(extract1);
    } else if (Str_RegexMatch(str, "^([_a-zA-Z][_a-zA-Z0-9]*)$", ARR_SIZE(matches), matches)) {
        extract1 = Str_Substr(str, matches[1].rm_so, matches[1].rm_eo);
        ao->ao_type = AO_MEM_SYM;
        ao->ao_name = strdup(extract1);
    } else if (Str_RegexMatch(str, "^(%sp|%pc|%r[0-9]|%r1[0-5])$", ARR_SIZE(matches), matches)) {
        extract1 = Str_Substr(str, matches[1].rm_so, matches[1].rm_eo);
        ao->ao_type = AO_REG;
        ao->ao_reg = Asm_ParseRegOperand(extract1);
    } else if (Str_RegexMatch(str, "^\\[[ \t]*(%sp|%pc|%r[0-9]|%r1[0-5])[ \t]*\\]$", ARR_SIZE(matches), matches)) {
        extract1 = Str_Substr(str, matches[1].rm_so, matches[1].rm_eo);
        ao->ao_type = AO_MEM_REG;
        ao->ao_reg = Asm_ParseRegOperand(extract1);
        printf("%d", ao->ao_reg);
    } else if (Str_RegexMatch(str, "^\\[[ \t]*(%sp|%pc|%r[0-9]|%r1[0-5])[ \t]*\\+[ \t]*([-+0-9a-fA-F][xXa-fA-F0-9]*)\\]$", ARR_SIZE(matches), matches)) {
        extract1 = Str_Substr(str, matches[1].rm_so, matches[1].rm_eo);
        extract2 = Str_Substr(str, matches[2].rm_so, matches[2].rm_eo);
        ao->ao_type = AO_MEM_REG_LIT;
        ao->ao_reg = Asm_ParseRegOperand(extract1);
        ao->ao_value = Str_ParseInt(extract2);
    } else if (Str_RegexMatch(str, "^\\[[ \t]*(%sp|%pc|%r[0-9]|%r1[0-5])[ \t]*\\+[ \t]*([_a-zA-Z][_a-zA-Z0-9]*)\\]$", ARR_SIZE(matches), matches)) {
        extract1 = Str_Substr(str, matches[1].rm_so, matches[1].rm_eo);
        extract2 = Str_Substr(str, matches[2].rm_so, matches[2].rm_eo);
        ao->ao_type = AO_MEM_REG_SYM;
        ao->ao_reg = Asm_ParseRegOperand(extract1);
        ao->ao_name = strdup(extract2);
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
static struct Asm_Operand *Asm_ParseBranchOperand(const char *str)
{
    char *extract = NULL;
    struct Asm_Operand *ao = calloc(1, sizeof(struct Asm_Operand));
    assert(ao != NULL);
    ao->ao_type = AO_INVALID;
    if (Str_RegexMatch(str, "^([-+0-9a-fA-F][xX]?[a-f0-9A-F]*)$", ARR_SIZE(matches), matches)) {
        extract = Str_Substr(str, matches[1].rm_so, matches[1].rm_eo);
        ao->ao_type = AO_LIT;
        ao->ao_value = Str_ParseInt(extract);
    } else if (Str_RegexMatch(str, "^([_a-zA-Z][_a-zA-Z0-9]*)$", ARR_SIZE(matches), matches)) {
        extract = Str_Substr(str, matches[1].rm_so, matches[1].rm_eo);
        ao->ao_type = AO_SYM;
        ao->ao_name = strdup(extract);
    } 
    if (extract)
        free(extract);
    return ao;
}

// Destroy parsed operand 
static void Asm_OperandDestroy(struct Asm_Operand *ao)
{
    if (ao) {
        ao->ao_type = AO_INVALID;
        ao->ao_reg = 0;
        ao->ao_value = 0;
        if (ao->ao_name) {
            free(ao->ao_name);
            ao->ao_name = NULL;
        }
        free(ao);
    }
}

typedef struct Asm_InstrInfo Asm_InstrInfo;

// Instruction argument types
enum {
    IAT_NONE,
    IAT_BOP,
    IAT_REG_REG_BOP,
    IAT_REG_REG,
    IAT_REG,
    IAT_DOP_REG,
    IAT_REG_DOP,
    IAT_CSR_REG,
    IAT_REG_CSR
};

// Instructions
enum {
    I_NONE = -1,
    I_HALT = 0,
    I_INT,
    I_IRET,
    I_CALL,
    I_RET,
    I_JMP,
    I_BEQ,
    I_BNE,
    I_BGT,
    I_PUSH,
    I_POP,
    I_XCHG,
    I_ADD,
    I_SUB,
    I_MUL,
    I_DIV,
    I_NOT,
    I_AND,
    I_OR,
    I_XOR,
    I_SHL,
    I_SHR,
    I_LD,
    I_ST,
    I_CSRRD,
    I_CSRWR
};

// Instruction map
static struct Asm_InstrInfo {
    char *i_name;
    int i_instr_id;
    int i_op_code;
    int i_argc;
    int i_args_type;
    int i_instr_type;
} Asm_InstrList[] = {
    { "halt",  I_HALT,  0x00, 0, IAT_NONE },
    { "int",   I_INT,   0x10, 0, IAT_NONE },
    { "iret",  I_IRET,  0x00, 0, IAT_NONE },
    { "call",  I_CALL,  0x00, 1, IAT_BOP },
    { "ret",   I_RET,   0x00, 0, IAT_NONE },
    { "jmp",   I_JMP,   0x00, 1, IAT_BOP },
    { "beq",   I_BEQ,   0x00, 3, IAT_REG_REG_BOP },
    { "bne",   I_BNE,   0x00, 3, IAT_REG_REG_BOP },
    { "bgt",   I_BGT,   0x00, 3, IAT_REG_REG_BOP },
    { "push",  I_PUSH,  0x81, 1, IAT_REG },
    { "pop",   I_POP,   0x93, 1, IAT_REG },
    { "xchg",  I_XCHG,  0x40, 2, IAT_REG_REG },
    { "add",   I_ADD,   0x50, 2, IAT_REG_REG },
    { "sub",   I_SUB,   0x51, 2, IAT_REG_REG },
    { "mul",   I_MUL,   0x52, 2, IAT_REG_REG },
    { "div",   I_DIV,   0x53, 2, IAT_REG_REG },
    { "not",   I_NOT,   0x60, 1, IAT_REG },
    { "and",   I_AND,   0x61, 2, IAT_REG_REG },
    { "or",    I_OR,    0x62, 2, IAT_REG_REG },
    { "xor",   I_XOR,   0x63, 2, IAT_REG_REG },
    { "shl",   I_SHL,   0x70, 2, IAT_REG_REG },
    { "shr",   I_SHR,   0x71, 2, IAT_REG_REG },
    { "ld",    I_LD,    0x00, 2, IAT_DOP_REG },
    { "st",    I_ST,    0x00, 2, IAT_REG_DOP },
    { "csrrd", I_CSRRD, 0x90, 2, IAT_CSR_REG },
    { "csrwr", I_CSRWR, 0x94, 2, IAT_REG_CSR }
};

// Directive argument types
enum {
    DAT_NONE,
    DAT_SYM,
    DAT_LIT,
    DAT_SYM_LIST,
    DAT_SYM_LIT,
    DAT_STR,
    DAT_SYM_EQU,
};

// Directives
enum {
    D_NONE,
    D_GLOBAL,
    D_EXTERN,
    D_SECTION,
    D_BYTE,
    D_SHORT,
    D_WORD,
    D_SKIP,
    D_ASCII,
    D_EQU,
    D_END
};

static struct Asm_DirecInfo {
    char *d_name;
    int d_direc_id;
    int d_args_type;
} Asm_DirecList[] = {
    { ".global",  D_GLOBAL,  DAT_SYM_LIST },
    { ".globl",   D_GLOBAL,  DAT_SYM_LIST },
    { ".extern",  D_EXTERN,  DAT_SYM_LIST },
    { ".section", D_SECTION, DAT_SYM      },
    { ".byte",    D_BYTE,    DAT_SYM_LIT  },
    { ".short",   D_SHORT,   DAT_SYM_LIT  },
    { ".word",    D_WORD,    DAT_SYM_LIT  },
    { ".skip",    D_SKIP,    DAT_LIT      },
    { ".ascii",   D_ASCII,   DAT_STR      },
    { ".equ",     D_EQU,     DAT_SYM_EQU  },
    { ".end",     D_END,     DAT_NONE     },
};

// Find instruction
static int Asm_FindInstrIdx(const char *instr)
{
    int i;
    for (i = 0; i < ARR_SIZE(Asm_InstrList); i++) {
        if (Str_Equals(instr, Asm_InstrList[i].i_name))
            return i;
    }
    return -1;
}

// Find directive
static int Asm_FindDirecIdx(const char *direct)
{
    int i;
    for (i = 0; i < ARR_SIZE(Asm_DirecList); i++) {
        if (Str_Equals(direct, Asm_DirecList[i].d_name))
            return i;
    }
    return -1;
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

    while (mode != MODE_QUIT && (nread = getline(&line, &linelen, input)) != -1) {
        /*
         * Read line
         */
        linenum += 1;
        line[strlen(line) - 1] = '\0';
        char *label = NULL; // Label
        char *instr = NULL; // Instruction
        char *direc = NULL; // Directive
        char *other = NULL; // Arguments
        char *linebuf = strdup(line);
        // Arguments
        int argc = 0;
        char *args[MAX_ASM_ARGS];
        
        /*
         * Skip comments and empty lines
         */
        if (Str_RegexMatch(linebuf, "^[ \t]*$", ARR_SIZE(matches), matches))
            continue;
        if (Str_RegexMatch(linebuf, "^[ \t]*#.*$", ARR_SIZE(matches), matches))
            continue;

        /*
         * Remove comments and extract label
         */
        if (Str_RegexMatch(linebuf, "^[ \t]*([^:]*)[ \t]*:(.*)$", ARR_SIZE(matches), matches))  {
            // Extract label
            label = Str_Substr(linebuf, matches[1].rm_so, matches[1].rm_eo);
            other = Str_Substr(linebuf, matches[2].rm_so, matches[2].rm_eo);

            // Cut line
            free(linebuf);
            linebuf = other;
        }


        /* 
         * Handle label
         */
        if (label) {
            Elf_Sym *sym = Elf_UseSymbol(elf, label);
            sym->st_value = Elf_GetSectionSize(elf);
            if (sym->st_shndx == SHN_UNDEF)  {
                sym->st_shndx = Elf_GetCurrentSection(elf);
            } else {
               Show_Error("Line %d: Multiple symbol definitions with name '%s'", linenum, label, line);
            }
        }


        if (Str_RegexMatch(linebuf, "^[ \t]*(\\.[a-zA_Z0-9]+)[ \t]*(.*)[ \t]*(.*)$", ARR_SIZE(matches), matches) > 0) {
            /*
             * Extaract directive and directive arguments, remove comments
             */
            direc = Str_Substr(linebuf, matches[1].rm_so, matches[1].rm_eo);
            other = Str_Substr(linebuf, matches[2].rm_so, matches[2].rm_eo);
            if (Str_RegexMatch(other, "(#.*)$", ARR_SIZE(matches), matches)) {
                Str_Cut(&other, matches[1].rm_so, matches[1].rm_eo);
            }

            /*
             * Find directive
             */
            int isOk = TRUE;
            int idx = Asm_FindDirecIdx(direc);
            if (idx == -1) {
                Show_Error("Line %d: Invalid directive '%s':\n %s", linenum, direc, line);
                isOk = FALSE;
            }

            /*
             * Extract directive argumetns and check validity
             */
            if (isOk)  {
                argc = Asm_ExtractArguments(other, args, ARR_SIZE(args));
                // TODO: Check arguments
            }

            /*
             * Parse directives
             */
            if (isOk) {
                int direcId = Asm_DirecList[idx].d_direc_id;
                switch (direcId) {
                    case D_GLOBAL:
                    {
                        int i;
                        for (i = 0; i < argc; i++) {
                            Elf_Sym *sym = Elf_UseSymbol(elf, args[i]);
                            sym->st_info = ELF_ST_INFO(STB_GLOBAL, ELF_ST_TYPE(sym->st_info));
                        }
                    } break;

                    case D_EXTERN:
                    {
                        // Do nothing
                    } break;

                    case D_SECTION:
                    {
                        Elf_UseSection(elf, args[0]);
                    } break;

                    case D_BYTE:
                    case D_SHORT:
                    case D_WORD:
                    {
                        int i;
                        for (i = 0; i < argc; i++) {
                            Asm_Operand *ao = Asm_ParseBranchOperand(args[i]);
                            if (ao->ao_type == AO_SYM) {
                                Elf_Rela *rela = Elf_AddRelaSymb(elf, args[i]);
                                int relaType = 
                                      direcId == D_BYTE  ? R_X86_64_8
                                    : direcId == D_SHORT ? R_X86_64_16
                                    : direcId == D_WORD  ? R_X86_64_32
                                    : R_X86_64_NONE;
                                rela->r_offset = Elf_GetSectionSize(elf);
                                rela->r_info = ELF_R_INFO(ELF_R_SYM(rela->r_info), relaType);
                                rela->r_addend = 0;
                                Elf_PushWord(elf, 0);
                            } else if (ao->ao_type == AO_LIT) {
                                if (direcId == D_WORD) 
                                    Elf_PushWord(elf, ao->ao_value);
                                else if (direcId == D_SHORT) 
                                    Elf_PushHalf(elf, ao->ao_value);
                                else if (direcId == D_BYTE) 
                                    Elf_PushByte(elf, ao->ao_value);
                            } else if (ao->ao_type == AO_INVALID) {
                                Show_Error("Line %d: Invalid symbol '%s':\n %s", linenum, args[i], line);
                            }
                            Asm_OperandDestroy(ao);
                        }
                    } break;

                    case D_SKIP:
                    {
                        int value = Str_ParseInt(args[0]);
                        Elf_PushSkip(elf, value, 0x00);
                    } break;

                    case D_ASCII:
                    {
                        Str_RmQuotes(args[0]);
                        Elf_PushString(elf, args[0]);
                    } break;

                    case D_EQU:
                    {
                        // Not Implemented
                    } break;

                    case D_END:
                    {
                        mode = MODE_QUIT;
                    } break;
                }
            }

        } else if (Str_RegexMatch(linebuf, "^[ \t]*([a-zA-Z0-9]+)[ \t]*(.*)[ \t]*$", ARR_SIZE(matches), matches)) {
            /*
             * Extract instruction, arguments and remove comments
             */
            instr = Str_Substr(linebuf, matches[1].rm_so, matches[1].rm_eo);
            other = Str_Substr(linebuf, matches[2].rm_so, matches[2].rm_eo);
            if (Str_RegexMatch(other, "(#.*)$", ARR_SIZE(matches), matches)) {
                Str_Cut(&other, matches[1].rm_so, matches[1].rm_eo);
            }
            argc = Asm_ExtractArguments(other, args, ARR_SIZE(args));

            /*
             * Find instruction index in the ISA map
             */
            int opOK = TRUE;
            int idx = Asm_FindInstrIdx(instr);
            if (idx == -1)  {
                Show_Error("Line %d: Invalid instruction '%s':\n %s", linenum, instr, line);
                opOK = FALSE;
            }

            /* 
             * Check if the number of arguments given are proper
             */
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
            int gprS = -1;
            int gprD = -1;
            int csr = -1;
            Asm_Operand *ao = NULL;
            if (opOK) {
                int argsType = Asm_InstrList[idx].i_args_type;

                switch (argsType) {
                    default: {
                        // DO NOTHING
                    } break;

                    case IAT_BOP: {
                        ao = Asm_ParseBranchOperand(args[0]);
                        if (ao->ao_type == AO_INVALID) {
                            opOK = FALSE;
                        }
                    } break;

                    case IAT_REG_REG_BOP: {
                        gprS = Asm_ParseRegOperand(args[0]);
                        gprD = Asm_ParseRegOperand(args[1]);
                        ao = Asm_ParseBranchOperand(args[2]);
                        if (gprS == -1 || gprD == -1 || ao->ao_type == AO_INVALID)  {
                            opOK = FALSE;
                        }
                    } break;

                    case IAT_REG_REG: {
                        gprS = Asm_ParseRegOperand(args[0]);
                        gprD = Asm_ParseRegOperand(args[1]);
                        if (gprS == -1  || gprD == -1)  {
                            opOK = FALSE;
                        }
                    } break;

                    case IAT_REG: {
                        gprD = Asm_ParseRegOperand(args[0]);
                        if (gprD == -1)  {
                            opOK = FALSE;
                        }
                    } break;

                    case IAT_DOP_REG: {
                        ao = Asm_ParseDataOperand(args[0]);
                        gprD = Asm_ParseRegOperand(args[1]);
                        if (gprD == -1 || ao->ao_type == AO_INVALID) {
                            opOK = FALSE;
                        }
                    } break;

                    case IAT_REG_DOP: {
                        gprD = Asm_ParseRegOperand(args[0]);
                        ao = Asm_ParseDataOperand(args[1]);
                        if (gprD == -1 || ao->ao_type == AO_INVALID) {
                            opOK = FALSE;
                        }
                    } break;

                    case IAT_CSR_REG: {
                        csr = Asm_ParseCsrOperand(args[0]);
                        gprD = Asm_ParseRegOperand(args[1]);
                        if (csr == -1 || gprD == -1) {
                            opOK = FALSE;
                        }
                    } break;

                    case IAT_REG_CSR: {
                        gprD = Asm_ParseRegOperand(args[0]);
                        csr = Asm_ParseCsrOperand(args[1]);
                        if (csr == -1 || gprD == -1) {
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
             * [ ] iret
             * [ ] call
             * [ ] ret
             * [ ] jmp
             * [ ] beq
             * [ ] bne
             * [ ] bgt
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
             * [ ] ld
             * [ ] st
             * [x] csrrd
             * [x] csrwr
             */
            if (opOK) {
                int opCode = Asm_InstrList[idx].i_op_code;
                int instrId = Asm_InstrList[idx].i_instr_id;
                switch (instrId) {
                    default: {
                        Show_Error("Line %d: Unknown instruction '%s':\n %s", linenum, instr, line);
                    } break;

                    /*
                     * HALT and INT instructions
                     * opcodes:
                     *  halt: 0x00
                     *  int:  0x10
                     *
                     *           byte1      byte2       byte3      byte4
                     * Format: [ opcode | 0000 0000 | 0000 0000 | 0000 0000 ]
                     */
                    case I_HALT: 
                    case I_INT:
                    {
                        Elf_PushByte(elf, opCode);
                        Elf_PushByte(elf, 0x00);
                        Elf_PushByte(elf, 0x00);
                        Elf_PushByte(elf, 0x00);
                    } break;

                    case I_CALL: {
                        // TODO
                    } break;

                    case I_RET: {
                        // TODO
                    } break;



                    /*
                     * XCHG instruction
                     * opcodes:
                     *  xchg: 0x40
                     * 
                     *           byte1      byte2       byte3      byte4
                     * Format: [ opcode | 0000 0000 | 0000 0000 | 0000 0000 ]
                     */
                    case I_XCHG:
                    {
                        Elf_PushByte(elf, opCode);
                        Elf_PushByte(elf, (gprS << 0));
                        Elf_PushByte(elf, (gprD << 4));
                        Elf_PushByte(elf, 0x00);
                    } break;

                    /*
                     * ALU instructions
                     * opcodes:
                     *  add: 0x50
                     *  sub: 0x51
                     *  mul: 0x52
                     *  div: 0x53
                     *  and: 0x61
                     *  or:  0x62
                     *  xor: 0x63
                     *  shl: 0x70
                     *  shr: 0x71
                     *
                     *            byte1      byte2       byte3       byte4
                     * Format:  [ opcode | AAAA BBBB | CCCC 0000 | 0000 0000 ]
                     * Meaning: gpr[A] <= gpr[B] `OP` gpr[C]
                     */
                    // Format:
                    case I_ADD:
                    case I_SUB:
                    case I_MUL:
                    case I_DIV:
                    case I_AND: 
                    case I_OR: 
                    case I_XOR: 
                    case I_SHL: 
                    case I_SHR: 
                    {
                        Elf_PushByte(elf, opCode);
                        Elf_PushByte(elf, (gprS << 4) | (gprS << 0));
                        Elf_PushByte(elf, (gprD << 4));
                        Elf_PushByte(elf, 0x00);
                    } break;

                    /*
                     * NOT instruction
                     * opcodes:
                     *  not: 0x60
                     *
                     *            byte1      byte2       byte3       byte4
                     * Format:  [ opcode | AAAA BBBB | CCCC 0000 | 0000 0000 ]
                     * Meaning: gpr[A] <= gpr[B] `OP` gpr[C]
                     * C is irrelevant here
                     */
                    case I_NOT:
                    {
                        Elf_PushByte(elf, opCode);
                        Elf_PushByte(elf, (gprD << 4) | (gprD << 0));
                        Elf_PushByte(elf, 0x00);
                        Elf_PushByte(elf, 0x00);
                    } break;


                    case I_PUSH: {
                        Elf_PushByte(elf, opCode);
                        Elf_PushByte(elf, (ASM_SP_REG << 4));
                        Elf_PushByte(elf, gprD);
                        Elf_PushByte(elf, 0x04);
                    } break;

                    case I_POP: {
                        Elf_PushByte(elf, opCode);
                        Elf_PushByte(elf, gprD << 4 | ASM_SP_REG);
                        Elf_PushByte(elf, 0x0f);
                        Elf_PushByte(elf, 0xfc);
                    } break;

                    case I_LD: {
                        // TODO
                    } break;

                    case I_ST: {
                        // TODO
                    } break;


                    /*
                     * CSRWR and CSRRD instructions
                     * opcodes:
                     *  csrrd: 0x90
                     *  csrwr: 0x94
                     *
                     *            byte1      byte2       byte3       byte4
                     * Format:  [ opcode | AAAA BBBB | CCCC DDDD | DDDD DDDD ]
                     * Meaning:
                     *   gpr[A] <= csr[B] if opcode = 0x90
                     *   csr[A] <= gpr[B] if opcode = 0x94
                     * C and D are irrelevant here
                     */
                    case I_CSRWR: 
                    case I_CSRRD: {
                        Elf_PushByte(elf, opCode);
                        Elf_PushByte(elf, (gprD << 4) | (csr << 0));
                        Elf_PushByte(elf, 0x00);
                        Elf_PushByte(elf, 0x00);
                    } break;
                }
            }

            /*
             * Free memory
             */
            if (ao) {
                Asm_OperandDestroy(ao);
            }
        }

        /*
         * Free memory
         */
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
    /*
     * Free memory
     */
    if (line)
        free(line);
}

int main(int argc, char *argv[])
{
    int i;
    FILE *input = NULL;
    FILE *output = NULL;

    /* 
     * Set input and output
     */
    enum {
        F_HEX = 1
    };
    int flags = 0;
    for (i = 1; i < argc; i++) {
        char *option = argv[i];
        if (Str_Equals(option, "-hex")) {
            flags |= F_HEX;
        } else if (Str_Equals(option, "-o")) {
            // Output option (-o)
            if (i + 1 == argc) {
                fprintf(stderr, "Error: -o Option requires an argument\n");
                exit(EXIT_FAILURE);
            }
            char *output_path = argv[i + 1];
            i += 1;
            if (Str_Equals(output_path, "-")) {
                output = stdout;
            } else {
                output = fopen(output_path, "w");
                if (output == NULL) {
                    fprintf(stderr, "Error: Failed to open \"%s\" for writing\n", output_path);
                    exit(EXIT_FAILURE);
                }
            }
        } else {
            // Input option (no argument)
            if (input) {
                fprintf(stderr, "Warning: Multiple input files set\n");
            } else {
                char *input_path = option;
                if (Str_Equals(input_path, "-")) {
                    input = stdin;
                } else {
                    input = fopen(input_path, "r");
                    if (input == NULL) {
                        fprintf(stderr, "Error: Failed to open file \"%s\" for reading\n", input_path);
                        exit(EXIT_FAILURE);
                    }
                }
            }
        }
    }

    /*
     * Check input and output
     */
    if (input == NULL || output == NULL) {
        show_help();
    } else {
        Elf_Builder elf;
        Elf_Init(&elf);
        Asm_Compile(&elf, input, F_DEBUG);

        // Testing only
        //Elf_WriteDump(&elf, stdout);

        if (flags | F_HEX)  {
            Elf_WriteHex(&elf, output);
        } else {
            Elf_WriteDump(&elf, output);
        }
        Elf_Destroy(&elf);
    }

    /* Close files */
    if (input != stdin) 
        fclose(input);
    if (output != stdout)
        fclose(output);
    return 0;
}

