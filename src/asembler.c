#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <assert.h>
#include <regex.h>
#include <ctype.h>
#include <asembler.h>
#include <elf.h>
#define MAX_ASM_ARGS 100
#define MAX_ASM_SECTIONS 100
#define ARR_SIZE(arr) (sizeof((arr)) / sizeof((arr)[0]))
#define BUF_DEFAULT_SIZE 10
#define MAX_REGEX_MATCHES 100
#ifdef DEBUG
#define debug(...) do { \
        fprintf(stdout, "\e[32mDebug: \e[0m"); \
        fprintf(stdout, __VA_ARGS__); \
        fprintf(stdout, "\n"); \
        fflush(stdout); \
    } while (0)
#else
#define debug(...)
#endif

#define error(...) do { \
        fprintf(stderr, "\e[31mError: \e[0m"); \
        fprintf(stderr, __VA_ARGS__); \
        fprintf(stdout, "\n"); \
        fflush(stderr); \
    } while (0)

typedef struct buf_s {
    char *b_buffer;
    int b_size;
    int b_count;
} buf_t;

void buf_init(buf_t *buf)
{
    buf->b_buffer = (char*) calloc(BUF_DEFAULT_SIZE, 1);
    assert(buf->b_buffer != NULL);
    buf->b_buffer[0] = '\0';
    buf->b_size = BUF_DEFAULT_SIZE;
    buf->b_count = 0;
}

void buf_putc(buf_t *buf, char ch) 
{
    if (buf->b_count == buf->b_size) {
        buf->b_buffer = realloc(buf->b_buffer, (buf->b_size *= 2));
        assert(buf->b_buffer != NULL);
    }
    buf->b_buffer[buf->b_count] = ch;
    buf->b_count += 1;
    buf->b_buffer[buf->b_count] = '\0';
}

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


char* Str_RmSpaces(char *str)
{
    char* curs = str;
    while (*str) {
        if (! isspace(*str)) {
            *curs = *str;
            curs++;
        }
        str++;
    }
    *curs = '\0';
}

char* str_rmquotes(char *str)
{
    char *cur = str;
    enum {
        SKIP_SPACES,
        SKIP_DATA
    };
}

typedef struct section_t {
    struct section_t *s_next;
    char *s_name;
    char *s_data;
    char *s_size;
    char *s_total;
} section_t;

section_t *section_new(const char *name)
{
    section_t *sec_new = calloc(1, sizeof(section_t));
    assert(sec_new != NULL);
    sec_new->s_name = strdup(name);
    sec_new->s_next = NULL;
}


typedef struct label_t {
    char *l_name;
} label_t;

typedef struct symbol_t {
    char *s_name;
    int s_type;
} symbol_t;


void Str_RmQuotes(char *str)
{
    char *head = str;
    char *iter = str;
    enum {
        SKIPPING = 0,
        READING = 1
    };
    int mode = SKIPPING;
    char prev = '\0';
    while (*iter) {
        if (*iter == '"')  {
            if (prev == '\\')  {
                *head++ = '"';
            } else {
                mode = (mode + 1) % 2;
            }
        } else {
            if (mode == READING) {
                *head++ = *iter;
            }
            prev = *iter;
        }
        *iter = '\0';
        iter++;
    }
}

