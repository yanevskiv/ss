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










void Test3(Elf_Builder *elf) 
{
    // my_data
    {
        Elf_PushSection(elf);
        Asm_AddAbsSymbol(elf, "fuck", 0x40);
        Elf_Shdr *shdr = Elf_UseSection(elf, "my_data");
        shdr->sh_addr = 0xcccc0000;
        Asm_AddLabel(elf, "value1");
        Elf_PushWord(elf, 0);
        Asm_AddLabel(elf, "value2");
        Elf_PushWord(elf, 0);
        Asm_AddLabel(elf, "value3");
        Elf_PushWord(elf, 0x31441);
        Asm_AddLabel(elf, "value4");
        Elf_PushWord(elf, 0);
        Asm_AddLabel(elf, "value5");
        Elf_PushWord(elf, 0);
        Asm_AddLabel(elf, "value6");
        Elf_PushWord(elf, 0);
        Elf_PopSection(elf);
    }

    // math
    {
        Elf_PushSection(elf);
        Elf_Shdr *shdr = Elf_UseSection(elf, "math");
        shdr->sh_addr = 0xf0000000;

        // mathAdd function
        Asm_AddLabel(elf, "mathAdd");
        Asm_PushStackPush(elf, REG2);
        Asm_PushLoadMemRegDisp(elf, SP, 0x08, REG1);
        Asm_PushLoadMemRegDisp(elf, SP, 0x0c, REG2);
        Asm_PushAdd(elf, REG2, REG1);
        Asm_PushStackPop(elf, REG2);
        Asm_PushRet(elf);

        // mathSub function
        Asm_AddLabel(elf, "mathSub");
        Asm_PushStackPush(elf, REG2);
        Asm_PushLoadMemRegDisp(elf, SP, 0x08, REG1);
        Asm_PushLoadMemRegDisp(elf, SP, 0x0c, REG2);
        Asm_PushSub(elf, REG2, REG1);
        Asm_PushStackPop(elf, REG2);
        Asm_PushRet(elf);

        Asm_AddLabel(elf, "loadCe");
        Asm_PushLoadByte(elf, REG1, 0xcb);
        Asm_PushStackPop(elf, PC);

        Asm_AddLabel(elf, "handler");
        Asm_PushLoadByte(elf, REG8, 0x77);
        Asm_PushLoadHalf(elf, REG5, 0xabcd, ADR);
        Asm_PushIret(elf);

        Asm_AddLabel(elf, "func");
        Asm_PushLoadByte(elf, REG0, 0);  // 
        Asm_PushLoadByte(elf, REG1, 0);
        Asm_PushLoadByte(elf, REG2, 1);
        Asm_PushLoadWord(elf, REG3, 5, IDX);
        Asm_AddLabel(elf, "loop");
        Asm_PushAdd(elf, REG2, REG0);
        Asm_PushAdd(elf, REG2, REG1);
        Asm_PushBneSymbol(elf, REG1, REG3, "loop");
        Asm_PushRet(elf);

        // Expects the address of the string to be stored in %r5
        /*
            r5 <= startAddr
            -------
            r0 <= 0
            r1 <= 1
            r2 <= r5
            r4 <= TEMR_OUT
            r6 <= 0xff

            loop:
                r3 <= mem32[r2 + 0]
                r3 <= r3 & r6
                r2 <= r2 + r1
                beq r3, r0, out
                mem32[r4] <= r3
                jmp loop
            out:
                ret
                

        */
        Asm_AddLabel(elf, "printStr");
        Asm_PushLoadByte(elf, REG0, 0);
        Asm_PushLoadByte(elf, REG1, 1);
        Asm_PushLoadReg(elf, REG5, REG2);
        Asm_PushLoadWord(elf, REG4, EMU_TERM_OUT, IDX);
        Asm_PushLoadByte(elf, REG6, 0xff);
        Asm_AddLabel(elf, "loopStr");
            Asm_PushLoadMemRegDisp(elf, REG2, 0, REG3);
            Asm_PushAnd(elf, REG6, REG3);
            Asm_PushAdd(elf, REG1, REG2);
            Asm_PushBeqSymbol(elf, REG3, REG0, "outSTr");
            Asm_PushStoreMemRegDisp(elf, REG3, REG4, 0);
            Asm_PushJmpSymbol(elf, "loopStr");
        Asm_AddLabel(elf, "outStr");
        Asm_PushRet(elf);


        Elf_PopSection(elf);
        Elf_PushSection(elf);
        Elf_Shdr *shdr2 = Elf_UseSection(elf, "strings");
        shdr2->sh_addr = 0x33330000;
        Asm_AddLabel(elf, "hello");
        Elf_PushString(elf, "Hello world! This is some string nig");
        Asm_AddLabel(elf, "world");
        Elf_PushString(elf, "This is some other string\n");
        Elf_PopSection(elf);

    }

    Mem_WriteWord(0xee00ee00, 0x12345678);

    // my_start
    {
        Elf_PushSection(elf);
        Elf_Shdr *shdr = Elf_UseSection(elf, "my_start");
        shdr->sh_addr = 0x40000000;

        Asm_PushLoadLiteralValue(elf, 0xfffffefe, SP);

        Asm_PushLoadSymbolValue(elf, "handler", REG1);
        Asm_PushCsrwr(elf, REG1, HANDLE);

        Asm_PushStoreSymbolAddr(elf, REG1, "value1");
        Asm_PushLoadMemSymbolValue(elf, "value1", REG7);
        Asm_PushInt(elf);
        Asm_PushLoadByte(elf, REG9, 0x88);

        Asm_PushCallSymbol(elf, "func");
        Asm_PushLoadSymbolValue(elf, "mathSub", REG0);
        Asm_PushCallSymbol(elf, "mathAdd");

        Asm_PushLoadByte(elf, REG3, 'c');
        Asm_PushStoreAddr(elf, REG3, EMU_TERM_OUT);


        // Print string
        Asm_PushLoadSymbolValue(elf, "world", REG5);
        Asm_PushCallSymbol(elf, "printStr");

        Asm_PushHalt(elf);




        //Asm_PushLoadByte(elf, REG2, 0xfe);

        //Asm_PushLoadByte(elf, REG1, 9);
        //Asm_PushStackPush(elf, REG1);
        //Asm_PushLoadByte(elf, REG1, 2);
        //Asm_PushStackPush(elf, REG1);
        //Asm_PushCallSymbol(elf, "mathAdd");

        // Asm_PushLoadByte(elf, REG0, 1);   // Sum
        // Asm_PushLoadByte(elf, REG1, 0);   // iterator
        // Asm_PushLoadByte(elf, REG2, 1);   // increment
        // Asm_PushLoadByte(elf, REG3, 10); // Limit
        // Asm_AddLabel(elf, "loop");
        // Asm_PushAdd(elf, REG2, REG1);
        // Asm_PushAdd(elf, REG1, REG0);
        // Asm_PushBneSymbol(elf, REG1, REG3, "loop");

        // Asm_PushLoadSymbol(elf, REG8, "loop");
        // Asm_PushLoadMemSymbol(elf, REG9, "loop");
        // Asm_PushBgtSymbol(elf, REG3, REG1, "loop");

        Elf_PopSection(elf);
    }
}

