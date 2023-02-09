#ifndef _SS_ASEMBLER_H_
#define _SS_ASEMBLER_H_
#include "elf.h"



typedef struct Asm_Instr {
    union {
        struct {
            Elf_Byte ai_oc : 4;
            Elf_Byte ai_mod : 4;
        };
        Elf_Byte ai_instr;
    };
    union {
        struct {
            Elf_Byte ai_rd : 4;
            Elf_Byte ai_rs : 4;
        };
        Elf_Byte ai_regdesc;
    };
    union {
        struct {
            Elf_Byte ai_up : 4;
            Elf_Byte ai_am : 4;
        };
        Elf_Byte ai_addrmode;
    };
    Elf_Byte ai_datahi;
    Elf_Byte ai_datalo;
} Asm_Instr;

void Asm_Compile(Elf_Builder *elf, FILE *input, int flags);
Asm_Instr Asm_ParseInstr(const char *instr, const char **args);




#endif /* _SS_ASEMBLER_H_ */ 
