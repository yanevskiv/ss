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