void Test4(Elf_Builder *elf)
{
    // my code
    Elf_Builder elf1, elf2;
    Elf_Init(&elf1);
    Elf_Init(&elf2);
    {
        Elf_PushSection(&elf1);
        Elf_UseSection(&elf1, "my_code");
        Asm_PushLoadByte(&elf1, REG0, 'A');
        Asm_PushStoreAddr(&elf1, REG0, EMU_TERM_OUT);

        Asm_PushCallSymbol(&elf1, "funny");
        Elf_PopSection(&elf1);
    }
    
    // functions
    {
        Elf_PushSection(&elf2);
        Elf_UseSection(&elf2, "functions");
        Asm_AddLabel(&elf2, "funny");
        Asm_PushLoadByte(&elf2, REG0, 0x0);
        Asm_PushLoadByte(&elf2, REG1, 0x1);
        Asm_PushLoadByte(&elf2, REG2, 26);
        Asm_PushLoadByte(&elf2, REG3, 0);
        Asm_PushLoadWord(&elf2, REG4, EMU_TERM_OUT, ADR);
        Asm_PushLoadByte(&elf2, REG5, 'a');
        Asm_AddLabel(&elf2, "loop");
        Asm_PushStoreMemRegDisp(&elf2, REG5, REG4, 0);
        Asm_PushAdd(&elf2, REG1, REG3);
        Asm_PushAdd(&elf2, REG1, REG5);
        Asm_PushBneSymbol(&elf2, REG2, REG3, "loop");
        Asm_PushRet(&elf2);
        Elf_PopSection(&elf2);
    }

    Elf_Link(elf, &elf1);
    Elf_Link(elf, &elf2);
    
    // place
    {
        Elf_PushSection(elf);
        Elf_Shdr *shdr = Elf_UseSection(elf, "my_code");
        shdr->sh_addr = 0x40000000;
        Elf_PopSection(elf);
    }
    {
        Elf_PushSection(elf);
        Elf_Shdr *shdr = Elf_UseSection(elf, "functions");
        shdr->sh_addr = 0xffff0000;
        Elf_PopSection(elf);
    }
}






