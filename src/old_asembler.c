
void _Asm_Compile(Elf_Builder *elf, FILE *input, int flags)
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
                        rela->r_offset = Elf_GetSectionSize(elf);
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
                    } else if (Str_RegMatch(args[0], "^[*]\\[(r[0-9])[+]([^0-9][._a-zA-Z0-9]+)]$", ARR_SIZE(matches), matches)) {
                        /* *[ <reg> + <symbol> ] */

                        // Parse register
                        char *reg_str = Str_Substr(args[0], matches[1].rm_so, matches[1].rm_eo);
                        ai.ai_rd = 0xf;
                        ai.ai_rs = Asm_ParseRegOperand(reg_str);
                        free(reg_str);

                        // Get symbol name
                        char *sym_name= Str_Substr(args[0], matches[2].rm_so, matches[2].rm_eo);
                        printf("'%s'\n", sym_name);

                        // Add rela
                        int rela_type = (*args[0] == '%') 
                            ? R_X86_64_PC16 
                            : R_X86_64_16;
                        Elf_PushSection(elf);
                        Elf_Rela *rela = Elf_AddRelaSymb(elf, sym_name);
                        rela->r_offset = Elf_GetSectionSize(elf);
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
                        ai.ai_am = (*args[1] == '$') 
                            ? ASM_AM_IMMED 
                            : ASM_AM_MEMORY; 
                        ai.ai_up = 0x0; // no change

                        // Get symbol name
                        char *sym_name= Str_Substr(args[1], matches[1].rm_so, matches[1].rm_eo);

                        // Add rela
                        Elf_PushSection(elf);
                        Elf_Rela *rela = Elf_AddRelaSymb(elf, sym_name);
                        rela->r_offset = Elf_GetSectionSize(elf);
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
                        rela->r_offset = Elf_GetSectionSize(elf);
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

