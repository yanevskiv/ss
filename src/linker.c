#include <stdio.h>
#include <string.h>
#include <elf.h>
#define LD_MAX_OBJECTS 128

int main(int argc, char *argv[])
{
    Elf_Builder objects[LD_MAX_OBJECTS];
    int count = 0;
    int flags = 0;
    enum {
        F_HEX = 1,
        F_RELOCATABLE = 2,
    };
    FILE *output = stdout;
    int i;
    for (i = 1; i < argc; i++)  {
        if (strcmp(argv[i], "-hex") == 0)  {
            // Hex option
            flags |= F_HEX;
        } else if (strcmp(argv[i], "-relocatable") == 0) {
            // Reloc option
            flags |= F_RELOCATABLE;
        } else if (strcmp(argv[i], "-o") == 0) {
            // Set output
            i++;
            output = NULL;
            if (i == argc)  {
                fprintf(stderr, "Linker error: Option '-o' requires an argument\n");
                return 1;
            }
            output = fopen(argv[i], "w");
            if (output == NULL) {
                fprintf(stderr, "Linker error: Failed to open file '%s' for writing\n", argv[i]);
                return 1;
            }
            // 
        } else if (*argv[i] != '-') {
            // Read object
            FILE *input = fopen(argv[i], "r");
            if (input == NULL) {
                fprintf(stderr, "Linker error: Failed to open file '%s' for reading\n", argv[i]);
            } else{
                // Read elf
                Elf_ReadHexInit(&objects[count], input);
                count += 1;
                fclose(input);
            }
        }
    }
    if (output == NULL) {
        fprintf(stderr, "Linker error: No output set\n");
        return 0;
    }
    if (count == 0) {
        fprintf(stderr, "Linker error: No input '.o' files provided\n");
        return 0;
    }

    Elf_Builder elf;
    Elf_Init(&elf);
    for (i = 0; i < count; i++) {
        Elf_Link(&elf, &objects[i]);
    }

    // For testing
    //Elf_WriteDump(&elf, stdout);
    if (flags & F_HEX)  {
        Elf_WriteHex(&objects[0], output);
    } else {
        Elf_WriteDump(&objects[0], output);
    }
    if (output != stdout)
        fclose(output);
    return 0;
}