/*
                     * HALT and INT instructions
                     * opcodes:
                     *  halt: 0x00
                     *  int:  0x10
                     *
                     *           byte1      byte2       byte3      byte4
                     * Format: [ opcode | 0000 0000 | 0000 0000 | 0000 0000 ]
                     */
                    case I_HALT: 
                    case I_INT:
                    {
                        Elf_PushByte(elf, opCode);
                        Elf_PushByte(elf, 0x00);
                        Elf_PushByte(elf, 0x00);
                        Elf_PushByte(elf, 0x00);
                    } break;

                    case I_CALL: {
                        // TODO
                    } break;

                    case I_RET: {
                        // TODO
                    } break;



                    /*
                     * XCHG instruction
                     * opcodes:
                     *  xchg: 0x40
                     * 
                     *           byte1      byte2       byte3      byte4
                     * Format: [ opcode | 0000 0000 | 0000 0000 | 0000 0000 ]
                     */
                    case I_XCHG:
                    {
                        Elf_PushByte(elf, opCode);
                        Elf_PushByte(elf, (gprS << 0));
                        Elf_PushByte(elf, (gprD << 4));
                        Elf_PushByte(elf, 0x00);
                    } break;

                    /*
                     * ALU instructions
                     * opcodes:
                     *  add: 0x50
                     *  sub: 0x51
                     *  mul: 0x52
                     *  div: 0x53
                     *  and: 0x61
                     *  or:  0x62
                     *  xor: 0x63
                     *  shl: 0x70
                     *  shr: 0x71
                     *
                     *            byte1      byte2       byte3       byte4
                     * Format:  [ opcode | AAAA BBBB | CCCC 0000 | 0000 0000 ]
                     * Meaning: gpr[A] <= gpr[B] `OP` gpr[C]
                     */
                    // Format:
                    case I_ADD:
                    case I_SUB:
                    case I_MUL:
                    case I_DIV:
                    case I_AND: 
                    case I_OR: 
                    case I_XOR: 
                    case I_SHL: 
                    case I_SHR: 
                    {
                        Elf_PushByte(elf, opCode);
                        Elf_PushByte(elf, (gprS << 4) | (gprS << 0));
                        Elf_PushByte(elf, (gprD << 4));
                        Elf_PushByte(elf, 0x00);
                    } break;

                    /*
                     * NOT instruction
                     * opcodes:
                     *  not: 0x60
                     *
                     *            byte1      byte2       byte3       byte4
                     * Format:  [ opcode | AAAA BBBB | CCCC 0000 | 0000 0000 ]
                     * Meaning: gpr[A] <= gpr[B] `OP` gpr[C]
                     * C is irrelevant here
                     */
                    case I_NOT:
                    {
                        Elf_PushByte(elf, opCode);
                        Elf_PushByte(elf, (gprD << 4) | (gprD << 0));
                        Elf_PushByte(elf, 0x00);
                        Elf_PushByte(elf, 0x00);
                    } break;


                    case I_PUSH: {
                        Elf_PushByte(elf, opCode);
                        Elf_PushByte(elf, (SP << 4));
                        Elf_PushByte(elf, gprD);
                        Elf_PushByte(elf, 0x04);
                    } break;

                    case I_POP: {
                        Elf_PushByte(elf, opCode);
                        Elf_PushByte(elf, gprD << 4 | SP);
                        Elf_PushByte(elf, 0x0f);
                        Elf_PushByte(elf, 0xfc);
                    } break;

                    case I_LD: {
                        // TODO
                    } break;

                    case I_ST: {
                        // TODO
                    } break;


                    /*
                     * CSRWR and CSRRD instructions
                     * opcodes:
                     *  csrrd: 0x90
                     *  csrwr: 0x94
                     *
                     *            byte1      byte2       byte3       byte4
                     * Format:  [ opcode | AAAA BBBB | CCCC DDDD | DDDD DDDD ]
                     * Meaning:
                     *   gpr[A] <= csr[B] if opcode = 0x90
                     *   csr[A] <= gpr[B] if opcode = 0x94
                     * C and D are irrelevant here
                     */
                    case I_CSRWR: 
                    case I_CSRRD: {
                        Elf_PushByte(elf, opCode);
                        Elf_PushByte(elf, (gprD << 4) | (csr << 0));
                        Elf_PushByte(elf, 0x00);
                        Elf_PushByte(elf, 0x00);
                    } break;


                //Elf_Rela *rela = Elf_AddRelaSymb(elf, args[i]);
                //int relaType = 
                //      direcId == D_BYTE ? R_X86_64_8
                //    : direcId == D_HALF ? R_X86_64_16
                //    : direcId == D_WORD ? R_X86_64_32
                //    : R_X86_64_NONE;
                //rela->r_offset = Elf_GetSectionSize(elf);
                //rela->r_info = ELF_R_INFO(ELF_R_SYM(rela->r_info), relaType);
                //rela->r_addend = 0;








    for (int i = 1; i <= Elf_GetSectionCount(src); i++) {
        Elf_PushSection(src);
        Elf_SetSection(src, i);
        Elf_Shdr *shdr = Elf_GetSection(src);
        switch (shdr->sh_type) {
            case SHT_STRTAB: {
                Elf_Word off = 1;
                while (off < Elf_GetSectionSize(src))  {
                    char *str = Elf_GetSectionData(src) + off;
                    Elf_AddUniqueString(dest, str);
                    off += strlen(str) + 1;
                }
            } break;
            case SHT_SYMTAB: {
                int i, size = shdr->sh_size / sizeof(Elf_Sym);
                for (i = 1; i < size; i++) {
                    Elf_Sym *sym = Elf_GetSymbol(src, i);
                    const char *sym_name = Elf_GetSymbolName(src, i);
                    if (ELF_ST_TYPE(sym->st_info) == STT_SECTION) {
                        Elf_PushSection(dest);
                        Elf_UseSection(dest, sym_name);
                        Elf_PopSection(dest);
                    } else {
                        // Create or fetch symbol in the destination
                        //printf("Loading - %s\n", sym_name);
                        Elf_Sym *dest_sym = Elf_UseSymbol(dest, sym_name);

                        // Copy symbol only if it's undefined in the destination
                        if (dest_sym->st_shndx == SHN_UNDEF) {
                            // Copy symbol
                            //  - st_name is set by Elf_UseSymbol()
                            //  - st_shndx will depend on which section (if any) the src symbol belongs to
                            // dest_sym->st_name - Set by Elf_UseSymbol()
                            dest_sym->st_info = sym->st_info;
                            dest_sym->st_other = sym->st_other;
                            // dest_sym->st_shndx - Set below
                            dest_sym->st_value = sym->st_value;
                            dest_sym->st_size  = sym->st_size;

                            // Import symbol to destination
                            if (sym->st_shndx == SHN_UNDEF || sym->st_shndx == SHN_ABS) {
                                // Undefined or absolute symbol
                                dest_sym->st_shndx = sym->st_shndx;
                                //printf("Importing %s %d\n", sym_name, dest_sym->st_shndx);
                            } else {
                                // Label symbol
                                // Get symbol section name in the source
                                Elf_PushSection(src);
                                Elf_SetSection(src, sym->st_shndx);
                                const char *section_name = Elf_GetSectionName(src);
                                Elf_PopSection(src);

                                // Import symbol
                                Elf_PushSection(dest);
                                Elf_UseSection(dest, section_name);
                                dest_sym->st_shndx = Elf_GetCurrentSection(dest);
                                Elf_PopSection(dest);
                            }
                        }
                    }
                }
            } break;
            case SHT_RELA: {
                // Initialize rela section
                Elf_PushSection(dest);
                Elf_UseSection(dest, Elf_GetSectionName(src));
                const char *shName = Elf_GetSectionName(src) + strlen(".rela");
                Elf_GetSection(dest)->sh_type = SHT_RELA;
                Elf_GetSection(dest)->sh_entsize = sizeof(Elf_Rela);
                Elf_PopSection(dest);

                Elf_Word shDestSize = 0;
                if (Elf_SectionExists(dest, shName)) {
                    Elf_PushSection(dest);
                    Elf_UseSection(dest, shName);
                    shDestSize = Elf_GetSectionSize(dest);
                    Elf_PopSection(dest);
                }

                // Add Rela entries
                int i, size = Elf_GetSectionSize(src) / sizeof(Elf_Rela);
                for (i = 0; i < size; i++) {
                    // Get rela and its symbol/symbol name
                    Elf_Rela *rela = Elf_GetSectionEntry(src, i);
                    Elf_Half type = ELF_R_TYPE(rela->r_info);
                    Elf_Word symndx = ELF_R_SYM(rela->r_info);
                    Elf_Sym *sym = Elf_GetSymbol(src, symndx);
                    const char *sym_name = Elf_GetSymbolName(src, symndx);
                    printf("Rela shDestSize: %d symName: %s\n", shDestSize, sym_name);

                    // If symbol is defined, copy value
                    //if (sym->st_shndx != SHN_UNDEF) {
                    // Get symbol and then section name
                    // (Problematic, if symbol doesn't exist)
                    // Ok, I don't need section name at all.
                    // This is legacy stuff
                    //Elf_PushSection(src);
                    //Elf_SetSection(src, sym->st_shndx);
                    //const char *section_name = Elf_GetSectionName(src);
                    //Elf_PopSection(src);

                    // Import symbol if necessary
                    // What we actually want is the new symndx in the `dest`
                    //Elf_PushSection(dest);
                    //Elf_UseSection(dest, section_name);


                    // Create or fetch symbol in the destination
                    Elf_Sym *dest_sym = Elf_UseSymbol(dest, sym_name);
                    // dest_sym->st_name - Set by Elf_UseSymbol()
                    dest_sym->st_info = sym->st_info;
                    dest_sym->st_other = sym->st_other;
                    // dest_sym->st_shndx - Set below = Elf_GetCurrentSection(dest);
                    dest_sym->st_value = sym->st_value;
                    dest_sym->st_size = sym->st_size;

                    // Import symbol to destination
                    if (sym->st_shndx == SHN_UNDEF || sym->st_shndx == SHN_ABS) {
                        dest_sym->st_shndx = sym->st_shndx;
                    } else {
                        // Label symbol
                        // Get symbol section name in the source
                        Elf_PushSection(src);
                        Elf_SetSection(src, sym->st_shndx);
                        const char *section_name = Elf_GetSectionName(src);
                        Elf_PopSection(src);

                        // Import symbol
                        Elf_PushSection(dest);
                        Elf_UseSection(dest, section_name);
                        dest_sym->st_shndx = Elf_GetCurrentSection(dest);
                        Elf_PopSection(dest);
                    }

                    // Add rela to destination
                    Elf_Word dest_symndx = Elf_FindSymbol(dest, sym_name);
                    Elf_Rela dest_rela;
                    // Offset should be offset from the beginning of the section
                    dest_rela.r_offset = /* original_size + */ rela->r_offset;
                    dest_rela.r_info = ELF_R_INFO(dest_symndx, type);
                    dest_rela.r_addend = rela->r_addend;
                    Elf_PushSection(dest);
                    Elf_UseSection(dest, Elf_GetSectionName(src));
                    Elf_PushBytes(dest, &dest_rela, sizeof(dest_rela));
                    Elf_PopSection(dest);
                    //}
                }
            } break;
            default: {
                Elf_PushSection(dest);
                Elf_UseSection(dest, Elf_GetSectionName(src));
                Elf_PushBytes(dest, Elf_GetSectionData(src), Elf_GetSectionSize(src));
                Elf_PopSection(dest);
            } break;
        }
        Elf_PopSection(src);
    }






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

















