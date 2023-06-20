                /*
                 * 
                 */
                int gprS = -1;
                int gprD = -1;
                if (Str_RegexMatch(other, "[ \t]*%(pc|sp|r[0-9]+),[ \t]*%(pc|sp|r[0-9]+)", ARR_SIZE(matches), matches)) {
                    char *reg1 = Str_Substr(other, matches[1].rm_so, matches[1].rm_eo);
                    char *reg2 = Str_Substr(other, matches[2].rm_so, matches[2].rm_eo);
                    gprS = Asm_ParseRegOperand(reg1);
                    gprD = Asm_ParseRegOperand(reg2);
                    if (gprS != -1 || gprD != -1) {
                        Asm_Instr asm_instr;
                        asm_instr.ai_instr = 
                              Str_Equals(instr, "add") ? 0x50  
                            : Str_Equals(instr, "sub") ? 0x51 
                            : Str_Equals(instr, "mul") ? 0x52 
                            : Str_Equals(instr, "div") ? 0x53 
                            : Str_Equals(instr, "and") ? 0x60 
                            : Str_Equals(instr, "or")  ? 0x61 
                            : Str_Equals(instr, "xor") ? 0x62 
                            : Str_Equals(instr, "shr") ? 0x70 
                            : Str_Equals(instr, "shl") ? 0x71 
                            : 0;
                        asm_instr.ai_ra = gprS;
                        asm_instr.ai_rb = gprS;
                        asm_instr.ai_rc = gprD;
                        Elf_PushBytes(elf, &asm_instr, sizeof(asm_instr));
                    } else {
                        Show_Error("Line %d: Invalid register", linenum, instr);
                    }


                    free(reg1);
                    free(reg2);
                } else {
                    Show_Error("Line %d: `%s %s` - Invalid instruction arguments", linenum, instr, other);
                }

            int opOK = TRUE;
            int opALU =
                  Str_Equals(instr, "add") ? 0x50
                : Str_Equals(instr, "sub") ? 0x51
                : Str_Equals(instr, "mul") ? 0x52
                : Str_Equals(instr, "div") ? 0x53
                : Str_Equals(instr, "and") ? 0x61
                : Str_Equals(instr, "or")  ? 0x62
                : Str_Equals(instr, "xor") ? 0x63
                : Str_Equals(instr, "shl") ? 0x70
                : Str_Equals(instr, "shr") ? 0x71
                : -1;
            int opNOT = Str_Equals(instr, "not") ? 0x60
                : -1;
            int opXCHG = Str_Equals(instr, "xchg") ? 0x40
                : -1;
            int opHALT = Str_Equals(instr, "halt") ? 0x00
                : -1;
            int opINT = Str_Equals(instr, "int") ? 0x00
                : -1;
            int opPUSH = Str_Equals(instr, "push") ? 1
                : -1;
            int opPOP = Str_Equals(instr, "pop") ? 1
                : -1;
            int opBRANCH = Str_Equals(instr, "beq") ? 1
                : Str_Equals(instr, "bne") ? 1
                : Str_Equals(instr, "bgt") ? 1
                : -1
            ;
            int opJMP = Str_Equals(instr, "jmp") ? 1
                : -1;
            int opLD = Str_Equals(instr, "ld") ? 1 
                : -1;
            int opST = Str_Equals(instr, "st") ? 1 
                : -1;

                if (opALU != -1 || opXCHG != -1)  {
                    // Two register operands
                    gprS = Asm_ParseRegOperand(args[0]);
                    gprD = Asm_ParseRegOperand(args[1]);
                    if (gprS == -1  || gprD == -1)  {
                        Show_Error("Line %d: Invalid register operand for instruction '%s' (r0-r15,sp,pc):\n %s", linenum, instr, line);
                        opOK = FALSE;
                    }
                } else if (opNOT != -1 || opPUSH != -1 || opPOP != -1) {
                    gprD = Asm_ParseRegOperand(args[0]);
                    if (gprD == -1  || gprD == -1)  {
                        Show_Error("Line %d: Invalid register oprand for instruction '%s' (r0-r15,sp,pc):\n %s", linenum, instr, line);
                        opOK = FALSE;
                    }
                } else if (opBRANCH != -1)  {
                    if (argc == 3) {
                        gprS = Asm_ParseRegOperand(args[0]);
                        gprD = Asm_ParseRegOperand(args[1]);
                    } else {
                        Show_Error("Line %d: Wrong number of arguments for instruction '%s':\n %s", linenum, instr, line);
                        opOK = FALSE; 
                    }
                }

                /*
                if (Str_Equals(instr, "halt")) {
                    Elf_PushByte(elf, 0x00);
                    Elf_PushByte(elf, 0x00);
                    Elf_PushByte(elf, 0x00);
                    Elf_PushByte(elf, 0x00);
                } else if (opINT != -1) {
                    Elf_PushByte(elf, 0x10);
                    Elf_PushByte(elf, 0x00);
                    Elf_PushByte(elf, 0x00);
                    Elf_PushByte(elf, 0x00);
                } else if (opXCHG != -1) {
                    Elf_PushByte(elf, 0x40);
                    Elf_PushByte(elf, gprS);
                    Elf_PushByte(elf, gprD << 4);
                    Elf_PushByte(elf, 0x00);
                } else if (opALU != -1) {
                    Elf_PushByte(elf, 0x50);
                    Elf_PushByte(elf, (gprS << 4) | (gprS));
                    Elf_PushByte(elf, gprD << 4);
                    Elf_PushByte(elf, 0x00);
                } else if (opNOT != -1) {
                    Elf_PushByte(elf, opNOT);
                    Elf_PushByte(elf, (gprD << 4) | gprD);
                    Elf_PushByte(elf, 0x00);
                    Elf_PushByte(elf, 0x00);
                } else if (opLD != -1) {
                } else {
                    Show_Error("Line %d: Unknown instruction `%s`:\n %s", linenum, instr, line);
                }
                */

            if (Str_Equals(direc, ".global") || Str_Equals(direc, ".globl")) {
                argc = Asm_ExtractArguments(other, args, ARR_SIZE(args));
                if (argc > 0) {
                    for (int i = 0; i < argc; i++) {
                        Elf_Sym *sym = Elf_UseSymbol(elf, args[i]);
                        sym->st_info = ELF_ST_INFO(STB_GLOBAL, ELF_ST_TYPE(sym->st_info));
                        printf("|%s|\n", args[i]);
                    }
                } else {
                    Show_Error("Line %d: Directive `.global` requires at least one argument:\n %s", linenum, line);
                }
            } else if (Str_Equals(direc, ".extern")) {
                /*
                 * Do nothing
                 */
            } else if (Str_Equals(direc, ".section")) {
                char *name = other;
                Elf_UseSection(elf, name);
            } else if (Str_Equals(direc, ".word")) {
                int value = Str_ParseInt(other);
                Elf_PushByte(elf, value);
            } else if (Str_Equals(direc, ".byte")) {
            } else if (Str_Equals(direc, ".skip")) {
                int value = Str_ParseInt(other);
                Elf_PushSkip(elf, value, 0x00);
            } else if (Str_Equals(direc, ".ascii")) {
                Elf_PushString(elf, other);
            } else if (Str_Equals(direc, ".equ")) {
            } else if (Str_Equals(direc, ".end")) {
            }





