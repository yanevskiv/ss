#include <stdlib.h>
#include <emulator.h>
#include <assert.h>
#include <elf.h>
#include <string.h>
#include <util.h>
#include <unistd.h>
#include <pthread.h>
#include <termios.h>

#ifdef DEBUG
#define Show_DebugInstr(...)  \
    do { \
        fprintf(stdout, "\e[32mDebug instr\e[0m %d: ", instrCount); \
        fprintf(stdout, __VA_ARGS__); \
        fprintf(stdout, "\n"); \
        fflush(stdout); \
    } while (0)
#else
#define DebugInstr(...)
#endif


// Pending interrupts
volatile int INT[4] = { 0 };

// Whether interrupts are enabled at all
volatile int INT_ENA = 1;

// Reentrant lock level
volatile int LCK_LVL = 0;

// General purpose registers
Asm_Reg GPR[GPR_COUNT] = { 0 };

// Control registers
Asm_Reg CSR[CSR_COUNT]  = { 0 };

// Virtual memory
unsigned char *PAGES[VA_PAGE_SIZE] = { 0 };

// Memory map start addr / end addr 
Asm_Addr MPR_Addr[2] = { 0x0, 0x0 };

// Memory map callback
MPR_CallbackType MPR_Callback = NULL;

// Current operationg modes for emulator, timer, terminal.
Emu_ModeType MODES[3] = {
    MODE_START,
    MODE_START,
    MODE_START
};

// Mutex protecting shared data (INT[])
pthread_mutex_t Emu_ThreadMutex;

// Timer thread
pthread_t Emu_TimerThread;

// Terminal thread
pthread_t Emu_TerminalThread;

// EMU_TIM_CFG values mapped to timer sleep time type
const Emu_TimeType Emu_TimConfigMap[] = {
    500  * 1000,        // 0x0 -> 500ms
    1000 * 1000,        // 0x1 -> 1000ms
    1500 * 1000,        // 0x2 -> 1500ms
    2000 * 1000,        // 0x3 -> 2000ms
    5000 * 1000,        // 0x4 -> 5000ms
    10   * 1000 * 1000, // 0x5 -> 10s
    30   * 1000 * 1000, // 0x6 -> 30s
    60   * 1000 * 1000, // 0x7 -> 60s
};

// Timer tick configuration (500ms default)
volatile int Emu_TimConfig = 0;

// Last character read from the terminal
volatile Asm_Word Emu_TermBuffer = 'a';

// Check whether an address points to a memory mapped register
int Mem_IsMemoryMapped(Elf_Addr addr)
{
    return MPR_Callback != NULL && (addr >= MPR_Addr[MPR_START_ADDR] && addr <= MPR_Addr[MPR_END_ADDR]);
}

// Initialize memory mapped segment
void Mem_InitMemoryMap(MPR_CallbackType callback, Asm_Addr start, Asm_Addr end)
{
    MPR_Addr[MPR_START_ADDR] = start;
    MPR_Addr[MPR_END_ADDR] = end;
    MPR_Callback = callback;
}

// Disable memory mapping
void Mem_ResetMemoryMap()
{
    MPR_Addr[MPR_START_ADDR] = 0x0;
    MPR_Addr[MPR_END_ADDR] = 0x0;
    MPR_Callback = NULL;
}

// Write raw byte
void Mem_WriteRawByte(Asm_Addr addr, Asm_Byte value)
{
    int page = VA_PAGE(addr);
    int word = VA_WORD(addr);
    if (PAGES[page] == NULL) {
        PAGES[page] = malloc(VA_PAGE_SIZE);
        assert(PAGES[page] != NULL);
    }
    PAGES[page][word] = value & 0xff;
}

// Write raw half
void Mem_WriteRawHalf(Asm_Addr addr, Asm_Half value)
{
    Mem_WriteRawByte(addr + 0, (value >> 0)  & 0xff);
    Mem_WriteRawByte(addr + 1, (value >> 8)  & 0xff);
}

// Write raw word
void Mem_WriteRawWord(Asm_Addr addr, Asm_Word value)
{
    Mem_WriteRawByte(addr + 0, (value >> 0)  & 0xff);
    Mem_WriteRawByte(addr + 1, (value >> 8)  & 0xff);
    Mem_WriteRawByte(addr + 2, (value >> 16) & 0xff);
    Mem_WriteRawByte(addr + 3, (value >> 24) & 0xff);
}

// Write raw bytes
void Mem_WriteRawBytes(void *data, Asm_Addr startAddr, int size)
{
    unsigned char *bytes = (unsigned char*)data;
    int i;
    for (i = 0; i < size; i++) {
        Mem_WriteRawByte(startAddr + i, bytes[i]);
    }
}

// Read raw byte
Asm_Byte Mem_ReadRawByte(Asm_Addr addr)
{
    int page = VA_PAGE(addr);
    int word = VA_WORD(addr);
    if (PAGES[page] == NULL) {
        return 0;
    }
    return PAGES[page][word];
}

// Read raw half
Asm_Half Mem_ReadRawHalf(Asm_Addr addr)
{
    return 
          Mem_ReadRawByte(addr + 0) << 0
        | Mem_ReadRawByte(addr + 1) << 8
    ;
}