void Asm_Compile(Elf_Builder *elf, FILE *input, int flags)
{
    /*
     * Init elf
     */

    int linenum = 0;
    int nread = 0;
    char *line = NULL;
    ssize_t linelen;

    regex_t re_empty; // Matches empty lines
    regex_t re_label; // Matches line label
    regex_t re_instr; // Matches instruction or directive
    regex_t re_comnt; // Matches comment
    regex_t re_isnum; // Matches numbers
    regmatch_t matches[MAX_REGEX_MATCHES];
    if (regcomp(&re_empty, "^\\s*(#.*)*$", REG_EXTENDED)) {
        fprintf(stderr, "Error compiling regex for empty lines\n");
        return;
    }
    if (regcomp(&re_label, "^\\s*([^ \t]*)\\s*:\\s*", REG_EXTENDED)) {
        fprintf(stderr, "Error compiling regex for labels\n");
        return;
    }
    if (regcomp(&re_comnt, "([^#]*)#(.*)$", REG_EXTENDED)) {
        fprintf(stderr, "Error compiling regex for comments\n");
        return;
    }
    // \\s*([^ :]*:)?\\s*(.?[a-zA-Z]+)\\s*(.*)?(a.*#)?$
    if (regcomp(&re_instr, "^\\s*(.?[^ \t]+)\\s*", REG_EXTENDED)) {
        fprintf(stderr, "Error compiling regex for instructions\n");
        return;
    }

    if (regcomp(&re_isnum, "^\\s*([-+]?[0-9]+)\\s*$", REG_EXTENDED)) {
        fprintf(stderr, "Error compiling regex for numbers\n");
        return;
    }

    enum {
        MODE_OK,
        MODE_QUIT
    };
    int mode = MODE_OK;
    while (mode != MODE_QUIT && (nread = getline(&line, &linelen, input)) != -1) {
        /*
         * read line
         */
        linenum += 1;
        line[strlen(line) - 1] = '\0';
        char *lbuf = line;

        /*
         * Extract label, instruction, arguments
         */
        char *label = NULL;
        char *instr = NULL;
        char *args[MAX_ASM_ARGS];
        int argc = 0;

        // Skip empty or comment lines
        if (regexec(&re_empty, lbuf, ARR_SIZE(matches), matches, 0) == 0) {
            continue;
        }
        
        // Check if line has a label
        if (regexec(&re_label, lbuf, ARR_SIZE(matches), matches, 0) == 0) {
            // Read label
            regoff_t start = matches[1].rm_so;
            regoff_t end = matches[1].rm_eo;
            regoff_t len = end - start;
            label = malloc(len + 1);
            assert(label != NULL);
            strncpy(label, lbuf + start, len);
            label[len] = '\0';

            // Skip
            lbuf += (matches[0].rm_eo - matches[0].rm_so);
        }

        // Remove comments
        if (regexec(&re_comnt, lbuf, ARR_SIZE(matches), matches, 0) == 0) {
            // Skip comment
            regoff_t start = matches[1].rm_so;
            regoff_t end = matches[1].rm_eo;
            regoff_t len = end - start;
            lbuf[len] = '\0';
        }

        // Extract directive or instruction
        if (regexec(&re_instr, lbuf, ARR_SIZE(matches), matches, 0) == 0) {
            regoff_t start = matches[1].rm_so;
            regoff_t end = matches[1].rm_eo;
            regoff_t len = end - start;
            instr = malloc(len + 1);
            strncpy(instr, lbuf + start, len);
            instr[len] = '\0';

            // Skip instruction
            lbuf += (matches[0].rm_eo - matches[0].rm_so);
            // Read arguments
            //printf("Instr: '%s' ", instr);

            char *arg = strtok(lbuf, ",");
            while (arg) {
                args[argc] = strdup(arg);
                argc += 1;
                arg = strtok(NULL, ",");
            }
        }

        /*
         * Parse
         */
        do {
            if (label) {
                Elf_Sym *sym = Elf_UseSymbol(elf, label);
                sym->st_shndx = Elf_GetCurrentSection(elf);
                sym->st_value = Elf_GetSectionSize(elf);
            }
            if (! instr) 
                break;
            if (*instr == '.') {
                if (strcmp(instr, ".global") == 0) {
                    for (int i = 0; i < argc; i++) {
                        Str_RmSpaces(args[i]);
                        char *name = args[i];
                        Elf_Sym *sym = Elf_UseSymbol(elf, name);

                        sym->st_info = ELF_ST_INFO(
                            STB_GLOBAL,
                            ELF_ST_TYPE(sym->st_info)
                        );
                    }
                } else if (strcmp(instr, ".extern") == 0) {
                } else if (strcmp(instr, ".section") == 0) {
                    if (argc < 1 || argc >= 2) {
                        error("Directive '.section' at line %d requires exatly 1 argument", linenum);
                        break;
                    }
                    debug(".section directive on line %d", linenum);
                    char *name = args[0];
                    Str_RmSpaces(name);
                    Elf_UseSection(elf, name);
                    debug("Current section name: '%s'", Elf_GetSectionName(&elf));
                } else if (strcmp(instr, ".word") == 0) {
                    debug(".word directive on line %d", linenum);
                    char *arg = args[0];
                    Str_RmSpaces(arg);
                    if (regexec(&re_isnum, arg, ARR_SIZE(matches), matches, 0) == 0) {
                        Elf_PushHalf(elf, strtol(arg, NULL, 10));
                    } else {
                        
                    }
                } else if (strcmp(instr, ".skip") == 0) {
                    debug(".skip directive on line %d", linenum);
                } else if (strcmp(instr, ".ascii") == 0) {
                    debug(".ascii directive on line %d", linenum);
                    char *arg = args[0];
                    Str_RmQuotes(arg);
                    printf("(%s)", arg);
                    Elf_PushString(elf, arg);
                } else if (strcmp(instr, ".equ") == 0) {
                    // NOT IMPLEMENTED
                } else if (strcmp(instr, ".end") == 0) {
                    mode = MODE_QUIT;
                } else {
                    error("Unknown directive '%s' at line %d\n", instr, linenum);
                }
            } else {
                if (strcmp(instr, "halt") == 0) {
                } else if (strcmp(instr, "int") == 0) {
                } else if (strcmp(instr, "iret") == 0) {
                } else if (strcmp(instr, "call") == 0) {
                } else if (strcmp(instr, "ret") == 0) {
                } else if (strcmp(instr, "jmp") == 0) {
                } else if (strcmp(instr, "jeq") == 0) {
                } else if (strcmp(instr, "jne") == 0) {
                } else if (strcmp(instr, "jgt") == 0) {
                } else if (strcmp(instr, "push") == 0) {
                } else if (strcmp(instr, "pop") == 0) {
                } else if (strcmp(instr, "xchg") == 0) {
                } else if (strcmp(instr, "add") == 0) {
                } else if (strcmp(instr, "sub") == 0) {
                } else if (strcmp(instr, "mul") == 0) {
                } else if (strcmp(instr, "div") == 0) {
                } else if (strcmp(instr, "cmp") == 0) {
                } else if (strcmp(instr, "not") == 0) {
                } else if (strcmp(instr, "and") == 0) {
                } else if (strcmp(instr, "or") == 0) {
                } else if (strcmp(instr, "xor") == 0) {
                } else if (strcmp(instr, "test") == 0) {
                } else if (strcmp(instr, "shl") == 0) {
                } else if (strcmp(instr, "shr") == 0) {
                } else if (strcmp(instr, "ldr") == 0) {
                } else if (strcmp(instr, "str") == 0) {
                } else {
                    fprintf(stderr, "Unknown instruction '%s' at line %d\n", instr, linenum);
                }
            }
        } while (0);

        // Free memory
        if (label)
            free(label);
        if (instr)
            free(instr);
        for (int i = 0; i < argc; i++)
            free(args[i]);
    }
    if (line)
        free(line);
    regfree(&re_empty);
    regfree(&re_label);
    regfree(&re_instr);
    regfree(&re_comnt);
    regfree(&re_isnum);
}

