#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <assert.h>
#include <regex.h>
#include <ctype.h>
#include "elf.h"
#define MAX_ASM_ARGS 100
#define MAX_ASM_SECTIONS 100


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


#define ARR_SIZE(arr) (sizeof((arr)) / sizeof((arr)[0]))


static void skip_spaces(char **ret) {
    char *curs = *ret;
    while (isblank(*curs))
        curs++;
    *ret = curs;
}


static int is_label(char *curs) {
    int length  = 0;
    while (*curs) {
        if (*curs == ':') {
            return length > 0;
        } else if (isalnum(*curs) || strchr("._-", *curs)) {
            length += 1;
        } else {
            return 0;
        }
        length += 1;
        curs ++;
    }
    return 0;
}

#define BUF_DEFAULT_SIZE 10

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

static void read_label(char **ret_curs, char **ret_label) {
    char *curs = *ret_curs;
    if (*ret_label) 
        free(*ret_label);
    buf_t buf;
    buf_init(&buf);
    skip_spaces(ret_curs);
    while (*curs) {
        if (*curs == ':') {
            break;
        } else if (isalnum(*curs) || strchr("._-", *curs)) {
            buf_putc(&buf, *curs);
        }
        curs++;
    }


    char *label = buf.b_buffer;
    *ret_curs = curs;
    if (ret_label) {
        *ret_label = label;
    } else {
        free(buf.b_buffer);
    }
}

int is_directive(char *curs)
{
    return *curs == '.';
}



void read_directive(char **ret_curs, char **ret_direc) 
{
    char *curs = *ret_curs;
    buf_t buf;
    buf_init(&buf);
    skip_spaces(ret_curs);
    while (isalnum(*curs) || *curs == '.') {
        buf_putc(&buf, *curs);
        curs++;
    }
    if (ret_direc)
        *ret_direc = buf.b_buffer;
    *ret_curs = curs;
}


int is_instruction(char *curs) {
    return isalnum(*curs);
}

void read_instruction(char **ret_curs, char **ret_instr) 
{
    char *curs = *ret_curs;
    buf_t buf;
    buf_init(&buf);
    skip_spaces(ret_curs);
    while (isalnum(*curs) || *curs == '.') {
        buf_putc(&buf, *curs);
        curs++;
    }
    if (ret_instr)
        *ret_instr = buf.b_buffer;
    *ret_curs = curs;
}

enum {
    F_DEBUG = 1
};

int is_comment(char *curs) {
    return *curs == '#';
}

void read_arguments(char **ret_curs, char **ret_args, int *ret_argnum)
{
    char *curs = *ret_curs;
    int argnum = 0;
    buf_t *buf;
    while (*curs && argnum < MAX_ASM_ARGS)  {
        curs ++;
    }
    skip_spaces(ret_curs);
    *ret_curs = curs;
}

char* s_rmspaces(char *str)
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


int ss_asm(FILE *input, FILE *output, int falgs)
{
    int linenum = 0;
    int nread = 0;
    char *line = NULL;
    char *lbuf = line;
    ssize_t linelen;

    #define MAX_REGEX_MATCHES 100
    regex_t re_empty; // Matches empty lines
    regex_t re_label; // Matches line label
    regex_t re_instr;
    regex_t re_comnt;
    regex_t re_regex;
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
        // Read line
        linenum += 1;
        line[strlen(line) - 1] = '\0';
        lbuf = line;
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
            //printf("Label: '%s' After label: '%s'\n", label, lbuf);
        }

        // Remove comments
        if (regexec(&re_comnt, lbuf, ARR_SIZE(matches), matches, 0) == 0) {
            // Skip comment
            regoff_t start = matches[1].rm_so;
            regoff_t end = matches[1].rm_eo;
            regoff_t len = end - start;
            lbuf[len] = '\0';

            // What was the comment?
            //printf("Coment: %.*s\n", (matches[2].rm_eo - matches[2].rm_so), lbuf + matches[2].rm_so);

            // What was the text?
            //printf("Coment: %.*s\n", (matches[1].rm_eo - matches[1].rm_so), lbuf + matches[1].rm_so);
            //printf("Coment: %s\n", lbuf + matches[1].rm_so);
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
                printf("Arg: '%s' ", args[argc]);
                argc += 1;
                arg = strtok(NULL, ",");
            }
            printf("\n");
        }


        if (instr) {
            if (*instr == '.') {
                if (strcmp(instr, ".global") == 0) {
                } else if (strcmp(instr, ".extern") == 0) {
                } else if (strcmp(instr, ".section") == 0) {
                } else if (strcmp(instr, ".word") == 0) {
                } else if (strcmp(instr, ".skip") == 0) {
                } else if (strcmp(instr, ".ascii") == 0) {
                } else if (strcmp(instr, ".equ") == 0) {
                } else if (strcmp(instr, ".end") == 0) {
                } else {
                    fprintf(stderr, "Unknown directive '%s' at line %d\n", instr, linenum);
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
        }

        // Free memory


        // Read instruction and arguments
        //printf("\e[34m%s\e[0m", line);
        //printf("%*c", 50 - strlen(line), ' ');
        //if (regexec(&re_instr, line, ARR_SIZE(matches), matches, 0) == 0) {
        //    for (int i = 0; matches[i].rm_so != -1; i++) {
        //        regoff_t start = matches[i].rm_so;
        //        regoff_t end = matches[i].rm_eo;
        //        regoff_t len = end - start;
        //        printf("#%d: '%.*s' %*c", i, len, line + start, 20 - len, ' ');
        //    }
        //}
        //printf("\n");


        //printf("\e[34m%s\e[0m", line);
        //printf("%*c", 50 - strlen(line), ' ');
        //char *cursor = 
        //if (regexec(&regex, line, ARR_SIZE(matches), matches, 0) == 0) {
        //    for (int i = 0; matches[i].rm_so != -1; i++) {
        //        int start = matches[i].rm_so;
        //        int end = matches[i].rm_eo;
        //        int len = end - start;
        //        printf("#%d: %.*s %*c", i, len, line + start, 10 - len, ' ');
        //    }
        //} else {

        //}

    }
    free(line);
}