// Read raw word
Asm_Word Mem_ReadRawWord(Asm_Addr addr)
{
    return 
          Mem_ReadRawByte(addr + 0) << 0
        | Mem_ReadRawByte(addr + 1) << 8
        | Mem_ReadRawByte(addr + 2) << 16
        | Mem_ReadRawByte(addr + 3) << 24
    ;
}

// Read raw bytes
void Mem_ReadRawBytes(void *data, Asm_Addr startAddr, int size)
{
    unsigned char *bytes = (unsigned char*)data;
    int i;
    for (i = 0; i < size; i++) {
        bytes[i] = Mem_ReadRawByte(startAddr + i);
    }
}


// Write byte
void Mem_WriteByte(Asm_Addr addr, Asm_Byte value)
{
    if (Mem_IsMemoryMapped(addr)) {
        MPR_Callback(MPR_WRITE, addr, value, NULL);
    } else {
        Mem_WriteRawByte(addr, value);
    }
}

// Write half
void Mem_WriteHalf(Asm_Addr addr, Asm_Half value)
{
    if (Mem_IsMemoryMapped(addr)) {
        MPR_Callback(MPR_WRITE, addr, value, NULL);
    } else {
        Mem_WriteRawHalf(addr, value);

    }
}

// Write word
void Mem_WriteWord(Asm_Addr addr, Asm_Word value)
{
    if (Mem_IsMemoryMapped(addr)) {
        MPR_Callback(MPR_WRITE, addr, value, NULL);
    } else {
        Mem_WriteRawWord(addr, value);
    }
}

// Write bytes
void Mem_WriteBytes(void *data, Asm_Addr startAddr, int size)
{
    Mem_WriteRawBytes(data, startAddr, size);
}

// Read byte
Asm_Byte Mem_ReadByte(Asm_Addr addr)
{
    Asm_Word value = Mem_ReadRawByte(addr);
    if (Mem_IsMemoryMapped(addr)) {
        MPR_Callback(MPR_READ, addr, value, &value);
    }
    return value & 0xff;
}

// Read half
Asm_Half Mem_ReadHalf(Asm_Addr addr)
{
    Asm_Word value = Mem_ReadRawHalf(addr);
    if (Mem_IsMemoryMapped(addr)) {
        MPR_Callback(MPR_READ, addr, value, &value);
    }
    return value & 0xffff;
}

// Read word
Asm_Word Mem_ReadWord(Asm_Addr addr)
{
    Asm_Word value = Mem_ReadRawWord(addr);
    if (Mem_IsMemoryMapped(addr)) {
        MPR_Callback(MPR_READ, addr, value, &value);
    }
    return value;
}

// Read bytes
void Mem_ReadBytes(void *data, Asm_Addr startAddr, int size)
{
    Mem_ReadRawBytes(data, startAddr, size);
}

// Clear memory
void Mem_Clear(void)
{
    int page;
    for (page = 0; page < VA_PAGE_COUNT; page++) {
        if (PAGES[page])  {
            free(PAGES[page]);
            PAGES[page] = NULL;
        }
    }
}