int ss_asm(FILE *input, FILE *output, int falgs)
{
    /*
     * Init elf
     */
    section_t *section_head = NULL;
    section_t *section_curr = NULL;
    symbol_t *symbol_head = NULL;

    int linenum = 0;
    int nread = 0;
    char *line = NULL;
    ssize_t linelen;

    regex_t re_empty; // Matches empty lines
    regex_t re_label; // Matches line label
    regex_t re_instr; // Matches instruction or directive
    regex_t re_comnt; // Matches comment
    regmatch_t matches[MAX_REGEX_MATCHES];
    if (regcomp(&re_empty, "^\\s*(#.*)*$", REG_EXTENDED)) {
        fprintf(stderr, "Error compiling regex for empty lines\n");
        return -1;
    }
    if (regcomp(&re_label, "^\\s*([^ \t]*)\\s*:\\s*", REG_EXTENDED)) {
        fprintf(stderr, "Error compiling regex for labels\n");
        return -1;
    }
    if (regcomp(&re_comnt, "([^#]*)#(.*)$", REG_EXTENDED)) {
        fprintf(stderr, "Error compiling regex for comments\n");
        return -1;
    }
    // \\s*([^ :]*:)?\\s*(.?[a-zA-Z]+)\\s*(.*)?(a.*#)?$
    if (regcomp(&re_instr, "^\\s*(.?[^ \t]+)\\s*", REG_EXTENDED)) {
        fprintf(stderr, "Error compiling regex for instructions\n");
        return -1;
    }


    while ((nread = getline(&line, &linelen, input)) != -1) {
        /*
         * read line
         */
        linenum += 1;
        line[strlen(line) - 1] = '\0';
        char *lbuf = line;

        /*
         * Extract label, instruction, arguments
         */
        char *label = NULL;
        char *instr = NULL;
        char *args[MAX_ASM_ARGS];
        int argc = 0;

        // Skip empty or comment lines
        if (regexec(&re_empty, lbuf, ARR_SIZE(matches), matches, 0) == 0) {
            continue;
        }
        
        // Check if line has a label
        if (regexec(&re_label, lbuf, ARR_SIZE(matches), matches, 0) == 0) {
            // Read label
            regoff_t start = matches[1].rm_so;
            regoff_t end = matches[1].rm_eo;
            regoff_t len = end - start;
            label = malloc(len + 1);
            assert(label != NULL);
            strncpy(label, lbuf + start, len);
            label[len] = '\0';

            // Skip
            lbuf += (matches[0].rm_eo - matches[0].rm_so);
        }

        // Remove comments
        if (regexec(&re_comnt, lbuf, ARR_SIZE(matches), matches, 0) == 0) {
            // Skip comment
            regoff_t start = matches[1].rm_so;
            regoff_t end = matches[1].rm_eo;
            regoff_t len = end - start;
            lbuf[len] = '\0';
        }

        // Extract directive or instruction
        if (regexec(&re_instr, lbuf, ARR_SIZE(matches), matches, 0) == 0) {
            regoff_t start = matches[1].rm_so;
            regoff_t end = matches[1].rm_eo;
            regoff_t len = end - start;
            instr = malloc(len + 1);
            strncpy(instr, lbuf + start, len);
            instr[len] = '\0';

            // Skip instruction
            lbuf += (matches[0].rm_eo - matches[0].rm_so);
            // Read arguments
            //printf("Instr: '%s' ", instr);

            char *arg = strtok(lbuf, ",");
            while (arg) {
                args[argc] = strdup(arg);
                argc += 1;
                arg = strtok(NULL, ",");
            }
        }


        /*
         * Parse
         */
        do {
            if (! instr) 
                break;
            if (*instr == '.') {
                if (strcmp(instr, ".global") == 0) {
                } else if (strcmp(instr, ".extern") == 0) {
                } else if (strcmp(instr, ".section") == 0) {
                    if (argc < 1 || argc >= 2) {
                        error("Directive '.section' at line %d requires exatly 1 argument", linenum);
                        break;
                    }
                    char *sec_name = args[0];
                    Str_RmSpaces(sec_name);

                    // Find section
                    section_t *iter = section_head, *prev = NULL;
                    while (iter) {
                        if (strcmp(iter->s_name, sec_name) == 0)
                            break;
                        prev = iter;
                        iter = iter->s_next;
                    }
                    if (iter == NULL) {
                        iter = section_new(sec_name);
                        if (prev)
                            prev->s_next = iter;
                    }
                    section_curr = iter;
                    debug("Current section name: '%s'", iter->s_name);
                } else if (strcmp(instr, ".word") == 0) {
                    debug(".word directive on line %d", linenum);
                    if (! section_curr) {
                        error("Section is not set");
                        break;
                    }

                } else if (strcmp(instr, ".skip") == 0) {
                } else if (strcmp(instr, ".ascii") == 0) {
                } else if (strcmp(instr, ".equ") == 0) {
                } else if (strcmp(instr, ".end") == 0) {
                } else {
                    error("Unknown directive '%s' at line %d\n", instr, linenum);
                }
            } else {
                if (strcmp(instr, "halt") == 0) {
                } else if (strcmp(instr, "int") == 0) {
                } else if (strcmp(instr, "iret") == 0) {
                } else if (strcmp(instr, "call") == 0) {
                } else if (strcmp(instr, "ret") == 0) {
                } else if (strcmp(instr, "jmp") == 0) {
                } else if (strcmp(instr, "jeq") == 0) {
                } else if (strcmp(instr, "jne") == 0) {
                } else if (strcmp(instr, "jgt") == 0) {
                } else if (strcmp(instr, "push") == 0) {
                } else if (strcmp(instr, "pop") == 0) {
                } else if (strcmp(instr, "xchg") == 0) {
                } else if (strcmp(instr, "add") == 0) {
                } else if (strcmp(instr, "sub") == 0) {
                } else if (strcmp(instr, "mul") == 0) {
                } else if (strcmp(instr, "div") == 0) {
                } else if (strcmp(instr, "cmp") == 0) {
                } else if (strcmp(instr, "not") == 0) {
                } else if (strcmp(instr, "and") == 0) {
                } else if (strcmp(instr, "or") == 0) {
                } else if (strcmp(instr, "xor") == 0) {
                } else if (strcmp(instr, "test") == 0) {
                } else if (strcmp(instr, "shl") == 0) {
                } else if (strcmp(instr, "shr") == 0) {
                } else if (strcmp(instr, "ldr") == 0) {
                } else if (strcmp(instr, "str") == 0) {
                } else {
                    fprintf(stderr, "Unknown instruction '%s' at line %d\n", instr, linenum);
                }
            }
        } while (0);

        // Free memory
        if (label)
            free(label);
        if (instr)
            free(instr);
        for (int i = 0; i < argc; i++)
            free(args[i]);
    }
    free(line);
    regfree(&re_empty);
    regfree(&re_label);
    regfree(&re_instr);
    regfree(&re_comnt);
    return 0;
}