// Call instruction
void Asm_PushCall(Elf_Builder *elf, int mod, int ra, int rb, int disp)
{
    unsigned char code[] = {
        PACK(OC_CALL, mod),
        PACK(ra, rb),
        PACK(0, (disp >> 8) & 0xf),
        disp & 0xff
    };
    Elf_PushBytes(elf, code, sizeof(code));
}

void Asm_PushBranch(Elf_Builder *elf, int mod, int ra, int rb, int rc, int disp)
{
    unsigned char code[] = {
        PACK(OC_BRANCH, mod),
        PACK(ra, rb),
        PACK(rc, (disp >> 8) & 0xf),
        disp & 0xff
    };
    Elf_PushBytes(elf, code, sizeof(code));
}




void Test1(Elf_Builder *elf)
{
    int value = 0xcafebabe;
    unsigned char code[] = {
        0x91, 0x12, 0x00, 0x15,                     // r1 = 0x5

        // ld $cafebabe, %r0
        //0x81, 0xee, 0x10, 0x04,                     // push r1
        //0x63, 0x11, 0x10, 0x00,                     // r1 = 0x0
        //0x91, 0x11, 0x00, 0x08,                     // r1 = 0x8
        //0x63, 0x00, 0x00, 0x00,                     // r0 = 0
        //0x91, 0x00, 0x00, ((value >> 24) & 0xff),   // r0 += 0xff
        //0x70, 0x00, 0x10, 0x00,                     // r0 <<= r1
        //0x91, 0x00, 0x00, ((value >> 16) & 0xff),   // r0 += 0xff
        //0x70, 0x00, 0x10, 0x00,                     // r0 <<= r1
        //0x91, 0x00, 0x00, ((value >> 8) & 0xff),    // r0 += 0xff
        //0x70, 0x00, 0x10, 0x00,                     // r0 <<= r1
        //0x91, 0x00, 0x00, (value & 0xff),           // r0 += 0xff
        //0x93, 0x1e, 0xef, 0xfc,                     // pop r1

        // ld $cafebabe, %sp


        //0x00, 0x00, 0x00, 0x00,                     // halt

    };

    Elf_PushSection(elf);
    Elf_UseSection(elf, ".fdsa");
    Elf_Shdr *shdr = Elf_GetSection(elf);
    shdr->sh_addr = 0x40000000;
    Elf_UseSymbol(elf, Elf_GetSectionName(elf))->st_value = shdr->sh_addr;

    // Add code
    //Elf_PushBytes(elf, code, sizeof(code));

    // Add more code
    //Asm_PushLoadByte(elf, 2, 8);
    //Asm_PushLoadByte(elf, 0, 0xff);
    //Asm_PushLoadWord(elf, 4, 0xcafebabe, 13, 0);
    //Asm_PushInstr(elf, OC_SHIFT, MOD_SHIFT_LEFT, 0, 0, 2, 0);


    // Create a symbol with some value
    {
        Elf_Sym *sym = Elf_UseSymbol(elf, "hi");
            // sym->st_shndx = Elf_GetCurrentSection(elf);
        sym->st_shndx = SHN_ABS;
        sym->st_value = 0xdeadbeef;
    }

    // Push some random instructions
    {
        Asm_PushLoadByte(elf, 3, 0xff);
        Asm_PushLoadByte(elf, 3, 0xff);
        Asm_PushLoadByte(elf, 3, 0xff);
        Asm_PushLoadByte(elf, 3, 0xff);
        Asm_PushLoadByte(elf, 3, 0xff);
        Asm_PushLoadByte(elf, 3, 0xff);
    }

    // Create a new section that will be used for offset in the second rela
    {
        Elf_PushSection(elf);
        Elf_Shdr *shdr = Elf_UseSection(elf, ".data");
        shdr->sh_addr = 0xcc000000;

        // Push word and add a symbol for it
        {
            Elf_Sym * sym = Elf_UseSymbol(elf, "value1");
            sym->st_shndx = Elf_GetCurrentSection(elf);
            sym->st_value = Elf_GetSectionSize(elf);
            sym->st_size = 4;
            Elf_PushWord(elf, 0x33333333);
        }
        {
            Elf_Sym * sym = Elf_UseSymbol(elf, "value2");
            sym->st_shndx = Elf_GetCurrentSection(elf);
            sym->st_value = Elf_GetSectionSize(elf);
            sym->st_size = 4;
            Elf_PushWord(elf, 0x44444444);
        }
        {
            Elf_Sym * sym = Elf_UseSymbol(elf, "value3");
            sym->st_shndx = Elf_GetCurrentSection(elf);
            sym->st_value = Elf_GetSectionSize(elf);
            sym->st_size = 4;
            Elf_PushWord(elf, 0x55555555);
        }


        Elf_PopSection(elf);
    }

    // Add instruction and rela to the symbol
    {
        Elf_Rela *rela = Elf_AddRelaSymb(elf, "hi");
        rela->r_offset = Elf_GetSectionSize(elf);
        rela->r_info   = ELF_R_INFO(ELF_R_SYM(rela->r_info), R_SS_LD32);
        rela->r_addend = 0;
        Asm_PushLoadWord(elf, 5, 0xeeeeeeee, 13, 0);
    }

    {
        Elf_Rela *rela = Elf_AddRelaSymb(elf, "value3");
        rela->r_offset = Elf_GetSectionSize(elf);
        rela->r_info   = ELF_R_INFO(ELF_R_SYM(rela->r_info), R_SS_LD32);
        rela->r_addend = 0;
        Asm_PushLoadWord(elf, 4, 0, 13, 0);
    }
    Elf_PopSection(elf);
}