// Load ELF sections into memory
void Mem_LoadElf(Elf_Builder *elf)
{
    // Load into memory
    Asm_Addr offset = 0;
    for (int i = 1; i <= Elf_GetSectionCount(elf); i++) {
        // Load section into memory
        Elf_PushSection(elf);
        Elf_SetSection(elf, i);
        Elf_Shdr *shdr = Elf_GetSection(elf);
        if (shdr->sh_type != SHT_SYMTAB && shdr->sh_type != SHT_RELA) {
            Elf_Addr addr = shdr->sh_addr;
            Elf_Xword size = shdr->sh_size;
            unsigned char *data = Elf_GetSectionData(elf);

            // If address is zero, push it at the beginning
            // Otherwise, place it at the desired location
            if (addr == 0) {
                Mem_WriteRawBytes(data, offset, size);
                offset += size;
            } else {
                Mem_WriteRawBytes(data, addr, size);
            }

            // Rewrite relas
            char *relaName = Str_Concat(".rela", Elf_GetSectionName(elf));
            if (Elf_SectionExists(elf, relaName)) {
                Elf_PushSection(elf);
                Elf_UseSection(elf, relaName);
                int i, count = Elf_GetSectionSize(elf) / sizeof(Elf_Rela);
                for (i = 0; i < count; i++) {
                    Elf_Rela *rela  = (Elf_Rela*)Elf_GetSectionEntry(elf, i);
                    Elf_Half type   = ELF_R_TYPE(rela->r_info);
                    Elf_Word symndx = ELF_R_SYM(rela->r_info);
                    Elf_Sym *symb   = Elf_GetSymbol(elf, symndx);
                    Elf_Addr value  = symb->st_value;
                    value += rela->r_addend;
                    if (symb->st_shndx != SHN_ABS) {
                        // Non-absolute symbols
                        Elf_PushSection(elf);
                        Elf_Shdr *sym_shdr = Elf_SetSection(elf, symb->st_shndx);
                        value += sym_shdr->sh_addr;
                        Elf_PopSection(elf);
                    }
                    Elf_Addr start = shdr->sh_addr + rela->r_offset;
                    switch (type) {
                        case R_SS_8: {
                            //  Set byte
                            Mem_WriteRawByte(start, value & 0xff);
                        } break;

                        case R_SS_16: {
                            // Set half
                            Mem_WriteRawHalf(start, value & 0xffff);
                        } break;

                         case R_SS_32: {
                            // Set half
                            Mem_WriteRawWord(start, value);
                        } break;

                        case R_SS_D12: {
                            // Sets instruction displacement
                            Asm_Byte byte = Mem_ReadRawByte(start + 2);
                            Mem_WriteRawByte(start + 2, PACK(byte >> 4, value >> 8));
                            Mem_WriteRawByte(start + 3, value & 0xff);
                        } break;

                        case R_SS_LD8: {
                            // Replaces the byte value generated by `Asm_PushLoadByte()`
                            start += 4; // Skip the instruction that's setting the register to zero
                            Asm_Byte byte = Mem_ReadRawByte(start + 2);
                            Mem_WriteRawByte(start + 2, PACK(byte >> 4, value >> 8));
                            Mem_WriteRawByte(start + 3, value & 0xff);
                        } break;

                        case R_SS_LD16: {
                             // Replaces the half value generated by `Asm_PushLoadHalf()`
                             Mem_WriteRawByte(start + 3 * ASM_INSTR_WIDTH + 3, (value >> 8));
                             Mem_WriteRawByte(start + 5 * ASM_INSTR_WIDTH + 3, (value >> 0));
                        } break;

                        case R_SS_LD32: {
                             // Replaces the word value generated by `Asm_PushLoadWord()`
                             Mem_WriteRawByte(start + 3 * ASM_INSTR_WIDTH + 3, (value >> 24));
                             Mem_WriteRawByte(start + 5 * ASM_INSTR_WIDTH + 3, (value >> 16));
                             Mem_WriteRawByte(start + 7 * ASM_INSTR_WIDTH + 3, (value >> 8));
                             Mem_WriteRawByte(start + 9 * ASM_INSTR_WIDTH + 3, (value >> 0));
                        } break;

                        default: {
                            Show_Error("Unsupported rela type");
                        } break;
                    }
                }
                Elf_PopSection(elf);
            }
            free(relaName);
        }
        Elf_PopSection(elf);
    }
}

// Create mutex
void Emu_MutexInit(void)
{
    pthread_mutex_init(&Emu_ThreadMutex, NULL);
}

// Lock mutex
void Emu_MutexLock(void)
{
    //pthread_mutex_lock(&Emu_ThreadMutex);
}

// Unlock mutex
void Emu_MutexUnlock(void)
{
    //pthread_mutex_unlock(&Emu_ThreadMutex);
}

// Destroy mutex
void Emu_MutexDestroy(void)
{
    pthread_mutex_destroy(&Emu_ThreadMutex);
}

// Stop emulation
void Emu_StopEmulation()
{
    MODES[THR_EMULATOR] = MODE_STOP;
}

// Stop timer
void Emu_StopTimer()
{
    MODES[THR_TIMER] = MODE_STOP;
}

// Stop terminal
void Emu_StopTerminal()
{
    MODES[THR_TERMINAL] = MODE_STOP;
}

// Check whether thread is still in runing mode
int Emu_IsRunning(Emu_ThreadType thr)
{
    return MODES[thr] == MODE_RUNNING;
}

// Timer thread callback
static void *Emu_TimerCallback(void *data)
{
    while (Emu_IsRunning(THR_TIMER)) {
        if (Emu_TimConfig >= 0 && Emu_TimConfig < ARR_SIZE(Emu_TimConfigMap)) {
            usleep(Emu_TimConfigMap[Emu_TimConfig]);
        } else {
            usleep(500 * 1000);
        }

        Emu_MutexLock();
        INT[INT_TIMER] = 1;
        Emu_MutexUnlock();
    }
    return NULL;
}


// Terminal thread callback
static void *Emu_TerminalCallback(void *data)
{
    struct termios old_attributes, new_attributes;

    // Save the current terminal attributes
    tcgetattr(STDIN_FILENO, &old_attributes);
    new_attributes = old_attributes;

    // Disable canonical mode and echo
    new_attributes.c_lflag &= ~(ICANON | ECHO);

    // Set the new terminal attributes
    tcsetattr(STDIN_FILENO, TCSANOW, &new_attributes);

    int ch;
    while (Emu_IsRunning(THR_TERMINAL)) {
        if (read(STDIN_FILENO, &ch, 1) == 1) {
            Emu_TermBuffer = ch;
            // Exit the loop if the pressed character is 'q'
            if (ch == '`') {
                Emu_PrintRegs(stdout);
                fflush(stdout);
            } else if (ch == '\e') {
                Emu_StopEmulation();
                break;
            } else {
                Emu_MutexLock();
                INT[INT_TERMINAL] = 1;
                Emu_MutexUnlock();
            }
        }
    }

    // Restore the original terminal attributes
    tcsetattr(STDIN_FILENO, TCSANOW, &old_attributes);
    return NULL;
}