typedef enum {
    ARG_INVALID,
    ARG_LIT,
    ARG_SYM,
    ARG_MEM_LIT,
    ARG_MEM_SYM,
    ARG_REG,
    ARG_MEM_REG,
    ARG_MEM_REG_LIT_ADD,
    ARG_MEM_REG_LIT_SUB,
    ARG_MEM_REG_SYM_ADD,
    ARG_MEM_REG_SYM_SUB,
    ARG_STR,
    ARG_EXP
} Asm_ArgType;

typedef struct {
    Asm_ArgType am_type;
    char *am_name;
    char *am_regex;
} Asm_ArgMatch; 

typedef struct {
    Asm_ArgType a_type;
    Asm_RegType a_reg;
    Asm_Word    a_lit;
    char *a_sym;
    char *a_str;
    char *a_exp;
} Asm_Arg;


Asm_ArgMatch Asm_ArgMatchTypes[] = {
    { ARG_LIT,             "LIT",             X_LIT             },
    { ARG_SYM,             "SYM",             X_SYM             },
    { ARG_MEM_LIT,         "MEM_LIT",         X_MEM_LIT         },
    { ARG_MEM_SYM,         "MEM_SYM",         X_MEM_SYM         },
    { ARG_REG,             "REG",             X_REG             },
    { ARG_MEM_REG,         "MEM_REG",         X_MEM_REG         },
    { ARG_MEM_REG_LIT_ADD, "MEM_REG_LIT_ADD", X_MEM_REG_LIT_ADD },
    { ARG_MEM_REG_SYM_ADD, "MEM_REG_SYM_ADD", X_MEM_REG_SYM_ADD },
    { ARG_STR,             "STR",             X_STR             },
    //{ ARG_EXP,             "EXP",             X_EXP             }
};