int ss_asmw(FILE *input, FILE *output, int flags)
{

    // Elf 
    Elf64_Ehdr elf_header;

    buf_t *buf_sections;

    // Compile asm regex
    regex_t regex;
    regmatch_t matches[4];
    // Regex: ^\s*(a-zA-Z0)
    if (regcomp(&regex, "^\\s*([a-zA-Z]*:)?\\s*(\\.?[a-zA-Z]*)*\\s*$", REG_NEWLINE | REG_EXTENDED)) {
        perror("Error");
        return 0;
    }

    char *curr_section = strdup(".text");
    char *line = NULL;
    ssize_t len;
    ssize_t nread;
    int linenum = 0;
    int asmerr = 0;
    // Read line by line
    while (! asmerr && (nread = getline(&line, &len, input)) != -1) {
        linenum += 1;
        line[strlen(line) - 1] = '\0';
        char *curs = line;
        char *label = NULL;
        char *direc = NULL;
        char *instr = NULL;
        char *args[MAX_ASM_ARGS];
        bzero(args, MAX_ASM_ARGS * sizeof(char*));

        int argnum = 0;

        // Skip spaces and read label
        skip_spaces(&curs);
        if (is_comment(curs)) {
            continue;
        }

        if (is_label(curs))  {
            read_label(&curs, &label);
            skip_spaces(&curs);
        }

        // Read directive (starts with a .)
        if (is_directive(curs)) {
            // Read directive
            read_directive(&curs, &direc);
            read_arguments(&curs, args, &argnum);
            skip_spaces(&curs);
            if (direc == NULL) {
                fprintf(stderr, "Error reading directive at line %d:\n\t%s\n\n", linenum, line);
            } else if (strcmp(direc, ".global") == 0) {
            } else if (strcmp(direc, ".extern") == 0) {
            } else if (strcmp(direc, ".section") == 0) {
            } else if (strcmp(direc, ".word") == 0) {
            } else if (strcmp(direc, ".skip") == 0) {
            } else if (strcmp(direc, ".ascii") == 0) {
            } else if (strcmp(direc, ".equ") == 0) {
            } else if (strcmp(direc, ".end") == 0) {
            } else {
                fprintf(stderr, "Unknown directive '%s' at line %d\n", direc, linenum);
            }
            free(direc);
            direc = NULL;
        } else if (is_instruction(curs)) {
            // Read instruction
            read_instruction(&curs, &instr);
            skip_spaces(&curs);

            // Figure out what to do with the instruction
            if (instr == NULL) {
                fprintf(stderr, "Error reading instruction at line %d:\n\t%s\n\n", linenum, line);
            } else if (strcmp(instr, "halt") == 0) {
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
                fprintf(stderr, "Unknown instruction at line %d\n", linenum);
            }
            
            free(instr);
            instr = NULL;
        } 


        // Cleanup
        if (label) {
            free(label);
        }

        if (direc) {
            free(direc);
        }

        if (instr) {
            free(instr);
        }


        //printf("\e[34m%s\e[0m", line);
        //printf("%*c", 50 - strlen(line), ' ');
        //char *cursor = 
        //if (regexec(&regex, line, ARR_SIZE(matches), matches, 0) == 0) {
        //    for (int i = 0; matches[i].rm_so != -1; i++) {
        //        int start = matches[i].rm_so;
        //        int end = matches[i].rm_eo;
        //        int len = end - start;
        //        printf("#%d: %.*s %*c", i, len, line + start, 10 - len, ' ');
        //    }
        //} else {

        //}
        //printf("\n");
    }

    // Free memory
    regfree(&regex);
    free(line);
    free(curr_section);
    return 0;
}


int main(int argc, char *argv[])
{
    int i = 0;
    FILE *input = NULL;
    FILE *output = NULL;

    /* Set input and output */
    for (i = 1; i < argc; i++) {
        char *option = argv[i];
        if (strcmp(option, "-o") == 0) {
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

    ss_asm(input, output, F_DEBUG);

    /* Close files */
    if (input != stdin) 
        fclose(input);

    if (output != stdout)
        fclose(output);
    return 0;
}