// Start timer
void Emu_StartEmulation() 
{
    MODES[THR_EMULATOR] = MODE_RUNNING;
}

// Start timer
void Emu_StartTimer() 
{
    MODES[THR_TIMER] = MODE_RUNNING;
    pthread_create(&Emu_TimerThread, NULL, Emu_TimerCallback, NULL);
}

// Start terminal
void Emu_StartTerminal()
{
    MODES[THR_TERMINAL] = MODE_RUNNING;
    pthread_create(&Emu_TerminalThread, NULL, Emu_TerminalCallback, NULL);
}

// Join timer
void Emu_JoinTimer()
{
    pthread_join(Emu_TimerThread, NULL);
}

// Join terminal
void Emu_JoinTerminal()
{
    pthread_join(Emu_TerminalThread, NULL);
}

// Memory mapping callback
void Emu_MemoryMapCallback(MPR_Event event, Asm_Addr addr, Asm_Word value, Asm_Word *writeBack)
{
    switch (addr) {
        case EMU_TERM_OUT: {
            if (event == MPR_WRITE) {
                Emu_MutexLock();
                fprintf(stdout, "%c", (char) value);
                fflush(stdout);
                Emu_MutexUnlock();
            }
        } break;

        case EMU_TERM_IN: {
            if (event == MPR_READ) {
                *writeBack = Emu_TermBuffer;
            }
        } break;

        case EMU_TIM_CFG: {
            if (event == MPR_WRITE) {
                if (value >= 0 && value < ARR_SIZE(Emu_TimConfigMap)) {
                    Emu_TimConfig = value;
                }
            } else if (event == MPR_READ) {
                *writeBack = Emu_TimConfig;
            }
        } break;
    }
}