Asm_Arg *Asm_ParseArg(const char *str)
{
    char *matches[MAX_REGEX_MATCHES] = { 0 };
    Asm_Arg *arg = calloc(1, sizeof(Asm_Arg));
    assert(arg != NULL);

    char *buffer = strdup(str);
    Str_Trim(buffer);
    int i, len = strlen(buffer);
    for (i = 0; i < ARR_SIZE(Asm_ArgMatchTypes); i++) {
        Asm_ArgMatch am = Asm_ArgMatchTypes[i];
        if (! Str_RegexMatchStr(buffer, am.am_regex, ARR_SIZE(matches), matches))
            continue;
        switch (am.am_type) {
            case ARG_LIT: {
                arg->a_lit = Str_ParseInt(matches[1]);
            } break;

            case ARG_MEM_LIT: {
                arg->a_lit = Str_ParseInt(matches[1]);
            } break;

            case ARG_MEM_SYM: {
                arg->a_sym = strdup(matches[1]);
            } break;

            case ARG_SYM: {
                arg->a_sym = strdup(matches[1]);
            } break;

            case ARG_REG: {
                arg->a_reg = Asm_ParseRegOperand(matches[1]);
            } break;

            case ARG_MEM_REG: {
                arg->a_reg = Asm_ParseRegOperand(matches[1]);
            } break;
            
            case ARG_MEM_REG_LIT_ADD: {
                arg->a_reg = Asm_ParseRegOperand(matches[1]);
                arg->a_lit = Str_ParseInt(matches[2]);
            } break;

            case ARG_MEM_REG_SYM_ADD: {
                arg->a_reg = Asm_ParseRegOperand(matches[1]);
                arg->a_sym = strdup(matches[2]);
            } break;

            case ARG_EXP: {
                
            } break;
        }

        // done
        arg->a_type = am.am_type;
        printf("%s is %s\n", buffer, am.am_name);
        break;
    }
    if (i == ARR_SIZE(Asm_ArgMatchTypes)) {
        printf("%s is INVALID\n", buffer);
        arg->a_type = ARG_INVALID;
    }
    

    Str_Destroy(buffer);
    return arg;
}

// Free argument
void Asm_DestroyArg(Asm_Arg *arg)
{
    if (arg->a_sym)
        free(arg->a_sym);
    if (arg->a_str)
        free(arg->a_str);
    if (arg->a_exp)
        free(arg->a_exp);
    free(arg);
}


