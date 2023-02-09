#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <assert.h>
#include <regex.h>
#include <ctype.h>
#include <asembler.h>
#include <elf.h>
#include <stdarg.h>
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




int strsel(const char *str, ...) {
    va_list va;
    va_start(va, str);
    int index = 0;
    char *arg = va_arg(va, char*);
    while (arg)  {
        if (strcmp(arg, str) == 0) {
            va_end(va);
            return index;
        }

        index += 1;
        arg = va_arg(va, char*);
    }
    va_end(va);
    return -1;
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

char *Str_Substr(char *str, int from, int to)
{
    int len = strlen(str);
    if (from > to)  {
        int tmp = from;
        from = to;
        to = from;
    }
    if (from < 0) 
        from = 0;
    if (from > len)
        from = len;
    if (to < 0)
        to = len;
    char *substr = calloc(to - from + 1, sizeof(char));
    assert(substr != NULL);
    strncpy(substr, str + from, to - from);
    return substr;
}

Elf_Byte Asm_ParseRegOperand(char *arg)
{
    regex_t re_reg;
    regmatch_t matches[MAX_REGEX_MATCHES];
    if (regcomp(&re_reg, "^r([0-9])$", REG_EXTENDED)) {
        fprintf(stderr, "Error (Assembler): Failed compiling regex for registers\n");
        return 0xf;
    }
    if (regexec(&re_reg, arg, ARR_SIZE(matches), matches, 0) == 0) {
        return arg[1] - '0';
    }
    return 0xf;
}

int Str_RegMatch(const char *str, const char *re, int match_count, regmatch_t *matches) 
{
    regex_t regex; 
    if (regcomp(&regex, re, REG_EXTENDED))  {
        fprintf(stderr, "Error (Assembler): Failed to compile regex '%s'\n", re);
        return 0;
    }
    int result = 0;
    if (regexec(&regex, str, match_count, matches, 0) == 0) {
        result = 1;
    }
    regfree(&regex);
    return result;
}

int Str_ParseInt(const char *str) {
    int sign = 1;
    if (*str == '+') {
        str ++;
    }
    if (*str == '-') {
        sign = -1;
        str ++;
    }
    int value = 0;
    if (*str == '0')  {
        // If 0 is the only character, the value is 0
        if (*(str + 1) == '\0') 
            return 0;

        // Potentially octal or hexadecimal
        if (*(str + 1) == 'x')  {
            // Hexadecimal
            return sign * strtol(str + 2, NULL, 16);
        } else {
            // Octal
            return sign * strtol(str + 1, NULL, 8);
        }
    } else {
        // Decimal
        return sign * strtol(str, NULL, 10);
    }
    return value;
}


void Asm_Compile(Elf_Builder *elf, FILE *input, int flags)
{
    // Init variables used for line reading
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
    if (regcomp(&re_instr, "^\\s*(.?[^ \t]+)\\s*", REG_EXTENDED)) {
        fprintf(stderr, "Error compiling regex for instructions\n");
        return;
    }
    if (regcomp(&re_isnum, "^\\s*([-+]?[0-9]+)\\s*$", REG_EXTENDED)) {
        fprintf(stderr, "Error compiling regex for numbers\n");
        return;
    }


    enum {
        MODE_OK,  // Default
        MODE_QUIT // This mode will be set on .end directive
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
        enum {
            AT_LITERAL,
            AT_SYMBOL,
        };
        int argt[MAX_ASM_ARGS]; // Arg types

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
                    Str_RmSpaces(args[0]);
                    int count = Str_ParseInt(args[0]);
                    Elf_PushSkip(elf, count, 0x00);
                } else if (strcmp(instr, ".ascii") == 0) {
                    debug(".ascii directive on line %d", linenum);
                    char *arg = args[0];
                    Str_RmQuotes(arg);
                    Elf_PushString(elf, arg);
                } else if (strcmp(instr, ".equ") == 0) {
                    // NOT IMPLEMENTED
                } else if (strcmp(instr, ".end") == 0) {
                    mode = MODE_QUIT;
                } else {
                    error("Unknown directive '%s' at line %d\n", instr, linenum);
                }
            } else {
                // Parse the arguments
                int i;
                Asm_Instr ai;

                // Don't need spaces
                for (i = 0; i < argc; i++) {
                    Str_RmSpaces(args[i]);
                }

                // Push instruction
                bzero(&ai, sizeof(ai));

                int index = 0;
                if (strcmp(instr, "halt") == 0) {
                    // Halt instruction
                    ai.ai_oc = 0x0;
                    ai.ai_mod = 0x0;
                    Elf_PushByte(elf, ai.ai_instr);
                } else if (strcmp(instr, "int") == 0) {
                    // Software interrupt instruction
                    Str_RmSpaces(args[0]);
                    ai.ai_oc = 0x1;
                    ai.ai_mod = 0x0;
                    ai.ai_rd = Asm_ParseRegOperand(args[0]);
                    ai.ai_rs = 0xf;
                    Elf_PushByte(elf, ai.ai_instr);
                    Elf_PushByte(elf, ai.ai_regdesc);
                } else if (strcmp(instr, "iret") == 0) {
                    // Return from interrupt instruction
                    ai.ai_oc = 0x2;
                    ai.ai_mod = 0x0;
                    Elf_PushByte(elf, ai.ai_instr);
                } else if (strcmp(instr, "ret") == 0) {
                    // Return from function instructio
                    ai.ai_oc = 0x4;
                    ai.ai_mod = 0;
                    Elf_PushByte(elf, ai.ai_instr);
                } else if ((index = strsel(instr, "call", "jmp", "jeq", "jne", "jgt", NULL)) != -1) {
                    // Jump instruction
                    if (index == 1) {
                        // Call
                        ai.ai_oc = 0x3;
                        ai.ai_mod = 0x0;
                    } else {
                        // Jmp instructions
                        ai.ai_oc = 0x5;
                        ai.ai_mod = index & 0xf;
                    }
                    if (Str_RegMatch(args[0], "^\\*(r[0-9])$", ARR_SIZE(matches), matches)) {
                        /* *<reg> */

                        // Parse register
                        char *reg_str = Str_Substr(args[0], matches[1].rm_so, matches[1].rm_eo);
                        ai.ai_rd = 0xf;
                        ai.ai_rs = Asm_ParseRegOperand(reg_str);
                        free(reg_str);

                        // Set addr mode
                        ai.ai_am = ASM_AM_REGDIR;
                        ai.ai_up = ASM_UP_NONE;

                        // Push bytes
                        Elf_PushByte(elf, ai.ai_instr);
                        Elf_PushByte(elf, ai.ai_regdesc);
                        Elf_PushByte(elf, ai.ai_addrmode);
                    } else if (Str_RegMatch(args[0], "^\\*?([0-9a-fA-FxX]+)$", ARR_SIZE(matches), matches)) {
                        /* <literal> | *<literal> */

                        // Registers are unused
                        ai.ai_rd = 0xf;
                        ai.ai_rs = 0xf;

                        // Set addr mode
                        ai.ai_am = (*args[0] == '*') 
                            ? ASM_AM_MEMORY
                            : ASM_AM_REGDIR;
                        ai.ai_up = ASM_UP_NONE;

                        // Parse payload
                        char *payload_str = Str_Substr(args[0], matches[1].rm_so, matches[1].rm_eo);
                        int payload = Str_ParseInt(payload_str);
                        free(payload_str);
                        
                        // Push bytes
                        Elf_PushByte(elf, ai.ai_instr);
                        Elf_PushByte(elf, ai.ai_regdesc);
                        Elf_PushByte(elf, ai.ai_addrmode);
                        Elf_PushByte(elf, payload >> 8);
                        Elf_PushByte(elf, payload & 0xff);
                    } else if (Str_RegMatch(args[0], "^[%*]?([^0-9][._a-zA-Z0-9]*)$", ARR_SIZE(matches), matches)) {
                        /* <symbol> | *<symbol> | %<symbol> */

                        // Registers are unused
                        // Except if it's PC relative, then we use the PC register
                        // Which is register number 0x7
                        ai.ai_rs = (*args[0] == '%') 
                            ? 0x7
                            : 0xf;
                        ai.ai_rd = 0xf;

                        // Set addr mode
                        ai.ai_am = (*args[0] == '*') ? ASM_AM_MEMORY
                            : (*args[0] == '%') ? ASM_AM_REGDIR16
                            : ASM_AM_REGDIR;
                        ai.ai_up = ASM_UP_NONE;

                        // Get symbol name symbol
                        char *sym_name= Str_Substr(args[0], matches[1].rm_so, matches[1].rm_eo);
                        printf("%s\n", sym_name);

                        // Add rela
                        int rela_type = (*args[0] == '%') 
                            ? R_X86_64_PC16 
                            : R_X86_64_16;
                        Elf_PushSection(elf);
                        Elf_Rela *rela = Elf_AddRelaSymb(elf, sym_name);
                        rela->r_addend = 3;
                        rela->r_info = ELF_R_INFO(ELF_R_SYM(rela->r_info), rela_type);
                        Elf_PopSection(elf);

                        // Free symbol name memory
                        free(sym_name);

                        // Push bytes
                        Elf_PushByte(elf, ai.ai_instr);
                        Elf_PushByte(elf, ai.ai_regdesc);
                        Elf_PushByte(elf, ai.ai_addrmode);
                        // Next two bytes will be replaced by Rela
                        Elf_PushByte(elf, 0);
                        Elf_PushByte(elf, 0);
                    } else if (Str_RegMatch(args[0], "^[*]\\[(r[0-9])\\]$", ARR_SIZE(matches), matches)) {
                        /* *[ <reg> ]*/

                        // Parse register
                        char *reg_str = Str_Substr(args[0], matches[1].rm_so, matches[1].rm_eo);
                        ai.ai_rd = 0xf;
                        ai.ai_rs = Asm_ParseRegOperand(reg_str);
                        free(reg_str);

                        // Set addr mode
                        ai.ai_am = ASM_AM_REGIND;
                        ai.ai_up = ASM_UP_NONE;

                        // Push bytes
                        Elf_PushByte(elf, ai.ai_instr);
                        Elf_PushByte(elf, ai.ai_regdesc);
                        Elf_PushByte(elf, ai.ai_addrmode);
                    } else if (Str_RegMatch(args[0], "^[*]\\[(r[0-9])\\+([0-9a-fA-FxX]+)\\]$", ARR_SIZE(matches), matches)) {
                        /* *[ <reg> + <literal> ] */

                        // Parse register
                        char *reg_str = Str_Substr(args[0], matches[1].rm_so, matches[1].rm_eo);
                        ai.ai_rd = 0xf;
                        ai.ai_rs = Asm_ParseRegOperand(reg_str);
                        free(reg_str);

                        // Parse payload
                        char *payload_str = Str_Substr(args[1], matches[1].rm_so, matches[1].rm_eo);
                        int payload = Str_ParseInt(payload_str);
                        free(payload_str);

                        // Set addr mode
                        ai.ai_am = ASM_AM_REGIND16;
                        ai.ai_up = ASM_UP_NONE;

                        // Push bytes
                        Elf_PushByte(elf, ai.ai_instr);
                        Elf_PushByte(elf, ai.ai_regdesc);
                        Elf_PushByte(elf, ai.ai_addrmode);
                        Elf_PushByte(elf, payload >> 8);
                        Elf_PushByte(elf, payload & 0xff);
                    } else if (Str_RegMatch(args[0], "^[*]\\[(r[0-9])\\+([^0-9][._a-zA-Z0-9]*)]$", ARR_SIZE(matches), matches)) {
                        /* *[ <reg> + <symbol> ] */

                        // Parse register
                        char *reg_str = Str_Substr(args[0], matches[1].rm_so, matches[1].rm_eo);
                        ai.ai_rd = 0xf;
                        ai.ai_rs = Asm_ParseRegOperand(reg_str);
                        free(reg_str);

                        // Get symbol name
                        char *sym_name= Str_Substr(args[1], matches[1].rm_so, matches[1].rm_eo);

                        // Add rela
                        int rela_type = (*args[0] == '%') 
                            ? R_X86_64_PC16 
                            : R_X86_64_16;
                        Elf_PushSection(elf);
                        Elf_Rela *rela = Elf_AddRelaSymb(elf, sym_name);
                        rela->r_addend = 3;
                        rela->r_info = ELF_R_INFO(ELF_R_SYM(rela->r_info), rela_type);
                        Elf_PopSection(elf);

                        // Free symbol name memory
                        free(sym_name);

                        // Set addr mode
                        ai.ai_am = ASM_AM_REGIND16;
                        ai.ai_up = ASM_UP_NONE;

                        // Push bytes
                        Elf_PushByte(elf, ai.ai_instr);
                        Elf_PushByte(elf, ai.ai_regdesc);
                        Elf_PushByte(elf, ai.ai_addrmode);
                        // Next two bytes will be replaced by Rela
                        Elf_PushByte(elf, 0);
                        Elf_PushByte(elf, 0);
                    } else {
                        fprintf(stderr, "Error (Assembler): Bad syntax for instruction '%s' at line %d\n", instr, linenum);
                    }
                } else if (strcmp(instr, "push") == 0) {
                    // Push instruction
                } else if (strcmp(instr, "pop") == 0) {
                    // Pop instruction
                } else if (strcmp(instr, "xchg") == 0) {
                    // Exchange value instruction
                    ai.ai_oc = 0x6;
                    ai.ai_mod = 0x0;
                    ai.ai_rs = Asm_ParseRegOperand(args[0]);
                    ai.ai_rd = Asm_ParseRegOperand(args[1]);
                    Elf_PushByte(elf, ai.ai_instr);
                    Elf_PushByte(elf, ai.ai_regdesc);
                } else if ((index = strsel(instr, "add", "sub", "mul", "div", "cmp", NULL)) != -1) {
                    // Arithmetic instruction
                    ai.ai_oc = 0x7;
                    ai.ai_mod = index & 0xf;
                    ai.ai_rs = Asm_ParseRegOperand(args[0]);
                    ai.ai_rd = Asm_ParseRegOperand(args[1]);
                    Elf_PushByte(elf, ai.ai_instr);
                    Elf_PushByte(elf, ai.ai_regdesc);
                } else if ((index = strsel(instr, "not", "and", "or", "xor", "test", NULL)) != -1) {
                    // Logic instruction
                    ai.ai_oc = 0x8;
                    ai.ai_mod = index & 0xf;
                    ai.ai_rs = Asm_ParseRegOperand(args[0]);
                    ai.ai_rd = Asm_ParseRegOperand(args[1]);
                    Elf_PushByte(elf, ai.ai_instr);
                    Elf_PushByte(elf, ai.ai_regdesc);
                } else if ((index = strsel(instr, "shl", "shr", NULL)) != -1) {
                    // Shift instruction
                    ai.ai_oc = 0x9;
                    ai.ai_mod = index & 0xf;
                    ai.ai_rs = Asm_ParseRegOperand(args[0]);
                    ai.ai_rd = Asm_ParseRegOperand(args[1]);
                    Elf_PushByte(elf, ai.ai_instr);
                    Elf_PushByte(elf, ai.ai_regdesc);
                } else if ((index = strsel(instr, "ldr", "str", NULL)) != -1) {
                    ai.ai_oc = 0xa + index; // 0xa or 0xb
                    ai.ai_mod = 0x0;
                    ai.ai_rd = Asm_ParseRegOperand(args[0]);
                    // Parse second operarnd 
                    if (Str_RegMatch(args[1], "^\\$?([0-9a-fA-FxX]+)$", ARR_SIZE(matches), matches)) {
                        /* $<literal> */

                        // Source register is unused
                        ai.ai_rs = 0xf;

                        // Set addr type 
                        ai.ai_am = (*args[1] == '$') ? ASM_AM_IMMED : ASM_AM_MEMORY; // Immediate or Memory
                        ai.ai_up = 0x0; // no change

                        // Parse argument
                        char *arg = Str_Substr(args[1], matches[1].rm_so, matches[1].rm_eo);
                        int payload = Str_ParseInt(arg);
                        free(arg);

                        // Push bytes
                        Elf_PushByte(elf, ai.ai_instr);
                        Elf_PushByte(elf, ai.ai_regdesc);
                        Elf_PushByte(elf, ai.ai_addrmode);
                        Elf_PushByte(elf, payload >> 8);
                        Elf_PushByte(elf, payload & 0xff);
                    } else if (Str_RegMatch(args[1], "^(r[0-9])$", ARR_SIZE(matches), matches)) {
                        /* <reg> */
                        // Set source register
                        ai.ai_rs = Asm_ParseRegOperand(args[1]);
                        ai.ai_am = ASM_AM_REGDIR;
                        ai.ai_up = 0x0;

                        // Push bytes
                        Elf_PushByte(elf, ai.ai_instr);
                        Elf_PushByte(elf, ai.ai_regdesc);
                        Elf_PushByte(elf, ai.ai_addrmode);
                    }  else if (Str_RegMatch(args[1], "^\\[r([0-9])\\]$", ARR_SIZE(matches), matches)) {
                        /* [<reg>] */
                        // Set source register
                        ai.ai_rs = Asm_ParseRegOperand(args[1]);
                        ai.ai_am = ASM_AM_REGIND;
                        ai.ai_up = 0x0;

                        // Push bytes
                        Elf_PushByte(elf, ai.ai_instr);
                        Elf_PushByte(elf, ai.ai_regdesc);
                        Elf_PushByte(elf, ai.ai_addrmode);
                    } else if (Str_RegMatch(args[1], "^[%$]?([^0-9][._a-zA-Z0-9]*)$", ARR_SIZE(matches), matches)) {
                        /* <symbol> | $<symbol> | %<symbol>*/
                        // Source register is unused
                        ai.ai_rs = 0xf;

                        // Set addr type 
                        ai.ai_am = (*args[1] == '$') ? ASM_AM_IMMED 
                            : ASM_AM_MEMORY; 
                        ai.ai_up = 0x0; // no change

                        // Get symbol name
                        char *sym_name= Str_Substr(args[1], matches[1].rm_so, matches[1].rm_eo);

                        // Add rela
                        Elf_PushSection(elf);
                        Elf_Rela *rela = Elf_AddRelaSymb(elf, sym_name);
                        rela->r_addend = 3;
                        rela->r_info = ELF_R_INFO(ELF_R_SYM(rela->r_info), R_X86_64_16);
                        Elf_PopSection(elf);

                        // Free symbol memory
                        free(sym_name);

                        // Push bytes
                        Elf_PushByte(elf, ai.ai_instr);
                        Elf_PushByte(elf, ai.ai_regdesc);
                        Elf_PushByte(elf, ai.ai_addrmode);

                        // Next two bytes will be replaced by Rela
                        Elf_PushByte(elf, 0); 
                        Elf_PushByte(elf, 0);
                    } else if (Str_RegMatch(args[1], "^\\[(r[0-9])\\+([0-9a-fA-FxX]+)\\]$", ARR_SIZE(matches), matches)) {
                        /* [<reg> + <literal>] */

                        // Parse source register
                        char *reg_str = Str_Substr(args[1], matches[1].rm_so, matches[1].rm_eo);
                        ai.ai_rs = Asm_ParseRegOperand(reg_str);
                        free(reg_str);

                        // Parse argument
                        char *payload_str= Str_Substr(args[1], matches[2].rm_so, matches[2].rm_eo);
                        int payload = Str_ParseInt(payload_str);
                        free(payload_str);

                        // Set addr type 
                        ai.ai_am = ASM_AM_REGIND; // Immediate or Memory
                        ai.ai_up = 0x0; // no change

                        // Push bytes
                        Elf_PushByte(elf, ai.ai_instr);
                        Elf_PushByte(elf, ai.ai_regdesc);
                        Elf_PushByte(elf, ai.ai_addrmode);
                        Elf_PushByte(elf, payload >> 8);
                        Elf_PushByte(elf, payload & 0xff);

                    } else if (Str_RegMatch(args[1], "^\\[(r[0-9])\\+([^0-9][._a-zA-Z0-9]*)]$", ARR_SIZE(matches), matches)) {
                        /* [<reg> + <symbol>] */

                        // Parse source register
                        char *reg_str = Str_Substr(args[1], matches[1].rm_so, matches[1].rm_eo);
                        ai.ai_rs = Asm_ParseRegOperand(reg_str);
                        free(reg_str);

                        // Get symbol name
                        char *sym_name= Str_Substr(args[1], matches[2].rm_so, matches[2].rm_eo);

                        // Add rela
                        Elf_PushSection(elf);
                        Elf_Rela *rela = Elf_AddRelaSymb(elf, sym_name);
                        rela->r_addend = 3;
                        rela->r_info = ELF_R_INFO(ELF_R_SYM(rela->r_info), R_X86_64_16);
                        Elf_PopSection(elf);

                        // Free symbol memory
                        free(sym_name);

                        // Set addr type 
                        ai.ai_am = (*args[1] == '$') ? ASM_AM_IMMED : ASM_AM_MEMORY; // Immediate or Memory
                        ai.ai_up = 0x0; // no change


                        // Push bytes
                        Elf_PushByte(elf, ai.ai_instr);
                        Elf_PushByte(elf, ai.ai_regdesc);
                        Elf_PushByte(elf, ai.ai_addrmode);
                        // Next two bytes will be replaced by rela
                        Elf_PushByte(elf, 0x0);
                        Elf_PushByte(elf, 0x0);
                    } else {
                        fprintf(stderr, "Error (Assembler): Bad syntax for instruction '%s' at line %d\n", instr, linenum);
                    }
                } else {
                    fprintf(stderr, "Error (Assembler): Unknown instruction '%s' at line %d\n", instr, linenum);
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
    if (flags | F_HEX)  {
        Elf_WriteHex(&elf, output);
    } else {
        Elf_WriteDump(&elf, output);
    }

    /* Close files */
    if (input != stdin) 
        fclose(input);

    if (output != stdout)
        fclose(output);
    return 0;
}