// Print some information about an instruction 
void Emu_DebugInstr(FILE *output, Asm_OcType oc, Asm_ModType mod, Asm_RegType ra, Asm_RegType rb, Asm_RegType rc, Asm_DispType disp)
{
    switch (PACK(oc, mod)) {
        // Halt
        case PACK(OC_HALT, 0x0): {
            fprintf(output, "halt");
        } break;

        // Software interrupt
        case PACK(OC_INT, 0x0): {
            fprintf(output, "int");
        } break;
        
        // Call reg
        case PACK(OC_CALL, MOD_CALL_REG): {
            fprintf(output, "call [GPR[r%02d] + GPR[r%02d] + %d]", ra, rb, disp);
        } break;

        // Call mem
        case PACK(OC_CALL, MOD_CALL_MEM): {
            fprintf(output, "call mem32[GPR[r%02d] + GPR[r%02d] + %d]", ra, rb, disp);
        } break;

        // Jmp
        case PACK(OC_BRANCH, MOD_BRANCH_0): {
            fprintf(output, "branch pc <= GPR[r%02d] + %d", ra, disp);
        } break;

        // Beq
        case PACK(OC_BRANCH, MOD_BRANCH_1): {
            fprintf(output, "branch if (GPR[r%02d] == GPR[r%02d]) pc <= GPR[r%02d] + %d", rb, rc, ra, disp);
        } break;

        // Bne
        case PACK(OC_BRANCH, MOD_BRANCH_2): {
            fprintf(output, "branch if (GPR[r%02d] != GPR[r%02d]) pc <= GPR[r%02d] + %d", rb, rc, ra, disp);
        } break;

        // Bgt
        case PACK(OC_BRANCH, MOD_BRANCH_3): {
            fprintf(output, "branch if (GPR[r%02d] > GPR[r%02d]) pc <= GPR[r%02d] + %d", rb, rc, ra, disp);
        } break;

        // Jmp mem32
        case PACK(OC_BRANCH, MOD_BRANCH_4): {
            fprintf(output, "branch pc <= mem32[GPR[r%02d] + %d]", ra, disp);
        } break;

        // Beq mem32
        case PACK(OC_BRANCH, MOD_BRANCH_5): {
            fprintf(output, "branch if (GPR[r%02d] == GPR[r%02d]) pc <= mem32[GPR[r%02d] + %d]", rb, rc, ra, disp);
        } break;

        // Bne mem32
        case PACK(OC_BRANCH, MOD_BRANCH_6): {
            fprintf(output, "branch if (GPR[r%02d] != GPR[r%02d]) pc <= mem32[GPR[r%02d] + %d]", rb, rc, ra, disp);
        } break;

        // Bgt mem32
        case PACK(OC_BRANCH, MOD_BRANCH_7): {
            fprintf(output, "branch if (GPR[r%02d] > GPR[r%02d]) pc <= mem32[GPR[r%02d] + %d]", rb, rc, ra, disp);
        } break;

        // Xchg
        case PACK(OC_XCHG, 0x0): {
            fprintf(output, "xchg GPR[r%02d], GPR[r%02d]", rb, rc);
        } break;

        // Add
        case PACK(OC_ARITH, MOD_ARITH_ADD): {
            fprintf(output, "add GPR[r%02d] <= GPR[r%02d] + GPR[r%02d]", ra, rb, rc);
        } break;

        // Sub
        case PACK(OC_ARITH, MOD_ARITH_SUB): {
            fprintf(output, "sub GPR[r%02d] <= GPR[r%02d] - GPR[r%02d]", ra, rb, rc);
        } break;

        // Mul
        case PACK(OC_ARITH, MOD_ARITH_MUL): {
            fprintf(output, "mul GPR[r%02d] <= GPR[r%02d] * GPR[r%02d]", ra, rb, rc);
        } break;

        // Div
        case PACK(OC_ARITH, MOD_ARITH_DIV): {
            fprintf(output, "div GPR[r%02d] <= GPR[r%02d] / GPR[r%02d]", ra, rb, rc);
        } break;

        // Not
        case PACK(OC_LOGIC, MOD_LOGIC_NOT): {
            fprintf(output, "not GPR[r%02d] <= ~GPR[r%02d]", ra, rb);
        } break;

        // And
        case PACK(OC_LOGIC, MOD_LOGIC_AND): {
            fprintf(output, "and GPR[r%02d] <= GPR[r%02d] & GPR[r%02d]", ra, rb, rc);
        } break;

        // Or
        case PACK(OC_LOGIC, MOD_LOGIC_OR): {
            fprintf(output, "or GPR[r%02d] <= GPR[r%02d] | GPR[r%02d]", ra, rb, rc);
        } break;

        // Xor
        case PACK(OC_LOGIC, MOD_LOGIC_XOR): {
            fprintf(output, "xor GPR[r%02d] <= GPR[r%02d] ^ GPR[r%02d]", ra, rb, rc);
        } break;

        // Shl
        case PACK(OC_SHIFT, MOD_SHIFT_LEFT): {
            fprintf(output, "shl GPR[r%02d] <= GPR[r%02d] << GPR[r%02d]", ra, rb, rc);
        } break;

        // Shr
        case PACK(OC_SHIFT, MOD_SHIFT_RIGHT): {
            fprintf(output, "xor GPR[r%02d] <= GPR[r%02d] >> GPR[r%02d]", ra, rb, rc);
        } break;

        // St 0
        case PACK(OC_STORE, MOD_STORE_0): {
            fprintf(output, "st  mem32[GPR[r%02d] + GPR[r%02d] + %d] <= GPR[r%02d]", ra, rb, disp, rc);
        } break;

        // St 1
        case PACK(OC_STORE, MOD_STORE_1): {
            fprintf(output, "st  mem32[mem32[GPR[r%02d] + GPR[r%02d] + %d]] <= GPR[r%02d]", ra, rb, disp, rc);
        } break;

        // St 2
        case PACK(OC_STORE, MOD_STORE_2): {
            fprintf(output, "st  GPR[r%02d] <= GPR[r%02d] + %d; mem32[GPR[r%02d]] <= GPR[r%02d]", ra, ra, disp, ra, rc);
        } break;

        // Ld 0
        case PACK(OC_LOAD, MOD_LOAD_0): {
            fprintf(output, "ld  GPR[r%02d] <= CSR[s%02d]", ra, rb);
        } break;

        // Ld 1
        case PACK(OC_LOAD, MOD_LOAD_1): {
            fprintf(output, "ld  GPR[r%02d] <= GPR[r%02d] + %d", ra, rb, disp);
        } break;

        // Ld 2
        case PACK(OC_LOAD, MOD_LOAD_2): {
            fprintf(output, "ld  GPR[r%02d] <= mem32[GPR[r%02d] + GPR[r%02d] + %d]", ra, rb, rc, disp);
        } break;

        // Ld 3
        case PACK(OC_LOAD, MOD_LOAD_3): {
            fprintf(output, "ld  GPR[r%02d] <= mem32[GPR[r%02d]]; GPR[r%02d] <= GPR[r%02d] + %d", ra, rb, rb, rb, disp);
        } break;

        // Ld 4
        case PACK(OC_LOAD, MOD_LOAD_4): {
            fprintf(output, "ld  CSR[r%02d] <= GPR[s%02d]", ra, rb);
        } break;

        // Ld 5
        case PACK(OC_LOAD, MOD_LOAD_5): {
            fprintf(output, "ld  CSR[r%02d] <= CSR[r%02d] | %d", ra, rb, disp);
        } break;

        // Ld 6
        case PACK(OC_LOAD, MOD_LOAD_6): {
            fprintf(output, "ld  CSR[r%02d] <= mem32[GPR[r%02d] + GPR[r%02d] + %d]", ra, rb, rc, disp);
        } break;

        // Ld 7
        case PACK(OC_LOAD, MOD_LOAD_7): {
            fprintf(output, "ld  CSR[r%02d] <= mem32[GPR[r%02d]]; GPR[r%02d] <= GPR[r%02d] + %d", ra, rb, rb, rb, disp);
        } break;

        default: {
            fprintf(output, "?");
        } break;
    }

    //// Print information about registers
    //fprintf(output, "(GPR[ra=%d]=%08x GPR[rb=%d]=%08x GPR[rc=%d] disp=%d)",
    //    ra, GPR[ra],
    //    rb, GPR[rb],
    //    rc, GPR[rc],
    //    disp
    //);
    fprintf(output, "\n");
}

