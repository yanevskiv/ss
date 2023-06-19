#include <stdio.h>
#include <string.h>
#include <elf.h>
#include <util.h>
#define LD_MAX_OBJECTS 128
#define MAX_REGEX_MATCHES 100

#define Asm_Word unsigned int
#define Asm_Addr Asm_Word

int main(int argc, char *argv[])
{
    Elf_Builder objects[LD_MAX_OBJECTS];
    int oCount = 0;
    int flags = 0;
    enum {
        F_HEX = 1,
        F_RELOCATABLE = 2,
    };
    struct {
        char *p_name;
        Asm_Addr p_addr;
    } places[LD_MAX_OBJECTS];
    int pCount = 0;


    FILE *output = stdout;
    int i;
    regmatch_t matches[MAX_REGEX_MATCHES];
    for (i = 1; i < argc; i++)  {
        if (Str_Equals(argv[i], "-hex"))  {
            // Hex option
            flags |= F_HEX;
        } else if (Str_Equals(argv[i], "-relocatable")) {
            // Reloc option
            flags |= F_RELOCATABLE;
        } else if (Str_RegexMatch(argv[i], "-place=([^@]*)@(0x[0-9a-fA-F]+)", ARR_SIZE(matches), matches)) {
            char *s_name = Str_Substr(argv[i], matches[1].rm_so, matches[1].rm_eo);
            char *s_addr = Str_Substr(argv[i], matches[2].rm_so, matches[2].rm_eo);
            places[pCount].p_name = s_name;
            places[pCount].p_addr = Str_ParseInt(s_addr);
            free(s_addr);
        } else if (Str_Equals(argv[i], "-o")) {
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
                Elf_ReadHexInit(&objects[oCount], input);
                oCount += 1;
                fclose(input);
            }
        } else {
            fprintf(stderr, "Linker error: Unknown option '%s'\n", argv[i]);
        }
    }
    if (output == NULL) {
        fprintf(stderr, "Linker error: No output set\n");
        return 0;
    } else if (oCount == 0) {
        fprintf(stderr, "Linker error: No input '.o' files provided\n");
        return 0;
    } else {
        // Link
        //Elf_Builder elf;
        //Elf_Init(&elf);
        //for (i = 0; i < oCount; i++) {
        //    Elf_Link(&elf, &objects[i]);
        //}

        //// For testing
        ////Elf_WriteDump(&elf, stdout);

        //if (flags & F_HEX)  {
        //    Elf_WriteHex(&objects[0], output);
        //} else {
        //    Elf_WriteDump(&objects[0], output);
        //}
        //Elf_Destroy(&elf);
    }

    for (i = 0; i < oCount; i++) 
        Elf_Destroy(&objects[i]);
    for (i = 0; i < pCount; i++) 
        free(places[i].p_name);
    if (output != stdout)
        fclose(output);
    return 0;
}
