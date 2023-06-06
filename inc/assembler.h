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
            Elf_Byte ai_oc : 4;
            Elf_Byte ai_mod : 4;
        };
        Elf_Byte ai_instr;
    };
    struct {
        Elf_Byte ai_ra : 4;
        Elf_Byte ai_rb : 4;
        Elf_Byte ai_rc : 4;
    };
    struct {
        Elf_Byte ai_disp1: 4;
        Elf_Byte ai_disp2: 4;
        Elf_Byte ai_disp3: 4;
    };
} Asm_Instr;


#define ASM_INSTR_HALT       0x00000000
#define ASM_INSTR_INT        0x00000001
#define ASM_INSTR_ALU 



void Asm_Compile(Elf_Builder *elf, FILE *input, int flags);
//Asm_Instr Asm_ParseInstr(char *instr, char **args);

#endif /* _SS_ASEMBLER_H_ */ 
