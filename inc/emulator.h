#ifndef _SS_EMULATOR_H_
#define _SS_EMULATOR_H_
#include "types.h"
#include "elf.h"

// Memory functions
int Mem_IsMemoryMapped(Elf_Addr addr);
void Mem_InitMemoryMap(MPR_CallbackType callback, Asm_Addr start, Asm_Addr end);
void Mem_ResetMemoryMap();
void Mem_WriteRawByte(Asm_Addr addr, Asm_Byte value);
void Mem_WriteRawHalf(Asm_Addr addr, Asm_Half value);
void Mem_WriteRawWord(Asm_Addr addr, Asm_Word value);
void Mem_WriteRawBytes(void *data, Asm_Addr startAddr, int size);
Asm_Byte Mem_ReadRawByte(Asm_Addr addr);
Asm_Half Mem_ReadRawHalf(Asm_Addr addr);
Asm_Word Mem_ReadRawWord(Asm_Addr addr);
void Mem_ReadRawBytes(void *data, Asm_Addr startAddr, int size);
void Mem_WriteByte(Asm_Addr addr, Asm_Byte value);
void Mem_WriteHalf(Asm_Addr addr, Asm_Half value);
void Mem_WriteWord(Asm_Addr addr, Asm_Word value);
void Mem_WriteBytes(void *data, Asm_Addr startAddr, int size);
Asm_Byte Mem_ReadByte(Asm_Addr addr);
Asm_Half Mem_ReadHalf(Asm_Addr addr);
Asm_Word Mem_ReadWord(Asm_Addr addr);
void Mem_ReadBytes(void *data, Asm_Addr startAddr, int size);
void Mem_Clear(void);
void Mem_LoadElf(Elf_Builder *elf);
void Emu_MemoryMapCallback(MPR_Event event, Asm_Addr addr, Asm_Word value, Asm_Word *writeBack);

// Emulator threads
void Emu_PrintRegs(FILE *output);
void Emu_StopEmulation();
void Emu_StopTimer();
void Emu_StopTerminal();
int Emu_IsRunning(Emu_ThreadType thr);
void Emu_StartEmulation() ;
void Emu_StartTimer();
void Emu_StartTerminal();
void Emu_JoinTimer();
void Emu_JoinTerminal();

// Emulator
void Emu_RunElf(Elf_Builder *elf, FILE *output, Emu_FlagsType flags);

#endif /* _EMULATOR_H_ */ 
