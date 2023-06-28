#ifndef _SS_TYPES_H_
#define _SS_TYPES_H_
#include "util.h"
#include <unistd.h>

// Emulator memory mapping
#define EMU_MMAP_START 0xFFFFFF00
#define EMU_MMAP_END   0xFFFFFFFF
#define EMU_TERM_OUT   0xFFFFFF00
#define EMU_TERM_IN    0xFFFFFF04
#define EMU_TIM_CFG    0xFFFFFF10

// Interrupt mask bits (status register)
#define ST_MASK_TIMER      (1 << 0)
#define ST_MASK_TERMINAL   (1 << 1)
#define ST_MASK_INTERRUPTS (1 << 2)

// Virtual memory settings and util macros
#define VA_BIT_PAGE   16
#define VA_BIT_WORD   16
#define VA_BIT_LENGTH (VA_BIT_PAGE + VA_BIT_WROD)
#define VA_PAGE_SIZE  (1 << VA_BIT_WORD)
#define VA_PAGE_COUNT (1 << VA_BIT_PAGE)

#define VA_PAGE(addr) (((addr) >> VA_BIT_PAGE) & ((1 << VA_BIT_PAGE) - 1))
#define VA_WORD(addr) ((addr) & ((1 << VA_BIT_WORD) - 1))

// Instruction width (always 0x4)
#define ASM_INSTR_WIDTH 0x4

// Limits
#define MAX_ASM_EQU       100
#define MAX_ASM_ARGS      100
#define MAX_ASM_SECTIONS  100
#define MAX_REGEX_MATCHES 100
#define BUF_DEFAULT_SIZE  10
#define MAX_LINKER_OBJECTS 128
#define MAX_REGEX_MATCHES 100
#define MAX_EQU_TOKENS 128
#define MAX_EQU_STACK  128
#define MAX_EQU_OPER_ARGS 2
#define GPR_COUNT 16
#define CSR_COUNT 3

// Regex pirmitive
#define XBEG       "^"
#define XEND       "$"
#define XLIT       "[-+]?[0-9]+|[-+]?0x[0-9a-fA-F]+"
#define XSYM       "[_a-zA-Z.][-+_.a-zA-Z0-9]*"
#define XGPR       "%r[0-9]?[0-9]|%sp|%pc"
#define XCSR       "%status|%handler|%cause"
#define XREG       XGPR "|" XCSR
#define XS         "[ \t]*"
#define CAPTURE(X) "(" X ")"
#define XANY       ".*"
#define XDOLLAR    "\\$"

// Regex composite
#define X_LIT             (XBEG XS CAPTURE(XLIT) XS XEND)
#define X_SYM             (XBEG XS CAPTURE(XSYM) XS XEND)
#define X_MEM_LIT         (XBEG XS "\\$" CAPTURE(XLIT) XS XEND)
#define X_MEM_SYM         (XBEG XS "\\$" CAPTURE(XSYM) XS XEND)
#define X_REG             (XBEG XS CAPTURE(XREG) XS XEND)
#define X_MEM_REG         (XBEG XS "\\[" XS CAPTURE(XREG) XS "\\]" XEND)
#define X_MEM_REG_LIT_ADD (XBEG XS "\\[" XS CAPTURE(XGPR) XS "+" XS CAPTURE(XLIT) XS "\\]" XS XEND)
#define X_MEM_REG_SYM_ADD (XBEG XS "\\[" XS CAPTURE(XGPR) XS "+" XS CAPTURE(XSYM) XS "\\]" XS XEND)
#define X_STR             (XBEG XS "\"" CAPTURE(".*") "\""  XS XEND)
#define X_EXP             (XBEG XS CAPTURE("[ \t]+|\\$?[_.a-zA-Z0-9]+|\\(|\\)|\\+|-|<<|>>|\\*|/|^|&|\\|") "*" XS XEND)
#define X_OPERATOR        CAPTURE("\\(|\\)|\\+|-|\\*|/|~|&|\\||\\^|<<|>>|==|<|>|<=|>=|\\|\\||&&")

