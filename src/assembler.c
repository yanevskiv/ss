#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <assert.h>
#include <regex.h>
#include <ctype.h>
#include <assembler.h>
#include <elf.h>
#include <stdarg.h>
#include <util.h>

// Show assembly error
#define Show_AsmError(...)  \
    do { \
        if (Asm_LineNo > 0) { \
            fprintf(stderr, "\e[31mError\e[0m: Line %d: ", Asm_LineNo); \
        } else { \
            fprintf(stderr, "\e[31mError\e[0m: "); \
        } \
        fprintf(stderr, __VA_ARGS__); \
        fprintf(stderr, ":"); \
        if (Asm_Line) { \
            fprintf(stderr, "\n%s", Asm_Line); \
        } \
        fprintf(stderr, "\n"); \
        fflush(stderr); \
    } while (0)

// Show equ error
#define Show_EquError(...) \
    do { \
        fprintf(stderr, "\e[31mError\e[0m: Equ error: "); \
        fprintf(stderr, __VA_ARGS__); \
        fprintf(stderr, "\n"); \
        fflush(stderr); \
    } while (0)

// Equ count
int Asm_EquCount = 0;

// Equ list 
Asm_EquType Asm_EquList[MAX_ASM_EQU];

// Current line number
int Asm_LineNo = 0;

// Current line being parsed
char *Asm_Line = NULL;

// Current line capacity
size_t Asm_LineCap = 0;

// Current asm mode
Asm_ModeType Asm_CurrentMode = ASM_MODE_START;

static void show_help() 
{
    const char *help = 
        "Usage: asembler [OPTIONS] <INPUT_FILE> \n"
        "\n"
        "Available options:\n"
        "  -o OUTPUT_FILE - Set output file\n"
        "  -h             - Show help and exist\n"
        "  -v             - Show version and exit\n"
    ;
    printf("%s\n", help);
    exit(EXIT_SUCCESS);
}


int main(int argc, char *argv[])
{

    // Input file
    FILE *input = NULL;

    // Output file
    FILE *output = NULL;

    // Flags
    Asm_FlagsType flags = 0;

    // Parse options
    int i;
    for (i = 1; i < argc; i++) {
        char *option = argv[i];
        if (Str_Equals(option, "-hex")) {
            // -hex option
            flags |= F_ASM_HEX;
        } else if (Str_Equals(option, "-o")) {
            // -o output option
            if (i + 1 == argc) {
                Show_Error("-o Option requires an argument");
                break;
            }
            char *output_path = argv[i + 1];
            i += 1;
            if (Str_Equals(output_path, "-")) {
                output = stdout;
            } else {
                output = fopen(output_path, "w");
                if (output == NULL) {
                    Show_Error("Failed to open '%s' for writing", output_path);
                    break;
                }
            }
        } else {
            // Input option (no argument)
            if (input) {
                Show_Warning("Multiple input files set. Only the first will be used.");
            } else {
                char *input_path = option;
                if (Str_Equals(input_path, "-")) {
                    input = stdin;
                } else {
                    input = fopen(input_path, "r");
                    if (input == NULL) {
                        Show_Error("Failed to open file '%s' for reading", input_path);
                        break;
                    }
                }
            }
        }
    }

    // Check validity
    if (input == NULL || output == NULL) {
        show_help();
    } else if (input == NULL) {
        Show_Error("No input");
    } else if (output == NULL) {
        Show_Error("No output");
    } else {
        // Create ELF
        Elf_Builder elf;
        Elf_Init(&elf);

        // Compile input
        Asm_Compile(&elf, input, F_ASM_NONE);

        // Testing only
        //Elf_WriteDump(&elf, stdout);

        // Write elf to output
        if (flags & F_ASM_HEX)  {
            Elf_WriteHex(&elf, output);
        } else {
            Elf_WriteDump(&elf, output);
        }

        // Destroy ELF
        Elf_Destroy(&elf);
    }

    // Close files
    if (input && input != stdin) 
        fclose(input);
    if (output && output != stdout)
        fclose(output);
    return 0;
}

