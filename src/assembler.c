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
#define DEBUG
#define MAX_ASM_ARGS 100
#define MAX_ASM_SECTIONS 100
#define ARR_SIZE(arr) (sizeof((arr)) / sizeof((arr)[0]))
#define BUF_DEFAULT_SIZE 10
#define MAX_REGEX_MATCHES 100


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


enum {
    F_DEBUG = 1
};

static int Asm_ParseRegOperand(const char *str)
{
    regmatch_t matches[MAX_REGEX_MATCHES];
    if (Str_Equals(str, "^%sp$")) {
        return 14;
    } else if (Str_Equals(str, "^%pc$")) {
        return 15;
    } else if (Str_CheckMatch(str, "^%r([0-9]+)$"))  {
        int reg = strtol(str + 1, NULL, 10);
        if (reg > 15)  {
            return -1;
        }
        return reg;
    } else {
        return -1;
    }
}

static void Asm_PushALU(Elf_Builder *elf, int instr, int argc, char **argv) {
    Elf_PushByte(elf, instr);
}


void Asm_Compile(Elf_Builder *elf, FILE *input, int flags)
{
    enum {
        MODE_OK,
        MODE_QUIT
    };
    int linenum = 0;
    int nread = 0;
    int mode = MODE_OK;
    ssize_t linelen;

    char *line = NULL;  // Whole line
    char *currentSection = NULL;


    regmatch_t matches[MAX_REGEX_MATCHES];
    
    while (mode != MODE_QUIT && (nread = getline(&line, &linelen, input)) != -1) {
        /*
         * Read line
         */
        linenum += 1;
        line[strlen(line) - 1] = '\0';
        char *label = NULL; // Label
        char *instr = NULL; // Instruction
        char *direc = NULL; // Directive
        char *other = NULL; // The rest 
        char *linebuf = strdup(line);
        // Arguments
        int argc = 0;
        char *args[MAX_ASM_ARGS];
        
        // For string matches
        //char *matches[MAX_REGEX_MATCHES];
        
        /*
         * Skip comments and empty lines
         */
        if (Str_RegexMatch(linebuf, "^[ \t]*$", ARR_SIZE(matches), matches))
            continue;
        if (Str_RegexMatch(linebuf, "^[ \t]*#.*$", ARR_SIZE(matches), matches))
            continue;
        /*
         * Remove comments and extract label
         */
        if (Str_RegexMatch(linebuf, "^[ \t]*([^:]*)[ \t]*:(.*)$", ARR_SIZE(matches), matches))  {
            label = Str_Substr(linebuf, matches[1].rm_so, matches[1].rm_eo);
            other = Str_Substr(linebuf, matches[2].rm_so, matches[2].rm_eo);

            // Cut line
            free(linebuf);
            linebuf = other;
        }

        if (Str_RegexMatch(linebuf, "^[ \t]*(\\.[a-zA_Z0-9]+)[ \t]*(.*)[ \t]*", ARR_SIZE(matches), matches) > 0) {
            direc = Str_Substr(linebuf, matches[1].rm_so, matches[1].rm_eo);
            other = Str_Substr(linebuf, matches[2].rm_so, matches[2].rm_eo);

            if (Str_Equals(direc, ".global")) {
            } else if (Str_Equals(direc, ".extern")) {
            } else if (Str_Equals(direc, ".section")) {
                char *name = other;
                Elf_UseSection(elf, name);
            } else if (Str_Equals(direc, ".word")) {
            } else if (Str_Equals(direc, ".skip")) {
            } else if (Str_Equals(direc, ".ascii")) {
            } else if (Str_Equals(direc, ".equ")) {
            } else if (Str_Equals(direc, ".end")) {
            }
        } else if (Str_RegexMatch(linebuf, "^[ \t]*([a-zA-Z0-9]+)[ \t]*(.*)[ \t]*$", ARR_SIZE(matches), matches)) {
            /*
             * Extract instruction, arguments and remove comments
             */
            instr = Str_Substr(linebuf, matches[1].rm_so, matches[1].rm_eo);
            other = Str_Substr(linebuf, matches[2].rm_so, matches[2].rm_eo);
            if (Str_RegexMatch(other, "(#.*)$", ARR_SIZE(matches), matches)) {
                Str_Cut(&other, matches[1].rm_so, matches[1].rm_eo);
            }
            char *token = strtok(other, ",");
            while (token) {
                args[argc] = strdup(token);
                Str_Trim(args[argc]);
                argc += 1;
                token = strtok(NULL, ",");
            }
            for (int i = 0; i < argc; i++) {
                printf("'%s' \n", args[i]);
            }

            /*
             * Extract
             */

            /*
             * Parse instructions
             */
            if (Str_Equals(instr, "halt")) {
                Elf_PushWord(elf, ASM_INSTR_HALT);
            } else if (Str_Equals(instr, "int")) {
                Elf_PushWord(elf, ASM_INSTR_INT);
            } else if (Str_Equals(instr, "iret")) {
            } else if (Str_Equals(instr, "call")) {
            } else if (Str_Equals(instr, "ret")) {
            } else if (Str_Equals(instr, "jmp")) {
            } else if (Str_Equals(instr, "beq")) {
            } else if (Str_Equals(instr, "bne")) {
            } else if (Str_Equals(instr, "bgt")) {
            } else if (Str_Equals(instr, "push")) {
            } else if (Str_Equals(instr, "pop")) {
            } else if (Str_Equals(instr, "xchg")) {
            } else if (Str_Equals(instr, "add")) {
            } else if (Str_Equals(instr, "sub")) {
            } else if (Str_Equals(instr, "mul")) {
            } else if (Str_Equals(instr, "div")) {
            } else if (Str_Equals(instr, "not")) {
            } else if (Str_Equals(instr, "and")) {
            } else if (Str_Equals(instr, "or")) {
            } else if (Str_Equals(instr, "xor")) {
            } else if (Str_Equals(instr, "shl")) {
            } else if (Str_Equals(instr, "shr")) {
            } else if (Str_Equals(instr, "ld")) {
            } else if (Str_Equals(instr, "st")) {
            } else if (Str_Equals(instr, "csrrd")) {
            } else if (Str_Equals(instr, "csrwr")) {
            } else {
                Show_Error("Line %d: Unknown instruction `%s`:\n %s", linenum, instr, line);
            }
        }

        /*
         * Free memory
         */
        if (label)
            free(label);
        if (direc)
            free(direc);
        if (instr)
            free(instr);
        if (other) 
            free(other);
        for (int i = 0; i < argc; i++)
            free(args[i]);
        argc = 0;
    }
    if (line)
        free(line);
}