// Regex for assembling
#define X_EMPTY_LINE      (XBEG XS XEND)
#define X_COMMENT_LINE    (XBEG XS "#.*" XEND)
#define X_EXTRACT_LABEL   (XBEG XS CAPTURE("[^:]*") XS ":" CAPTURE(XANY) XEND)
#define X_EXTRACT_DIREC   (XBEG XS CAPTURE("\\.[a-zA-Z0-9_]+") XS CAPTURE(XANY) XEND)
#define X_EXTRACT_INSTR   (XBEG XS CAPTURE("[a-zA-Z0-9_]+") XS CAPTURE(XANY) XEND)

// Regex for operand parsing
#define XAO_CHAR_LIT     (XBEG XDOLLAR "'" CAPTURE("\\\\?.") "'" XEND)
#define XAO_LIT          (XBEG XDOLLAR CAPTURE(XLIT) XEND)
#define XAO_SYM          (XBEG XDOLLAR CAPTURE(XSYM) XEND)
#define XAO_MEM_LIT      (XBEG CAPTURE(XLIT) XEND)
#define XAO_MEM_CHAR_LIT (XBEG "'" CAPTURE("\\\\\?.") "'" XEND)
#define XAO_MEM_SYM      (XBEG CAPTURE(XSYM) XEND)
#define XAO_REG          (XBEG CAPTURE(XGPR) XEND)
#define XAO_MEM_REG      (XBEG "\\[" XS CAPTURE(XGPR) XS "\\]" XEND)
#define XAO_MEM_REG_LIT  (XBEG "\\[" XS CAPTURE(XGPR) XS "\\+" XS CAPTURE(XLIT) XS "\\]" XEND)
#define XAO_MEM_REG_SYM  (XBEG "\\[" XS CAPTURE(XGPR) XS "\\+" XS CAPTURE(XSYM) XS "\\]" XEND)
#define XAO_STR          (XBEG "\"" CAPTURE(XANY)  "\"" XEND)

// Asm primitive types
typedef char           Asm_Sbyte;
typedef short          Asm_Shalf;
typedef int            Asm_Sword;
typedef unsigned char  Asm_Byte;
typedef unsigned short Asm_Half;
typedef unsigned int   Asm_Word;
typedef Asm_Sword      Asm_Reg;
typedef Asm_Word       Asm_Addr;

// Instruction argument types
typedef enum {
    AO_INVALID,
    AO_LIT,
    AO_SYM,
    AO_MEM_LIT,
    AO_MEM_SYM,
    AO_REG,
    AO_MEM_REG,
    AO_MEM_REG_LIT,
    AO_MEM_REG_SYM
} Asm_ArgOperandType;

typedef struct {
    int ao_type;
    int ao_reg;
    int ao_lit;
    char *ao_sym;
} Asm_OperandType;

// Instr oc types
typedef enum {
    OC_HALT   = 0x0,
    OC_INT    = 0x1,
    OC_CALL   = 0x2,
    OC_BRANCH = 0x3,
    OC_XCHG   = 0x4,
    OC_ARITH  = 0x5,
    OC_LOGIC  = 0x6,
    OC_SHIFT  = 0x7,
    OC_STORE  = 0x8,
    OC_LOAD   = 0x9,
    OC_LOCK   = 0xa,
    OC_PROBE  = 0xb,

} Asm_OcType;

// Instr mod types
typedef enum {
    MOD_NONE = 0x0,

    MOD_CALL_REG = 0x0,
    MOD_CALL_MEM = 0x1,
    MOD_CALL_RET = 0x2,
    MOD_CALL_IRET = 0x3,

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

    MOD_LOCK_LOCK = 0x0,
    MOD_LOCK_UNLOCK = 0x1,

    MOD_PROBE_LIT          = 0x0,
    MOD_PROBE_REG          = 0x1,
    MOD_PROBE_MEM_REG_DISP = 0x2,
    MOD_PROBE_ALL          = 0x3
} Asm_ModType;