//Elf_Word Elf_GetStringCount(Elf_Buidler *elf, const char *str)
//{
//    Elf_PushSection(elf);
//    Elf_UseSection(elf, ".strtab");
//    Elf_Word start = 0;
//    int found = 0;
//    while ((! found) && start < Elf_GetSectionSize(elf)) {
//        char *check_str = Elf_GetSectionData(elf) + start;
//        size_t len = strlen(check_str);
//        printf("Hello??? %s\n", check_str);
//        if (strcmp(str, check_str) == 0) {
//            found = 1;
//        }
//        start += len + 1;
//    }
//
//    Elf_PopSection(elf);
//    
//}


typedef struct {
    int sb_len;
    int sb_cap;
    char *sb_str;
} Str_Buffer;

Str_Buffer *Str_BufferCreate();
void Str_BufferChar(Str_Buffer*, char);
void Str_BufferDec(Str_Buffer*, int);
void Str_BufferHex(Str_Buffer*, int);
void Str_BufferOct(Str_Buffer*, int);
void Str_BufferStr(Str_Buffer*, const char *);
void Str_BufferDestroy(Str_Buffer*);



Str_Buffer *Str_BufferCreate()
{
    Str_Buffer *buffer = (Str_Buffer*)malloc(sizeof(Str_Buffer));
    assert(buffer != NULL);
    buffer->sb_len = 0;
    buffer->sb_cap = 10;
    buffer->sb_str = (char*)calloc(buffer->sb_cap, sizeof(char));
}

void Str_BufferStr(Str_Buffer *buffer, const char *str)
{
}

void Str_BufferDestroy(Str_Buffer *buffer)
{
    free(buffer->sb_str);
    free(buffer);
}





    regmatch_t re[MAX_REGEX_MATCHES];
    char *matches[MAX_REGEX_MATCHES] = { 0 };
    char *string = "fdsafdsa fdsaf rewq rewq";
    int count;
    /*
    if (Str_RegexMatch(string, "\\s+", ARR_SIZE(re), re)) {
        for (int i = 0; i < ARR_SIZE(re); i++) {
            int so = re[i].rm_so;
            int eo = re[i].rm_eo;
            printf("(%d %d) ", so, eo);
        }
        
    }
    printf("%s\n");
    if ((count = Str_RegexSplit(string, "[ ,]", ARR_SIZE(matches), matches)) > 0) {
        printf("Count: '%d' ", count);
        for (int i = 0; i < count; i++) {
            printf("'%s' ", matches[i]);
        }
    }
    */

    char expr[] = "fdsafdsa fdsa fdsa +    rewqr f &&fdsaio+4324";
    if ((count = Str_RegexSplit(expr, X_OPERATOR, ARR_SIZE(matches), matches)) > 0) {
        printf("Count %d:\n", count);
        for (int i = 0; i < count; i++) {
            Str_Trim(matches[i]);
            printf("'%s'\n", matches[i]);
        }
    }
    //int i, count = Str_RegexMatchStr(string, "(<<|>>)", ARR_SIZE(matches), matches);
    //for (i = 0; i < count; i++) {
    //    printf("%d: %s\n", i, matches[i]);
    //}
    return 0;

















    int idx;
    char *matches[MAX_REGEX_MATCHES] = { 0 };
    //char *rpn[MAX_REGEX_MATCHES] = { 0 }; // Reverse polish notation
    //char *opStack[MAX_REGEX_MATCHES] = { 0 };

    Asm_EquElem equRpn[MAX_REGEX_MATCHES];
    Asm_EquElem equStack[MAX_REGEX_MATCHES];
    Asm_EquOpInfo equOpStack[MAX_REGEX_MATCHES];
    int rpnCount = 0;
    int stackCount = 0;
    int opStackCount = 0;


    for (idx = 0; idx < Asm_EquCount; idx++) {
        // Trim symbol and expression
        Asm_EquType ei = Asm_EquList[idx];
        int equLine = ei.ei_line;
        char *equSymName = ei.ei_sym;
        char *equExpression = ei.ei_expr;
        Str_Trim(equSymName);
        Str_Trim(equExpression);
        
        // Split expression
        int count;
        if ((count = Str_RegexSplit(equExpression, X_OPERATOR, ARR_SIZE(matches), matches)) > 0) {
            if (count == 1) {
                // Trivial expression
                Asm_OperandType *ao = Asm_ParseBranchOperand(equExpression);
                switch (ao->ao_type) {
                    // Literal value
                    case AO_LIT: {
                        Asm_Word value = ao->ao_lit;
                        Asm_AddAbsSymbol(elf, equSymName, value);
                    } break;

                    // Symbol value (symbol must be defined)
                    case AO_SYM: {
                        char *symName = equExpression;
                        if (Elf_SymbolExists(elf, symName)) {
                            Elf_Sym *sym = Elf_UseSymbol(elf, symName);
                            if (sym->st_shndx != SHN_UNDEF)  {
                                Asm_AddAbsSymbol(elf, equSymName, sym->st_value);
                            } else {
                                Show_Error(" Line %d: Symbol '%s' is undefined:\n\t.equ %s, %s", equLine, symName, equSymName, symName);
                            }
                        } else {
                            Show_Error(" Line %d: Symbol '%s' doesn't exist:\n\t.equ %s, %s", equLine, symName, equSymName, symName);
                        }
                    } break;
                }
                Asm_OperandDestroy(ao);
            } else if (count > 1) {
                // Composite expression.
                // Shunting-Yard algorithm creating reverse polish notation
                int i, equError = 0;
                for (i = 0; i < count && ! equError; i++) {
                    char *elem = matches[i];
                    Str_Trim(elem);
                    printf("'%s'\n", elem);
                    if (! Asm_EquIsOperator(elem)) {
                        Asm_EquElem ee;
                        Asm_OperandType *ao = Asm_ParseBranchOperand(elem);
                        switch (ao->ao_type) {
                            case AO_LIT: {
                                ee.ee_type = EQU_EE_LIT;
                                ee.ee_lit = ao->ao_lit;
                            } break;
                            case AO_SYM: {
                                char *symName = ao->ao_sym;
                                ee.ee_type = EQU_EE_SYM;
                                ee.ee_sym = strdup(symName);
                                //if (Elf_SymbolExists(elf, symName)) {
                                //    Elf_Sym *sym = Elf_UseSymbol(elf, symname);
                                //    if (sym->st_shndx != SHN_UNDEF) {

                                //    }
                                //} else {
                                //    Show_Error(" Line %d: Symbol '%s' doesn't exist:\n\t.equ %s, %s", equLine, symName, equSymName, symName);
                                //}
                            } break;
                            default : {
                                Show_Error("Line %d: Invalid equ expression", equLine);
                                equError = 1;
                            };
                        }
                        Asm_OperandDestroy(ao);
                        if (! equError) {
                            equRpn[rpnCount++] = ee;
                        }
                    } else {
                        // Pop operators from the stack
                        while (opStackCount > 0 && Asm_EquOpPrecedenceLte()) {

                            opStackCount -= 1;
                        }
                    }
                }

                // Parse rpn
                for (i = 0; i < rpnCount && !equError; i++)  {
                    Asm_EquElem ee = equRpn[i];
                    switch (ee.ee_type) {
                        case EQU_EE_LIT: {
                            printf("Literal value %08x\n", ee.ee_lit);
                        } break;
                        case EQU_EE_SYM: {
                            printf("Symbol value %s\n", ee.ee_sym);
                        } break;
                        case EQU_EE_OP: {

                        } break;
                    }
                }

            }
        }
    }