void Test1() {
    Elf_Builder elf;
    Elf_Init(&elf);
    Elf_CreateSection(&elf, ".text");
    Elf_PushSection(&elf);
    Elf_PushString(&elf, "Nigga what the fuck is this shit holy shit");
    Elf_PushString(&elf, "SAL SNEEDERINO PRETTY FUCKING BASED ?? ?? ? ?");
    Elf_UseSymbol(&elf, "nigga");
    Elf_UseSymbol(&elf, "nigga");

    Elf_UseSection(&elf, ".hello");
    Elf_AddRelaSymb(&elf, "A");
    Elf_AddRelaSymb(&elf, "B");
    Elf_AddRelaSymb(&elf, "C");
    Elf_AddRelaSymb(&elf, "D");
    Elf_PushString(&elf, "SAfdsafdsafdasfdasfdsaL SNEEDERINO PRETTY FUCKING BASED ?? ?? ? ?");

    Elf_PopSection(&elf);
    FILE  *out = fopen("here.elf", "w");
    Elf_WriteHex(&elf, out);
    fclose(out);

    FILE *in = fopen("here.elf", "r");
    Elf_Builder elf2;
    Elf_ReadHexInit(&elf2, in);
    fclose(in);
    Elf_WriteDump(&elf2, stdout);
    Elf_Destroy(&elf2);
}


