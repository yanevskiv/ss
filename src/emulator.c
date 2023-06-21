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
