#ifndef _EMULATOR_H_
#define _EMULATOR_H_
#include <elf.h>
#define EMU_REG_NUM 8
#define EMU_PC_REG 0x7
#define EMU_SP_REG 0x6
/* Stack points to occupied location and goes down */

typedef struct Emu {
    unsigned char *e_mem;
    Elf_Half e_reg[EMU_REG_NUM];
    union {
        struct {
            Elf_Half e_psw_z : 1; // zero flag
            Elf_Half e_psw_o : 1; // overflow flag
            Elf_Half e_psw_c : 1; // carry flag
            Elf_Half e_psw_n : 1; // negative flag
            Elf_Half e_psw_ignore : 9; // (unused)
            Elf_Half e_psw_tr : 1; // timer (0 - working 1 - masked)
            Elf_Half e_psw_tl : 1; // terminal (0 - working 1 - masked)
            Elf_Half e_psw_i : 1; // disable interrupts (0 - interrupts working, 1 - interrupts masked)
        };
        Elf_Half e_psw;
    };
} Emu;


void Emu_Init(Emu *emu);
void Emu_LoadElf(Emu *emu, Elf_Builder *elf);
void Emu_Reset(Emu *emu);
void Emu_Run(Emu *emu);
void Emu_Dump(Emu *emu);

#endif /* _EMULATOR_H_ */ 
