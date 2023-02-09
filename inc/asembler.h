#ifndef _SS_ASEMBLER_H_
#define _SS_ASEMBLER_H_
#include "elf.h"
#define ASM_AM_IMMED    0x0
#define ASM_AM_REGDIR   0x1
#define ASM_AM_REGDIR16 0x5
#define ASM_AM_REGIND   0x2
#define ASM_AM_REGIND16 0x3
#define ASM_AM_MEMORY   0x4

#define ASM_UP_NONE    0x0
#define ASM_UP_PRESUB  0x1
#define ASM_UP_PREADD  0x2
#define ASM_UP_POSTADD 0x3
#define ASM_UP_POSTSUB 0x4

#define ASM_PC_REG 0x7
#define ASM_SP_REG 0x6




typedef struct Asm_Instr {
    union {
        struct {
            Elf_Byte ai_mod : 4;
            Elf_Byte ai_oc : 4;
        };
        Elf_Byte ai_instr;
    };
    union {
        struct {
            Elf_Byte ai_rs : 4;
            Elf_Byte ai_rd : 4;
        };
        Elf_Byte ai_regdesc;
    };
    union {
        struct {
            Elf_Byte ai_am : 4;
            Elf_Byte ai_up : 4;
        };
        Elf_Byte ai_addrmode;
    };
    Elf_Byte ai_datahi;
    Elf_Byte ai_datalo;
} Asm_Instr;

void Asm_Compile(Elf_Builder *elf, FILE *input, int flags);
//Asm_Instr Asm_ParseInstr(char *instr, char **args);




#endif /* _SS_ASEMBLER_H_ */ 
