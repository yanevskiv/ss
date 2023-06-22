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
volatile Asm_Word Emu_TermBuffer = '\0';

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

static void show_help(FILE* file) 
{
    const char *help_text = 
        "Usage: emulator [HEX FILE]\n"
    ;
    fprintf(file, "%s", help_text);

}


int main(int argc, char *argv[])
{
    // Read elf
    FILE *input = NULL;
    //const char *input_name = argv[1];
    Emu_FlagsType flags = 
          F_EMU_NONE 
        // | F_EMU_TIMER
        // | F_EMU_TERMINAL
        // | F_EMU_DEBUG

    ;
    int i; 
    for (i = 1; i < argc; i++)  {
        if (Str_Equals(argv[i], "-timer")) {
            flags |= F_EMU_TIMER;
        } else if (Str_Equals(argv[i], "-terminal")) {
            flags |= F_EMU_TERMINAL;
        } else if (Str_Equals(argv[i], "-debug")) {
            flags |= F_EMU_DEBUG;
        } else if (Str_Equals(argv[i], "-")) {
            if (! input) {
                input = stdin;
            } else {
                Show_Warning("Multiple input files provided (Only first one used)");
            }
        } else {
            if (! input) {
                input = fopen(argv[i], "r");
                if (input == NULL)  {
                    fprintf(stderr, "Error (Emulator): Failed to open file '%s' for reading\n", argv[i]);
                    return 1;
                }
            } else {
                Show_Warning("Multiple input files provided (Only first one used)");
            }
        }
    }


    Elf_Builder elf;
    Elf_ReadHexInit(&elf, input);
    Emu_RunElf(&elf, stdout, flags);

    // For testing
    //Elf_WriteDump(&elf, stdout);

    Elf_Destroy(&elf);

    Mem_Clear();

    if (input && input != stdin)
        fclose(input);
        
    return 0;
}