// ALU types
typedef enum {
    ALU_ADD = PACK(OC_ARITH, MOD_ARITH_ADD),
    ALU_SUB = PACK(OC_ARITH, MOD_ARITH_SUB),
    ALU_MUL = PACK(OC_ARITH, MOD_ARITH_MUL),
    ALU_DIV = PACK(OC_ARITH, MOD_ARITH_DIV),
    ALU_NOT = PACK(OC_LOGIC, MOD_LOGIC_NOT),
    ALU_AND = PACK(OC_LOGIC, MOD_LOGIC_AND),
    ALU_OR  = PACK(OC_LOGIC, MOD_LOGIC_OR),
    ALU_XOR = PACK(OC_LOGIC, MOD_LOGIC_XOR),
    ALU_SHL = PACK(OC_SHIFT, MOD_SHIFT_LEFT),
    ALU_SHR = PACK(OC_SHIFT, MOD_SHIFT_RIGHT),
} Asm_AluType;

// Branch types
typedef enum {
    BRANCH_JMP      = PACK(OC_BRANCH, MOD_BRANCH_0),
    BRANCH_BEQ      = PACK(OC_BRANCH, MOD_BRANCH_1),
    BRANCH_BNE      = PACK(OC_BRANCH, MOD_BRANCH_2),
    BRANCH_BGT      = PACK(OC_BRANCH, MOD_BRANCH_3),
    BRANCH_CALL     = PACK(OC_CALL,   MOD_CALL_REG),
    BRANCH_MEM_JMP  = PACK(OC_BRANCH, MOD_BRANCH_4),
    BRANCH_MEM_BEQ  = PACK(OC_BRANCH, MOD_BRANCH_5),
    BRANCH_MEM_BNE  = PACK(OC_BRANCH, MOD_BRANCH_6),
    BRANCH_MEM_BGT  = PACK(OC_BRANCH, MOD_BRANCH_7),
    BRANCH_MEM_CALL = PACK(OC_CALL,   MOD_CALL_MEM),
} Asm_BranchType;

// Register types
typedef enum {
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
    HANDLER,
    CAUSE,

    NOREG = -1
} Asm_RegType;

// Displacement type (must be signed)
typedef int Asm_DispType;

// Instructions
typedef enum {
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
    I_CSRWR,
    I_PROBE
} Asm_InstrType;

// Instruction information type
typedef struct Asm_InstrInfo {
    char *i_name;
    int i_type;
    int i_op_code;
    int i_argc;
    int i_args_type;
    int i_instr_type;
} Asm_InstrInfo; 

// Instruction argument types
typedef enum {
    I_ARGS_NONE,
    I_ARGS_BOP,
    I_ARGS_REG_REG_BOP,
    I_ARGS_REG_REG,
    I_ARGS_REG,
    I_ARGS_DOP_REG,
    I_ARGS_REG_DOP,
    I_ARGS_CSR_REG,
    I_ARGS_REG_CSR,
    I_ARGS_DOP
} Asm_InstrArgsType;

// Directives
typedef enum {
    D_NONE,
    D_GLOBAL,
    D_EXTERN,
    D_SECTION,
    D_BYTE,
    D_HALF,
    D_WORD,
    D_SKIP,
    D_ASCII,
    D_EQU,
    D_END
} Asm_DirecType;

// Directive info
typedef struct {
    char *d_name;
    Asm_DirecType d_type;
    int d_args_type;
} Asm_DirecInfo;

// Directive arguments
typedef enum {
    D_ARGS_NONE,
    D_ARGS_SYM,
    D_ARGS_LIT,
    D_ARGS_SYM_LIST,
    D_ARGS_SYM_LIT,
    D_ARGS_STR,
    D_ARGS_SYM_EQU,
} Asm_DirecArgsType;

