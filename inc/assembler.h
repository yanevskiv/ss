#ifndef _SS_ASEMBLER_H_
#define _SS_ASEMBLER_H_
#include "util.h"
#include "types.h"
#include "elf.h"

// Find instruction / directive
int Asm_FindInstrIdx(const char *instr);
int Asm_FindDirecIdx(const char *direct);

// Add a label at current section location
Elf_Sym *Asm_AddLabel(Elf_Builder *elf, const char *label);
Elf_Sym *Asm_AddAbsSymbol(Elf_Builder *elf, const char *name, Elf_Addr value);
Elf_Rela *Asm_AddRela(Elf_Builder *elf, const char *symbol, Elf_Half type, Elf_Sxword addend);

// Loading values into registers
void Asm_PushXorZero(Elf_Builder *elf, Asm_RegType gprD);
void Asm_PushLoadByte(Elf_Builder *elf, Asm_RegType rDest, Asm_Byte value);
void Asm_PushLoadHalf(Elf_Builder *elf, Asm_RegType rDest, Asm_Half value, Asm_RegType rHelp);
void Asm_PushLoadWord(Elf_Builder *elf, Asm_RegType rDest, Asm_Word value, Asm_RegType rHelp);

// Instructions
void Asm_PushInstr(Elf_Builder *elf, Asm_OcType oc, Asm_ModType mod, Asm_RegType ra, Asm_RegType rb, Asm_RegType rc, Asm_DispType disp);
void Asm_PushHalt(Elf_Builder *elf);
void Asm_PushInt(Elf_Builder *elf);
void Asm_PushIret(Elf_Builder *elf);
void Asm_PushRet(Elf_Builder *elf);
void Asm_PushStackPush(Elf_Builder *elf, Asm_RegType gprS);
void Asm_PushStackPop(Elf_Builder *elf, Asm_RegType gprD);

void Asm_PushXchg(Elf_Builder *elf, Asm_RegType gpr1, Asm_RegType gpr2);
void Asm_PushALU(Elf_Builder *elf, Asm_AluType op, Asm_RegType gprS, Asm_RegType gprD);
void Asm_PushAdd(Elf_Builder *elf, Asm_RegType gprS, Asm_RegType gprD);
void Asm_PushSub(Elf_Builder *elf, Asm_RegType gprS, Asm_RegType gprD);
void Asm_PushMul(Elf_Builder *elf, Asm_RegType gprS, Asm_RegType gprD);
void Asm_PushNot(Elf_Builder *elf, Asm_RegType gprD);
void Asm_PushAnd(Elf_Builder *elf, Asm_RegType gprS, Asm_RegType gprD);
void Asm_PushOr(Elf_Builder *elf, Asm_RegType gprS, Asm_RegType gprD);
void Asm_PushXor(Elf_Builder *elf, Asm_RegType gprS, Asm_RegType gprD);
void Asm_PushShl(Elf_Builder *elf, Asm_RegType gprS, Asm_RegType gprD);
void Asm_PushShr(Elf_Builder *elf, Asm_RegType gprS, Asm_RegType gprD);

void Asm_PushCsrrd(Elf_Builder *elf, Asm_RegType csrS, Asm_RegType gprD);
void Asm_PushCsrwr(Elf_Builder *elf, Asm_RegType gprS, Asm_RegType csrD);

void Asm_PushLoadValue(Elf_Builder *elf, Asm_RegType gprD, Elf_Word literal, const char *symName);
void Asm_PushLoadMemValue(Elf_Builder *elf, Asm_RegType gprD, Elf_Addr addr, const char *symName);
void Asm_PushLoadLiteralValue(Elf_Builder *elf, Asm_Word literal, Asm_RegType gprD);
void Asm_PushLoadSymbolValue(Elf_Builder *elf, const char *symName, Asm_RegType gprD);
void Asm_PushLoadMemLiteralValue(Elf_Builder *elf, Asm_Addr literal, Asm_RegType gprD);
void Asm_PushLoadMemSymbolValue(Elf_Builder *elf, const char *symName, Asm_RegType gprD);
void Asm_PushLoadReg(Elf_Builder *elf, Asm_RegType gprS, Asm_RegType gprD);
void Asm_PushLoadMemRegDisp(Elf_Builder *elf, Asm_RegType gprS, Asm_DispType disp, Asm_RegType gprD);
void Asm_PushLoadMemRegSymbolDisp(Elf_Builder *elf, Asm_RegType gprS, const char *symName, Asm_RegType gprD);