void Test2() {
    Elf_Builder elf1, elf2;
    FILE *file1 = NULL, *file2 = NULL;

    // Create first elf
    printf("\e[31mFirst elf:\e[0m\n");
    Elf_Init(&elf1);
    Elf_PushSection(&elf1);
    Elf_UseSection(&elf1, ".text1");
    Elf_PushSkip(&elf1, 120, 0x55);
    Elf_UseSection(&elf1, ".text");
    Elf_UseSection(&elf1, ".text2");
    Elf_UseSection(&elf1, ".text3");
    Elf_UseSection(&elf1, ".text");
    Elf_Sym *sym = Elf_CreateSymbol(&elf1, "_ZFunction");
    sym->st_info = ELF_ST_INFO(STB_GLOBAL, STT_FUNC);
    Elf_UseSection(&elf1, ".text");
    Elf_PushSkip(&elf1, 48, 0x11);

    Elf_UseSection(&elf1, ".iv_table");
    Elf_AddRelaSymb(&elf1, "Ok");
    Elf_AddRelaSymb(&elf1, "Nigga");

    Elf_UseSection(&elf1, ".data");

    Elf_PopSection(&elf1);
    //Elf_WriteDump(&elf1, stdout);

    // Create second elf
    printf("\e[32mSecond elf:\e[0m\n");
    Elf_Init(&elf2);
    Elf_PushSection(&elf2);
    Elf_UseSection(&elf2, ".data");
    Elf_AddRelaSymb(&elf2, "What");
    Elf_CreateSymbol(&elf2, "nigga");
    Elf_PushSkip(&elf2, 12, 0x33);
    Elf_PopSection(&elf2);


    // Save them 
    file1 = fopen("elf1.hex", "w");
    file2 = fopen("elf2.hex", "w");
    Elf_WriteHex(&elf1, file1);
    Elf_WriteHex(&elf2, file2);
    fclose(file1);
    fclose(file2);

    // Load them
    Elf_Builder elf3, elf4;
    file1 = fopen("elf1.hex", "r");
    file2 = fopen("elf2.hex", "r");
    Elf_ReadHexInit(&elf3, file1);
    Elf_ReadHexInit(&elf4, file2);
    fclose(file1);
    fclose(file2);
    //Elf_WriteDump(&elf4, stdout);

    // Link them
    Elf_Builder elf5;
    Elf_Init(&elf5);
    Elf_Link(&elf5, &elf4);
    Elf_Link(&elf5, &elf3);
    printf("\e[33mThird elf:\e[0m\n");
    Elf_WriteDump(&elf5, stdout);
    const char *str = ".data";
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

    /* Check input and output */
    if (input == NULL || output == NULL)
        show_help();

    Elf_Builder elf;
    Elf_Init(&elf);
    Asm_Compile(&elf, input, F_DEBUG);
    Elf_WriteDump(&elf, output);

    /* Close files */
    if (input != stdin) 
        fclose(input);

    if (output != stdout)
        fclose(output);
    return 0;
}
