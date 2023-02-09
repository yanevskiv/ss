#include <stdlib.h>
#include <emulator.h>
#include <assert.h>
#include <elf.h>

void Emu_Init(Emu *emu)
{
    emu->e_mem = calloc(0xffff + 1, sizeof(unsigned char));
    assert(emu->e_mem != NULL);
    Emu_Reset(emu);
}


void Emu_Run(Emu *emu)
{
    Emu_Dump(emu);
}

void Emu_Dump(Emu *emu) {

}
void Emu_Reset(Emu *emu)
{

}


int main() {

}