// Running the emulator
void Emu_RunElf(Elf_Builder *elf, FILE *output, Emu_FlagsType flags)
{
    // Reset registers
    GPR[PC] = 0x40000000;
    GPR[SP] = 0x80000000;

    // Load elf
    Mem_LoadElf(elf);

    // Initialize memory mapping
    Mem_InitMemoryMap(Emu_MemoryMapCallback, EMU_MMAP_START, EMU_MMAP_END);
 
    // Number of executed instructions and timer interrupts
    int timerCount = 0;
    int instrCount = 0;

    // Initialize muetx
    Emu_MutexInit();

    // Start terminal and timer
    if (flags & F_EMU_TIMER)
        Emu_StartTimer();
    if (flags & F_EMU_TERMINAL)
        Emu_StartTerminal();

    // Start emulating
    Emu_StartEmulation();
    while (Emu_IsRunning(THR_EMULATOR))  {
        // Fetch instruction
        unsigned char bytes[4];
        Mem_ReadBytes(bytes, GPR[PC], ASM_INSTR_WIDTH);
        GPR[PC] += ASM_INSTR_WIDTH;
        Asm_OcType  oc  = (bytes[0] >> 4) & 0xf;
        Asm_ModType mod = (bytes[0] >> 0) & 0xf;
        Asm_RegType ra  = (bytes[1] >> 4) & 0xf;
        Asm_RegType rb  = (bytes[1] >> 0) & 0xf;
        Asm_RegType rc  = (bytes[2] >> 4) & 0xf; 
        Asm_DispType disp = (((bytes[2] >> 0) & 0xf) << 8) | bytes[3];
        if (disp >= (1 << 11))  {
            disp -= (1 << 12);
        }
        instrCount += 1;

        if (flags & F_EMU_DEBUG) {
            printf("\e[32mInstr\e[0m %2d: ", instrCount);
            Emu_DebugInstr(stderr, oc, mod, ra, rb, rc, disp);
            Emu_PrintRegs(stderr);
        }

        // Execute instruction
        switch (oc) {
            // Halt
            case OC_HALT: {
                Emu_StopEmulation();
            } break;

            // Software interrupt
            case OC_INT: {
                INT[INT_SOFTWARE] = 1;
            } break;

            // Call
            case OC_CALL: {
                switch (mod) {
                    case MOD_CALL_REG: {
                        // push pc; pc <= ra + rb + disp
                        Asm_Addr addr = GPR[ra] + GPR[rb] + disp;
                        GPR[SP] -= 4;
                        Mem_WriteWord(GPR[SP], GPR[PC]);
                        GPR[PC] = addr;
                    } break;

                    case MOD_CALL_MEM: {
                        // push pc; pc <= mem32[ra + rb + disp]
                        Asm_Addr addr = GPR[ra] + GPR[rb] + disp;
                        GPR[SP] -= 4;
                        Mem_WriteWord(GPR[SP], GPR[PC]);
                        GPR[PC] = Mem_ReadWord(addr);
                    } break;

                    case MOD_CALL_RET: {
                        // pop pc
                        GPR[PC] = Mem_ReadRawWord(GPR[SP]);
                        GPR[SP] += 4;
                    } break;

                    case MOD_CALL_IRET: {
                        // pop status
                        // pop pc
                        GPR[STATUS] = Mem_ReadRawWord(GPR[SP]);
                        GPR[SP] += 4;
                        GPR[PC] = Mem_ReadRawWord(GPR[SP]);
                        GPR[SP] += 4;
                    } break;
                }
            } break;

            // Branch
            case OC_BRANCH: {
                switch (mod) {
                    case MOD_BRANCH_0: {
                        GPR[PC] = GPR[ra] + disp;
                    } break;

                    case MOD_BRANCH_1: {
                        if (GPR[rb] == GPR[rc])
                            GPR[PC] = GPR[ra] + disp;
                    } break;

                    case MOD_BRANCH_2: {
                        if (GPR[rb] != GPR[rc])
                            GPR[PC] = GPR[ra] + disp;
                    } break;

                    case MOD_BRANCH_3: {
                        if ((int) GPR[rb] > (int)GPR[rc])
                            GPR[PC] = GPR[ra] + disp;
                    } break;

                    case MOD_BRANCH_4: {
                        GPR[PC] = Mem_ReadWord(GPR[ra] + disp);
                    } break;

                    case MOD_BRANCH_5: {
                        if (GPR[rb] == GPR[rc])
                            GPR[PC] = Mem_ReadWord(GPR[ra] + disp);
                    } break;

                    case MOD_BRANCH_6: {
                        if (GPR[rb] != GPR[rc])
                            GPR[PC] = Mem_ReadWord(GPR[ra] + disp);
                    } break;

                    case MOD_BRANCH_7: {
                        if ((int)GPR[rb] > (int)GPR[rc])
                            GPR[PC] = Mem_ReadWord(GPR[ra] + disp);
                    } break;
                }
            } break;

            // Xchg
            case OC_XCHG: {
                Asm_Word temp = GPR[rb];
                GPR[rb] = GPR[rc];
                GPR[rc] = temp;
            } break;

            // Arithmetic
            case OC_ARITH: {
                switch (mod) {
                    case MOD_ARITH_ADD: { 
                        GPR[ra] = GPR[rb] + GPR[rc]; 
                    } break;

                    case MOD_ARITH_SUB: { 
                        GPR[ra] = GPR[rb] - GPR[rc]; 
                    } break;

                    case MOD_ARITH_MUL: { 
                        GPR[ra] = GPR[rb] * GPR[rc]; 
                    } break;

                    case MOD_ARITH_DIV: { 
                        GPR[ra] = GPR[rb] / GPR[rc]; 
                    } break;
                }
            } break;

            // Logic
            case OC_LOGIC: {
                switch (mod) {
                    case MOD_LOGIC_NOT: { 
                        GPR[ra] = ~GPR[rb];
                    }  break;

                    case MOD_LOGIC_AND: { 
                        GPR[ra] = GPR[rb] & GPR[rc];
                    } break;

                    case MOD_LOGIC_OR:  { 
                        GPR[ra] = GPR[rb] | GPR[rc];
                    } break;

                    case MOD_LOGIC_XOR: { 
                        GPR[ra] = GPR[rb] ^ GPR[rc];
                    } break;
                }
            } break;

            // Shift
            case OC_SHIFT: {
                switch (mod) {
                    case MOD_SHIFT_LEFT:  {
                        GPR[ra] = GPR[rb] << GPR[rc];
                    } break;

                    case MOD_SHIFT_RIGHT: {
                        GPR[ra] = GPR[rb] >> GPR[rc];
                    } break;
                }
            } break;

            // Store
            case OC_STORE: {
                switch (mod) {
                    case MOD_STORE_0: {
                        Mem_WriteWord(GPR[ra] + GPR[rb] + disp, GPR[rc]);
                    } break;

                    case MOD_STORE_1: {
                        Mem_WriteWord(Mem_ReadWord(GPR[ra] + GPR[rb] + disp), GPR[rc]);
                    } break;

                    case MOD_STORE_2: {
                        GPR[ra] = GPR[ra] + disp;
                        Mem_WriteWord(GPR[ra], GPR[rc]);
                    } break;
                }
            } break;

            // Load
            case OC_LOAD: {
                switch (mod) {
                    case MOD_LOAD_0: {
                        GPR[ra] = CSR[rb];
                    } break;

                    case MOD_LOAD_1: {
                        GPR[ra] = GPR[rb] + disp;
                    } break;

                    case MOD_LOAD_2: {
                        GPR[ra] = Mem_ReadWord(GPR[rb] + GPR[rc] + disp);
                    } break;

                    case MOD_LOAD_3: {
                        GPR[ra] = Mem_ReadWord(GPR[rb]);
                        GPR[rb] = GPR[rb] + disp;
                    } break;


                    case MOD_LOAD_4: {
                        CSR[ra] = GPR[rb];
                    } break;

                    case MOD_LOAD_5: {
                        CSR[ra] = CSR[rb] | disp;
                    } break;

                    case MOD_LOAD_6: {
                        CSR[ra] = Mem_ReadWord(GPR[rb] + GPR[rc] + disp);
                    } break;

                    case MOD_LOAD_7: {
                        CSR[ra] = Mem_ReadWord(GPR[rb]);
                        GPR[rb] = GPR[rb] + disp;
                    } break;

                }
            } break;

            // Locking
            case OC_LOCK: {
                switch (mod) {
                    case MOD_LOCK_LOCK: {
                        INT_ENA = 0;
                        LCK_LVL += 1;
                    } break;

                    case MOD_LOCK_UNLOCK: {
                        if (LCK_LVL > 0) {
                            LCK_LVL -= 1;
                            if (LCK_LVL == 0) {
                                INT_ENA = 1;
                            }
                        }
                    } break;
                }
            } break;

            // Probing
            case OC_PROBE: {
                // probe $lit
                switch (mod) {
                    // probe $lit
                    case MOD_PROBE_LIT: {
                        fprintf(output, "Probe lit: %d\n", disp);
                        fflush(output);
                    } break;

                    // probe %reg
                    case MOD_PROBE_REG: {
                        fprintf(output, "Probe reg: r%d=0x%08x\n", ra, GPR[ra]);
                        fflush(output);
                    } break;

                    // probe [%reg + disp]
                    case MOD_PROBE_MEM_REG_DISP: {
                        fprintf(output, "Probe mem reg: mem32[r%d=0x%08x + %d]=0x%08x\n", ra, GPR[ra], disp, Mem_ReadWord(GPR[ra] + disp));
                        fflush(output);
                    } break;

                    // probe all
                    case MOD_PROBE_ALL: {
                        Emu_PrintRegs(output);
                        fflush(output);
                    } break;
                }
            }

            // Unknown instruction (interrupt)
            default: {
                INT[INT_INSTRUCTION] = 1;
            } break;
        }

        // Process interrupts (but only if handler is set)
        Emu_IntType idx;
        if (CSR[HANDLER] != 0x0 && INT_ENA) {
            Emu_MutexLock();
            for (idx = 0; idx < INT_NONE; idx++) {
                if (! INT[idx])
                    continue;
                Asm_Word status = CSR[STATUS];
                Asm_Word cause = CAUSE_UNKNOWN;
                switch (idx) {
                    // Bad instruction interrupt
                    case INT_INSTRUCTION:
                    {
                        cause = CAUSE_INSTRUCTION;
                    } break;

                    // Software interrupt
                    case INT_SOFTWARE: 
                    {
                        //status = status & (~0x1); 
                        cause = CAUSE_SOFTWARE;
                    } break;

                    // Timer interrupt
                    case INT_TIMER:
                    {
                        if ((CSR[STATUS] & ST_MASK_INTERRUPTS) || (CSR[STATUS] & ST_MASK_TIMER)) 
                            continue;
                        status |= ST_MASK_INTERRUPTS;
                        cause = CAUSE_TIMER;
                    } break;

                    // Terminal interrupt
                    case INT_TERMINAL:
                    {
                        // Check if maksed
                        if ((CSR[STATUS] & ST_MASK_INTERRUPTS) || (CSR[STATUS] & ST_MASK_TERMINAL)) 
                            continue;

                        // Set cause and mask interrupts
                        status |= ST_MASK_INTERRUPTS;
                        cause = CAUSE_TERMINAL;
                    } break;
                }

                // Only 
                if (cause != CAUSE_UNKNOWN) {
                    // Jump to handler
                    // push idx;
                    // push adr;
                    // push status; 
                    // push pc; 
                    // cause <= 4; 
                    // status <= status & (~0x1); 
                    // pc <= handle;
                    GPR[SP] -= 4;
                    Mem_WriteRawWord(GPR[SP], GPR[PC]);
                    GPR[SP] -= 4;
                    Mem_WriteRawWord(GPR[SP], CSR[STATUS]);
                    GPR[SP] -= 4;
                    Mem_WriteRawWord(GPR[SP], GPR[ADR]);
                    GPR[SP] -= 4;
                    Mem_WriteRawWord(GPR[SP], GPR[IDX]);
                    CSR[CAUSE] = cause;
                    CSR[STATUS] = status;
                    GPR[PC] = CSR[HANDLER];
                    INT[idx] = 0;
                    if (flags & F_EMU_DEBUG) {
                        fprintf(stderr, "\e[31mInterrupt\e[0m %d\n", cause);
                    }
                    break;
                }
            }
            Emu_MutexUnlock();
        }
    }

    // Cancel terminal and timer
    if (flags & F_EMU_TIMER)
        Emu_StopTimer();
    if (flags & F_EMU_TERMINAL)
        Emu_StopTerminal();
    if (flags & F_EMU_TIMER)
        Emu_JoinTimer();
    if (flags & F_EMU_TERMINAL)
        Emu_JoinTerminal();

    // Destroy mutex
    Emu_MutexDestroy();

    // Print registers
    Emu_PrintRegs(output);
    fflush(output);
}

