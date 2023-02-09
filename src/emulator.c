#include <stdlib.h>
#include <emulator.h>
#include <assert.h>
#include <elf.h>
#include <string.h>

void Emu_Init(Emu *emu)
{
    emu->e_mem = calloc(0xffff + 1, sizeof(unsigned char));
    assert(emu->e_mem != NULL);
    Emu_Reset(emu);
}

void Emu_Reset(Emu *emu)
{
    int i;
    bzero(emu->e_reg, EMU_REG_NUM * sizeof(Elf_Half));
    bzero(emu->e_mem, EMU_MEM_SIZE * sizeof(unsigned char));
    emu->e_psw = 0;
}


void Emu_Run(Emu *emu)
{
}

void Emu_Dump(Emu *emu) {

}

void Emu_LoadElf(Emu *emu, Elf_Builder *elf)
{
    Elf_WriteDump(elf, stdout);
}

void Emu_Destroy(Emu *emu)
{

}

static void show_help(FILE* file) 
{
    const char *help_text = 
        "Usage: emulator [HEX FILE]\n"
    ;
    fprintf(file, "%s", help_text);

}

int main(int argc, char *argv[]) {
    if (argc != 2)  {
        show_help(stdout);
        return 0;
    }

    // Read elf
    FILE *input = NULL;
    const char *input_name = argv[1];
    Elf_Builder elf;
    if (strcmp(input_name, "-") == 0) {
        input = stdin;
    } else {
        input = fopen(input_name, "r");
        if (input == NULL)  {
            fprintf(stderr, "Error (Emulator): Failed to open file '%s' for reading\n", input_name);
            return 1;
        }
    }
    Elf_ReadHexInit(&elf, input);


    // Start emulator
    Emu emu;
    Emu_Init(&emu);
    Emu_LoadElf(&emu, &elf);
    Emu_Run(&emu);
    Emu_Dump(&emu);
    Emu_Destroy(&emu);


    return 0;
}
