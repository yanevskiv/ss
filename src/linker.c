#include <stdio.h>
#include <string.h>
#include <elf.h>
#include <util.h>
#include <types.h>

regmatch_t matches[MAX_REGEX_MATCHES];

int main(int argc, char *argv[])
{
    // Elf objects
    Elf_Builder objects[MAX_LINKER_OBJECTS];

    // Object count
    int oCount = 0;

    // Flags
    Linker_FlagType flags = 0;

    // Section placements
    Linker_PlaceType Linker_PlaceList[MAX_LINKER_OBJECTS];

    // Placement count
    int pCount = 0;

    // Output file 
    FILE *output = stdout;
    int i;
    for (i = 1; i < argc; i++)  {
        if (Str_Equals(argv[i], "-hex"))  {
            // -hex option
            flags |= F_LINKER_HEX;
        } else if (Str_Equals(argv[i], "-relocatable")) {
            // -relocatable option
            flags |= F_LINKER_RELOC;
        } else if (Str_RegexMatch(argv[i], "-place=([^@]*)@(0x[0-9a-fA-F]+)", ARR_SIZE(matches), matches)) {
            // -place=section@addr option
            char *s_name = Str_Substr(argv[i], matches[1].rm_so, matches[1].rm_eo);
            char *s_addr = Str_Substr(argv[i], matches[2].rm_so, matches[2].rm_eo);
            Linker_PlaceList[pCount].p_name = s_name;
            Linker_PlaceList[pCount].p_addr = Str_ParseInt(s_addr);
            pCount += 1;
            free(s_addr);
        } else if (Str_Equals(argv[i], "-o")) {
            // -o output option
            i++;
            output = NULL;
            if (i == argc)  {
                Show_Error("Linker error: Option '-o' requires an argument");
                break;
            }
            output = fopen(argv[i], "w");
            if (output == NULL) {
                Show_Error("Linker error: Failed to open file '%s' for writing", argv[i]);
                break;
            }
        } else if (*argv[i] != '-') {
            // Read object
            FILE *input = fopen(argv[i], "r");
            if (input == NULL) {
                Show_Error("Linker error: Failed to open file '%s' for reading", argv[i]);
            } else{
                // Read elf
                Elf_ReadHexInit(&objects[oCount], input);
                oCount += 1;
                fclose(input);
            }
        } else {
            Show_Warning("Linker error: Unknown option '%s'", argv[i]);
        }
    }

    if (output == NULL) {
        Show_Error("Linker error: No output set");
    } else if (oCount == 0) {
        Show_Error("Linker error: No input '.o' files provided");
    } else if ((flags & F_LINKER_RELOC) && (flags & F_LINKER_HEX)) {
        Show_Error("Linker error: Options -hex and -relocatable are mutually exclusive");
    } else {
        // Link
        Elf_Builder elf;
        Elf_Init(&elf);
        for (i = 0; i < oCount; i++) {
            // Check for duplicate simbols
            Elf_PushSection(&objects[i]);
            Elf_Shdr *shdr = Elf_UseSection(&objects[i], ".symtab");
            int j, symCount = Elf_GetSectionSize(&objects[i]) / sizeof(Elf_Sym);
            for (j = 1; j < symCount; j++) {
                const char *symName = Elf_GetSymbolName(&objects[i], j);
                Elf_Sym *sym = Elf_GetSymbol(&objects[i], j);
                if (sym->st_shndx == SHN_UNDEF || ELF_ST_TYPE(sym->st_info) == STT_SECTION)
                    continue;
                if (Elf_SymbolExists(&elf, symName)) {
                    Elf_Sym *sym = Elf_UseSymbol(&elf, symName);
                    if (sym->st_shndx != SHN_UNDEF && ELF_ST_TYPE(sym->st_info) != STT_SECTION) {
                        Show_Error("Linker error: Multiple definitions of symbol '%s'", symName);
                    }
                }
            }
            Elf_PopSection(&objects[i]);

            // Link 
            Elf_Link(&elf, &objects[i]);
        }

        // Place
        if (! (flags & F_LINKER_RELOC))  {
            // Place sections moved using -place option
            for (i = 0; i < pCount; i++) {
                char *name = Linker_PlaceList[i].p_name;
                Asm_Addr addr = Linker_PlaceList[i].p_addr;
                if (Elf_SectionExists(&elf, name))  {
                    Elf_PushSection(&elf);
                    Elf_Shdr *shdr = Elf_UseSection(&elf, name);
                    shdr->sh_addr = addr;
                    Elf_PopSection(&elf);
                }
            }

            // Place other sections linearly from addr 0x00000000
            Asm_Addr offset = 0;
            for (Elf_Section i = 1; i <= Elf_GetSectionCount(&elf); i++) {
                Elf_PushSection(&elf);
                Elf_SetSection(&elf, i);
                Elf_Shdr *shdr = Elf_GetSection(&elf);
                if (shdr->sh_addr == 0) {
                    shdr->sh_addr = offset;
                    offset += shdr->sh_size;
                }
                Elf_PopSection(&elf);
            }
        } else {
            // Place all sections on addr 0x00000000
            for (Elf_Section i = 1; i <= Elf_GetSectionCount(&elf); i++) {
                Elf_PushSection(&elf);
                Elf_SetSection(&elf, i);
                Elf_Shdr *shdr = Elf_GetSection(&elf);
                shdr->sh_addr = 0;
                Elf_PopSection(&elf);
            }
        }

        // Check whether all symbols are defined
        if (! (flags & F_LINKER_RELOC)) {
            int i, size = Elf_GetSymbolCount(&elf);
            for (i = 1; i < size; i++) {
                Elf_Sym *sym = Elf_GetSymbol(&elf, i);
                const char *sym_name = Elf_GetSymbolName(&elf, i);
                if (sym->st_shndx == SHN_UNDEF) {
                    Show_Warning("Linker: Undefined symbol '%s'", sym_name);
                }
            }
        }

        // Output
        if (flags & F_LINKER_HEX)  {
            Elf_WriteHex(&elf, output);
        } else {
            Elf_WriteDump(&elf, output);
        }
        Elf_Destroy(&elf);
    }

    // Free memory and close files
    for (i = 0; i < oCount; i++) 
        Elf_Destroy(&objects[i]);
    for (i = 0; i < pCount; i++)
        free(Linker_PlaceList[i].p_name);
    if (output != stdout)
        fclose(output);
    return 0;
}