int main(int argc, char *argv[])
{
    int i = 0;
    FILE *input = NULL;
    FILE *output = NULL;

    /* Set input and output */
    enum {
        F_HEX = 1
    };
    int flags = 0;
    for (i = 1; i < argc; i++) {
        char *option = argv[i];
        if (strcmp(option, "-hex") == 0) {
            flags |= F_HEX;
        } else if (strcmp(option, "-o") == 0) {
            // Output option (-o)
            if (i + 1 == argc) {
                fprintf(stderr, "Error: -o Option requires an argument\n");
                exit(EXIT_FAILURE);
            }
            char *output_path = argv[i + 1];
            i += 1;
            if (strcmp(output_path, "-") == 0) {
                output = stdout;
            } else {
                output = fopen(output_path, "w");
                if (output == NULL) {
                    fprintf(stderr, "Error: Failed to open \"%s\" for writing\n", output_path);
                    exit(EXIT_FAILURE);
                }
            }
        } else {
            // Input option (no argument)
            if (input) {
                fprintf(stderr, "Warning: Multiple input files set\n");
            } else {
                char *input_path = option;
                if (strcmp(input_path, "-") == 0) {
                    input = stdin;
                } else {
                    input = fopen(input_path, "r");
                    if (input == NULL) {
                        fprintf(stderr, "Error: Failed to open file \"%s\" for reading\n", input_path);
                        exit(EXIT_FAILURE);
                    }
                }
            }
        }
    }
    char fdsa[] = "          Hello how are you?              ";
    Str_Trim(fdsa);
    printf("'%s'", fdsa);


    /* Check input and output */
    if (input == NULL || output == NULL)
        show_help();
    Elf_Builder elf;
    Elf_Init(&elf);
    Asm_Compile(&elf, input, F_DEBUG);


    Elf_UseSection(&elf, "my_code");
    Elf_WriteDump(&elf, stdout);

    if (flags | F_HEX)  {
        Elf_WriteHex(&elf, output);
    } else {
        Elf_WriteDump(&elf, output);
    }
    Elf_Destroy(&elf);

    /* Close files */
    if (input != stdin) 
        fclose(input);
    if (output != stdout)
        fclose(output);
    return 0;
}