// Cause types
typedef enum {
    CAUSE_UNKNOWN = 0,
    CAUSE_INSTRUCTION,
    CAUSE_TIMER,
    CAUSE_TERMINAL,
    CAUSE_SOFTWARE
} Emu_CauseType;

// Interrupt types (order defines priority; INT_NONE must be last)
typedef enum {
    INT_INSTRUCTION = 0,
    INT_SOFTWARE,
    INT_TERMINAL,
    INT_TIMER,
    INT_NONE,
} Emu_IntType;

// Memory mapped register event types (read / write)
typedef enum {
    MPR_READ,
    MPR_WRITE
} MPR_Event;

// Start and end addresses
typedef enum {
    MPR_START_ADDR,
    MPR_END_ADDR
} MPR_AddrType;

// Memory map callback type
typedef void (*MPR_CallbackType)(MPR_Event event, Asm_Addr addr, Asm_Word value, Asm_Word *writeBack);

// Emulator thread types
typedef enum {
    THR_EMULATOR,
    THR_TIMER,
    THR_TERMINAL
} Emu_ThreadType;

// Emulator thread modes
typedef enum {
    MODE_START = 0,
    MODE_RUNNING,
    MODE_STOP,
} Emu_ModeType;

// Timer sleep time type (microseconds)
typedef useconds_t Emu_TimeType;

// Section placement type
typedef struct {
    char *p_name;
    Asm_Addr p_addr;
} Linker_PlaceType;

// Assembler flags
typedef enum {
    F_ASM_NONE  = 0,
    F_ASM_HEX   = 1,
    F_ASM_DEBUG = 2,
} Asm_FlagsType;

// Linker flags
typedef enum {
    F_LINKER_NONE  = 0,
    F_LINKER_HEX   = 1,
    F_LINKER_RELOC = 2,
    F_LINKER_DEBUG = 4,
} Linker_FlagType;

// Emulator flags
typedef enum {
    F_EMU_NONE     = 0,
    F_EMU_TIMER    = 1,
    F_EMU_TERMINAL = 2,
    F_EMU_DEBUG    = 4,
} Emu_FlagsType;

// Equ expression type
typedef struct {
    int ei_line;
    char *ei_sym;
    char *ei_expr;
} Asm_EquType;

// Equ element type
typedef enum {
    EE_OPERATOR,
    EE_LITERAL,
    EE_SYMBOL,
    EE_VALUE,
    EE_SYMLIT
} Equ_ElemType;

// Equ operator type
typedef enum {
    O_POS, // + a
    O_NEG, // - a
    O_ADD, // a + b
    O_SUB, // a - b
    O_MUL, // a * b
    O_DIV, // a / b
    O_MOD, // a % b
    O_NOT, // ~ a
    O_AND, // a & b
    O_OR,  // a | b
    O_XOR, // a ^ b
    O_SHL, // a << b
    O_SHR, // a >> b
    O_LBR, // (
    O_RBR  // )
} Equ_OperType;

// Equ operator associativity
typedef enum {
    O_ASOC_LTR,
    O_ASOC_RTL,
} Equ_OperAsoc;

// Equ element information
typedef struct {
    Equ_ElemType ee_type;
    union {
        Asm_Word ee_value;
        Equ_OperType ee_oper;
    };
} Equ_ElemInfo;

// Equ operator information
typedef struct {
    char        *oi_name; // symbol
    Equ_OperType oi_type; // operator
    int          oi_prec; // precedence
    int          oi_argc; // arity
    Equ_OperAsoc oi_asoc; // associativity (RTL or LTR)
} Equ_OperInfo;

// Equ token type
typedef enum {
    TOK_NONE,
    TOK_OPERATOR,
    TOK_SYMLIT
} Equ_TokenType; 

// Asm mode type
typedef enum {
    ASM_MODE_START,
    ASM_MODE_RUNNING,
    ASM_MODE_QUIT,
    ASM_MODE_DONE
} Asm_ModeType;


#endif