void Test2(Elf_Builder *elf)
{
    // my_handler
    {
        Elf_PushSection(elf);
        // This is how you open a function
        Elf_Shdr *shdr = Elf_UseSection(elf, "my_handler");
        shdr->sh_addr = 0xff00ff00;;
        // This is how you add a label
        Elf_Sym *sym_handler = Elf_UseSymbol(elf, "handler");
        sym_handler->st_shndx = Elf_GetCurrentSection(elf);
        sym_handler->st_value = Elf_GetSectionSize(elf);


        // push %r1
        Asm_PushStackPush(elf, 1);

        Elf_PopSection(elf);
    }

    // my_code
    {
        Elf_PushSection(elf);
        Elf_Shdr *shdr = Elf_UseSection(elf, "my_start");
        shdr->sh_addr = 0x40000000;

        Asm_PushLoadWord(elf, 14, 0xfffffefe, 13, 0);
        Asm_PushLoadWord(elf, 0, 0xdadadede, 13, 0);
        Asm_PushStackPush(elf, 0);
        Asm_PushLoadByte(elf, 0, 0xff);
        Asm_PushStackPop(elf, 0);
        //Asm_PushInstr(elf, OC_XCHG, MOD_NONE, 0, 12, SP, 0);


        Elf_PopSection(elf);
    }
}