// Print Emulator registers
void Emu_PrintRegs(FILE *output)
{
    fprintf(output, "-----------------------------------------------------------\n");
    printf("Emulated processor executed halt instruction\n");
    printf("Emulated processor state:\n");
    fprintf(output, " r0=0x%08x  r1=0x%08x  r2=0x%08x  r3=0x%08x\n",  GPR[0],  GPR[1],  GPR[2],  GPR[3]);
    fprintf(output, " r4=0x%08x  r5=0x%08x  r6=0x%08x  r7=0x%08x\n",  GPR[4],  GPR[5],  GPR[6],  GPR[7]);
    fprintf(output, " r8=0x%08x  r9=0x%08x r10=0x%08x r11=0x%08x\n",  GPR[8],  GPR[9], GPR[10], GPR[11]);
    fprintf(output, "r12=0x%08x r13=0x%08x r14=0x%08x r15=0x%08x\n", GPR[12], GPR[13], GPR[14], GPR[15]);
    fprintf(output, "\nstatus  = %08x\nhandler = %08x\ncause   = %08x\n", CSR[STATUS], CSR[HANDLER], CSR[CAUSE]);
    fprintf(output, "-----------------------------------------------------------\n");
}

static void show_help(FILE* file) 
{
    const char *help_text = 
        "Usage: emulator [HEX FILE]\n"
    ;
    fprintf(file, "%s", help_text);

}


int main(int argc, char *argv[])
{
    if (argc != 2)  {
        show_help(stdout);
        return 0;
    }

    // Read elf
    FILE *input = NULL;
    const char *input_name = argv[1];
    Emu_FlagsType flags = 
          F_EMU_NONE 
        | F_EMU_TIMER
        // | F_EMU_TERMINAL
        // | F_EMU_DEBUG

    ;
    if (Str_Equals(input_name, "-")) {
        input = stdin;
    } else {
        input = fopen(input_name, "r");
        if (input == NULL)  {
            fprintf(stderr, "Error (Emulator): Failed to open file '%s' for reading\n", input_name);
            return 1;
        }
    }

    Elf_Builder elf;
    Elf_Init(&elf);
    Elf_ReadHexInit(&elf, input);
    Emu_RunElf(&elf, stdout, flags);

    // For testing
    //Elf_WriteDump(&elf, stdout);

    Mem_Clear();
    return 0;
}