/*
typedef enum {
    O_POS, // + a
    O_NEG, // - a
    O_ADD, // a + b
    O_SUB, // a - b
    O_MUL, // a * b
    O_DIV, // a / b
    O_MOD, // a % b
    O_NOT, // ~ a
    O_AND, // a & b
    O_OR,  // a | b
    O_XOR, // a ^ b
    O_SHL, // a << b
    O_SHR, // a >> b
    O_LBR, // (
    O_RBR  // )

    // O_LOGIC_NOT, // ! a
    // O_LOGIC_AND,     // a && b
    // O_LOGIC_OR,      // a || b
    // O_LOGIC_XOR,     // a ^^ b
    // O_LOGIC_EQ,      // a == b
    // O_LOGIC_NEQ,     // a != b
    // O_LOGIC_GT,      // a > b
    // O_LOGIC_LT,      // a < b
    // O_LOGIC_GTE,     // a >= b
    // O_LOGIC_LTE,     // a <= b
    
    
} Asm_EquOpType;

typedef struct {
    char *eo_sym;    // symbol
    Asm_EquOpType eo_type; // operator
    int eo_prec;           // precedence
    int eo_argc;           // arity
} Asm_EquOpInfo;

Asm_EquOpInfo Asm_EquOpList[] = {
    { "+",  O_POS, 2,  1 },
    { "-",  O_NEG, 2,  1 },
    { "~",  O_NOT, 2,  1 },
    { "*",  O_ADD, 3,  2 },
    { "/",  O_DIV, 3,  2 },
    { "%",  O_MOD, 3,  2 },
    { "+",  O_ADD, 4,  2 },
    { "-",  O_SUB, 4,  2 },
    { "<<", O_SHL, 5,  2 },
    { ">>", O_SHR, 5,  2 },
    { "&",  O_AND, 8,  2 },
    { "^",  O_XOR, 9,  2 },
    { "|",  O_OR,  10, 2 },
    { "(",  O_LBR, 15, 2 },
    { ")",  O_RBR, 15, 2 },
};

int Asm_EquOpFindByType(Asm_EquOpType op)
{
    int idx;
    for (idx = 0; idx < ARR_SIZE(Asm_EquOpList); idx++) {
        if (op == Asm_EquOpList[idx].eo_type)
            return idx;
    }
    return -1;
}

// prec(op1) <= prec(op2)
int Asm_EquOpPrecdenceLte(Asm_EquOpType op1, Asm_EquOpType op2)
{
    int idx1 = Asm_EquOpFindByType(op1);
    int idx2 = Asm_EquOpFindByType(op2);
    if (idx1 != -1 && idx2 != -1)
        return Asm_EquOpList[idx1].eo_prec < Asm_EquOpList[idx2].eo_prec;
    return -1;
    
}

typedef enum {
    EQU_EE_OP,
    EQU_EE_SYM,
    EQU_EE_LIT
} Asm_EquElemType;

typedef struct {
    Asm_EquElemType ee_type;
    union {
        Asm_EquOpType ee_op;
        char *ee_sym;
        Asm_Word ee_lit;
    };
} Asm_EquElem;

*/

                /*
                if (Str_RegexMatch(other, "(#.*)$", ARR_SIZE(matches), matches)) {
                    Str_Cut(&other, matches[1].rm_so, matches[1].rm_eo);
                }

                // Find directive
                int isOk = TRUE;
                int idx = Asm_FindDirecIdx(direc);
                if (idx == -1) {
                    Show_Error("Line %d: Invalid directive '%s':\n %s", linenum, direc, line);
                    isOk = FALSE;
                }

                // Extract directive argumetns and check validity
                if (isOk)  {
                    argc = Asm_ExtractArguments(other, args, ARR_SIZE(args));
                    // TODO: Check arguments
                }

                // Parse directives
                if (isOk) {
                    Asm_DirecType direcType = Asm_DirecList[idx].d_type;
                    switch (direcType) {
                        // .extern
                        // .global
                        case D_EXTERN:
                        case D_GLOBAL:
                        {
                            int i;
                            for (i = 0; i < argc; i++) {
                                Elf_Sym *sym = Elf_UseSymbol(elf, args[i]);
                                sym->st_info = ELF_ST_INFO(STB_GLOBAL, ELF_ST_TYPE(sym->st_info));
                            }
                        } break;

                        // .section name
                        case D_SECTION:
                        {
                            // .section name
                            if (argc == 1) {
                                Elf_UseSection(elf, args[0]);
                            } else {
                                Show_Error("Line %d: '%s' directive requires exactly 1 argument:\n %s", linenum, direc, line);
                            }
                        } break;

                        // .byte
                        // .half
                        // .word
                        case D_BYTE:
                        case D_HALF:
                        case D_WORD:
                        {
                            // .byte lit|sym, ...
                            // .half lit|sym, ...
                            // .word lit|sym, ...
                            int i;
                            for (i = 0; i < argc; i++) {
                                Asm_OperandType *ao = Asm_ParseBranchOperand(args[i]);
                                if (ao->ao_type == AO_SYM) {
                                    int relaType = 
                                          direcType == D_BYTE ? R_SS_8
                                        : direcType == D_HALF ? R_SS_16
                                        : direcType == D_WORD ? R_SS_32
                                        : R_SS_NONE;
                                    Asm_AddRela(elf, args[i], relaType, 0);
                                    Elf_PushWord(elf, 0);
                                } else if (ao->ao_type == AO_LIT) {
                                    if (direcType == D_WORD) 
                                        Elf_PushWord(elf, ao->ao_lit);
                                    else if (direcType == D_HALF) 
                                        Elf_PushHalf(elf, ao->ao_lit);
                                    else if (direcType == D_BYTE) 
                                        Elf_PushByte(elf, ao->ao_lit);
                                } else if (ao->ao_type == AO_INVALID) {
                                    Show_Error("Line %d: Invalid symbol '%s':\n %s", linenum, args[i], line);
                                }
                                Asm_OperandDestroy(ao);
                            }
                        } break;

                        // .skip num
                        case D_SKIP:
                        {
                            if (argc == 1) {
                                int value = Str_ParseInt(args[0]);
                                Elf_PushSkip(elf, value, 0x00);
                            } else {
                                Show_Error("Line %d: '%s' directive requires exactly 1 argument:\n %s", linenum, direc, line);
                            }
                        } break;

                        // .ascii str
                        case D_ASCII:
                        {
                            if (argc == 1)  {
                                Str_RmQuotes(args[0]);
                                Str_UnescapeStr(args[0]);
                                Elf_PushString(elf, args[0]);
                            } else {
                                Show_Error("Line %d: '%s' directive requires exactly 2 arguments:\n %s", linenum, direc, line);
                            }
                        } break;

                        // .equ sym, expr
                        case D_EQU:
                        {
                            if (argc == 2) {
                                Asm_EquType equ = {
                                    .ei_line = linenum,
                                    .ei_sym = strdup(args[0]),
                                    .ei_expr = strdup(args[1])
                                };
                                Asm_EquList[Asm_EquCount] = equ;
                                Asm_EquCount += 1;
                                //const char *symName = args[0];
                                //Asm_Word value = Str_ParseInt(args[1]);
                                //Asm_AddAbsSymbol(elf, symName, value);
                            } else {
                                Show_Error("Line %d: '%s' directive requires exactly 2 arguments:\n %s", linenum, direc, line);
                            }
                        } break;

                        // .end
                        case D_END:
                        {
                            mode = MODE_QUIT;
                        } break;
                    }
                }

                */

// Extracts arguments and returns argc
int Asm_ExtractArguments(char *other, char **args, int max_args)
{
    int argc = 0;
    char *token = strtok(other, ",");
    while (token) {
        args[argc] = strdup(token);
        Str_Trim(args[argc]);
        argc += 1;
        token = strtok(NULL, ",");
    }
    return argc;
}