void Asm_PushStoreAddr(Elf_Builder *elf, Asm_RegType gprS, Elf_Addr addr);
void Asm_PushStoreSymbolAddr(Elf_Builder *elf, Asm_RegType gprS, const char *symName);
void Asm_PushStoreMemAddr(Elf_Builder *elf, Asm_RegType gprS, Elf_Addr addr);
void Asm_PushStoreSymbolMemAddr(Elf_Builder *elf, Asm_RegType gprS, const char *symName);
void Asm_PushStoreReg(Elf_Builder *elf, Asm_RegType gprS, Asm_RegType gprD);
void Asm_PushStoreMemRegDisp(Elf_Builder *elf, Asm_RegType gprS, Asm_RegType gprD, Asm_DispType disp);
void Asm_PushStoreMemRegSymbolDisp(Elf_Builder *elf, Asm_RegType gprS, Asm_RegType gprD, const char *symName);

void Asm_PushBranchReg(Elf_Builder *elf, Asm_BranchType op, Asm_RegType gprA, Asm_RegType gprB, Asm_RegType gprC, Asm_DispType disp);
void Asm_PushBranch(Elf_Builder *elf, Asm_BranchType op, Asm_Addr addr, const char *symName, Asm_RegType gpr1, Asm_RegType gpr2, Asm_DispType disp);

void Asm_PushJmpLiteral(Elf_Builder *elf, Elf_Addr addr);
void Asm_PushBeqLiteral(Elf_Builder *elf, Asm_RegType gpr1, Asm_RegType gpr2, Elf_Addr addr);
void Asm_PushBneLiteral(Elf_Builder *elf, Asm_RegType gpr1, Asm_RegType gpr2, Elf_Addr addr);
void Asm_PushBgtLiteral(Elf_Builder *elf, Asm_RegType gpr1, Asm_RegType gpr2, Elf_Addr addr);
void Asm_PushCallLiteral(Elf_Builder *elf, Elf_Addr addr);

void Asm_PushJmpSymbol(Elf_Builder *elf, const char *symName);
void Asm_PushBeqSymbol(Elf_Builder *elf, Asm_RegType gpr1, Asm_RegType gpr2, const char *symName);
void Asm_PushBneSymbol(Elf_Builder *elf, Asm_RegType gpr1, Asm_RegType gpr2, const char *symName);
void Asm_PushBgtSymbol(Elf_Builder *elf, Asm_RegType gpr1, Asm_RegType gpr2, const char *symName);
void Asm_PushCallSymbol(Elf_Builder *elf, const char *symName);

// Parsing functions
int Asm_SplitArgs(const char *str, int size, char **args);
int Asm_ParseRegOperand(const char *str);
int Asm_ParseCsrOperand(const char *str);
Asm_OperandType *Asm_ParseDataOperand(const char *str);
Asm_OperandType *Asm_ParseBranchOperand(const char *str);
void Asm_OperandDestroy(Asm_OperandType *ao);

// Equ expressions
int Equ_FindOperIdxByType(Equ_OperType type);
int Equ_FindOperIdxByName(const char *name);
int Equ_CmpOperPrec(Equ_OperType op1, Equ_OperType op2);

// Add instruction, directive or equ symbol
int Asm_AddInstruction(Elf_Builder *elf, const char *instrName, const char *argsLine);
int Asm_AddDirective(Elf_Builder *elf, const char *direcName, const char *argsLine);
int Asm_AddEquSymbol(Elf_Builder *elf, const char *symName, const char *expression);

// Compile file
void Asm_Compile(Elf_Builder *elf, FILE *input, int flags);

#endif /* _SS_ASEMBLER_H_ */ 
