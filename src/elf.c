#include <assert.h>
#include <stdlib.h>
#include <string.h>
#include <elf.h>
#include <regex.h>


void Elf_Init(Elf_Builder *elf)
{
    // e_ident
	elf->eb_ehdr.e_ident[EI_MAG0] = 0x7f;
	elf->eb_ehdr.e_ident[EI_MAG1] = 'E';
	elf->eb_ehdr.e_ident[EI_MAG2] = 'L';
	elf->eb_ehdr.e_ident[EI_MAG3] = 'F';
	elf->eb_ehdr.e_ident[EI_CLASS] = ELFCLASS64;
	elf->eb_ehdr.e_ident[EI_DATA] = ELFDATA2LSB;
	elf->eb_ehdr.e_ident[EI_VERSION] = EV_CURRENT;
	elf->eb_ehdr.e_ident[EI_OSABI] = ELFOSABI_GNU;
	elf->eb_ehdr.e_ident[EI_ABIVERSION] = 0x00;
    int i;
    for (i = EI_PAD; i < EI_NIDENT; i++) 
        elf->eb_ehdr.e_ident[i] = 0x00;
    
    // other e_'s
	elf->eb_ehdr.e_type = ET_REL; // Relocatable
	elf->eb_ehdr.e_machine = EM_X86_64; // x86_64 machine
	elf->eb_ehdr.e_version = EV_CURRENT; // Current version
	elf->eb_ehdr.e_entry = 0x0;
	elf->eb_ehdr.e_phoff = 0x0;
	elf->eb_ehdr.e_shoff = 0x0;
	elf->eb_ehdr.e_flags = 0;
	elf->eb_ehdr.e_ehsize = sizeof(Elf_Ehdr);
	elf->eb_ehdr.e_phentsize = 0;
	elf->eb_ehdr.e_phnum = 0;
	elf->eb_ehdr.e_shentsize = 0;
	elf->eb_ehdr.e_shnum = 0;
	elf->eb_ehdr.e_shstrndx = SHN_UNDEF;

    // Init setions
    elf->eb_section_head = 0;
    elf->eb_shdr_size = ELF_MAX_SECTIONS;
    elf->eb_shdr = calloc(elf->eb_shdr_size, sizeof(Elf_Shdr));
    assert(elf->eb_shdr != NULL);
    elf->eb_buffer = calloc(elf->eb_shdr_size, sizeof(Elf_Buffer));
    assert(elf->eb_buffer != NULL);
    for (int i = 0; i < elf->eb_shdr_size; i++) {
        Elf_Buffer_Init(&elf->eb_buffer[i]);
    }
    elf->eb_current_section = SHN_UNDEF;

    // Init .strtab
	elf->eb_ehdr.e_shstrndx = 1; // Set .shstrtab section index
    elf->eb_ehdr.e_shnum += 1;    // We've added one more section to elf
    Elf_Shdr *strtab = &(elf->eb_shdr[elf->eb_ehdr.e_shstrndx]); // Get section info

    strtab->sh_name = 0x1; // ".strtab" is located at position 1 (first string is just '\0')
    strtab->sh_type = SHT_STRTAB; // Set type to string table
    strtab->sh_flags = SHF_STRINGS;  // Section contains strings
    strtab->sh_addr = 0;   // No virtual address
    strtab->sh_offset = 0; // NOTE: Remember to set this when compiling
    strtab->sh_size = 0;   // Data is completely empty for now
    strtab->sh_link = 0;   // No link
    strtab->sh_info = 0;   // No info
    strtab->sh_addralign = 0; // No alignment
    strtab->sh_entsize = 0; // Not an entry table 

    Elf_PushSection(elf);
    Elf_SetSection(elf, elf->eb_ehdr.e_shstrndx);
    Elf_PushByte(elf, '\0'); // First character
    Elf_PushString(elf, ".strtab");
    Elf_PopSection(elf);

    // Init .symtab
    elf->eb_ehdr.e_shnum += 1;
    Elf_Shdr *symtab = &(elf->eb_shdr[elf->eb_ehdr.e_shnum]);
    symtab->sh_type = SHT_SYMTAB; // Set type to string table
    symtab->sh_flags = 0;  // No flags
    symtab->sh_addr = 0;   // No virtual address
    symtab->sh_offset = 0; // NOTE: Remember to set this when compiling
    symtab->sh_size = 0;   // Data is completely empty 
    symtab->sh_link = 0;   // No link
    symtab->sh_info = 0;   // No info
    symtab->sh_addralign = 0; // No alignment
    symtab->sh_entsize = sizeof(Elf_Sym); // Not an entry table 

    // set .symtab name
    Elf_PushSection(elf);
    Elf_SetSection(elf, elf->eb_ehdr.e_shstrndx);
    symtab->sh_name = (Elf_Word)Elf_GetSection(elf)->sh_size;
    Elf_PushString(elf, ".symtab");
    Elf_PopSection(elf);

    // Add symbols for .strtab and .symtab sections
    Elf_PushSection(elf);
    Elf_UseSection(elf, ".symtab");

    // Undefined symbol
    Elf_Sym sym;
    sym.st_name = 0;  // No name
    sym.st_info = 0;  // No binding or tupe
    sym.st_other = 0; // No visibility (local)
    sym.st_shndx = 0; // No section
    sym.st_value = 0; // No value (addr)
    sym.st_size = 0;  // No size
    Elf_PushBytes(elf, &sym, sizeof(sym));

    // .strtab
    sym.st_name = strtab->sh_name;
    sym.st_info = ELF_ST_INFO(STB_LOCAL, STT_SECTION);
    sym.st_other = STV_DEFAULT;
    sym.st_shndx = 0x1; // Index 1
    sym.st_value = 0;
    sym.st_size = 0;
    Elf_PushBytes(elf, &sym, sizeof(sym));

    // .symtab
    sym.st_name = symtab->sh_name;
    sym.st_info = ELF_ST_INFO(STB_LOCAL, STT_SECTION);
    sym.st_other = STV_DEFAULT;
    sym.st_shndx = 0x2; // Index 1
    sym.st_value = 0;
    sym.st_size = 0;
    Elf_PushBytes(elf, &sym, sizeof(sym));

    Elf_PopSection(elf);

}

void Elf_Destroy(Elf_Builder *elf)
{
    for (int i = 0; i < elf->eb_shdr_size; i++) {
        Elf_Buffer_Destroy(&elf->eb_buffer[i]);
    }
    free(elf->eb_buffer);
}

void Elf_Buffer_Destroy(Elf_Buffer *buffer)
{
    free(buffer->eb_data);
}

Elf_Shdr *Elf_CreateSection(Elf_Builder *elf, const char *name)
{
    // Assign section
    elf->eb_ehdr.e_shnum += 1; 
    Elf_Word ndx = elf->eb_ehdr.e_shnum;
    Elf_Shdr *shdr = &(elf->eb_shdr[ndx]);

    // Add section name to ".strtab" and set `shdr->sh_name`
    //Elf_Word namendx = Elf_AddUniqueString(elf, name);
    //shdr->sh_name = namendx;

    /*
    Elf_PushSection(elf);
    Elf_SetSection(elf, elf->eb_ehdr.e_shstrndx);
    Elf_Word namendx = (Elf_Word)Elf_GetSection(elf)->sh_size;
    shdr->sh_name = namendx;
    Elf_PushString(elf, name);
    Elf_PopSection(elf);
    */

    // Add symbol 
    Elf_Sym *sym = Elf_CreateSymbol(elf, name);
    shdr->sh_name = Elf_FindString(elf, name); 
    sym->st_name = shdr->sh_name;
    sym->st_info = ELF_ST_INFO(STB_LOCAL, STT_SECTION); 
    sym->st_other = STV_DEFAULT;
    sym->st_shndx = ndx;
    sym->st_value = 0;
    sym->st_size = 0;

    /*
    Elf_PushSection(elf);
    Elf_UseSection(elf, ".symtab");
    Elf_Sym sym;
    sym.st_name = namendx; 
    sym.st_info = ELF_ST_INFO(STB_LOCAL, STT_SECTION); 
    sym.st_other = STV_DEFAULT;
    sym.st_shndx = ndx;
    sym.st_value = 0;
    sym.st_size = 0;
    Elf_PushBytes(elf, &sym, sizeof(sym));
    Elf_PopSection(elf);
    */

    // Set default values
    shdr->sh_flags = 0;  // No flags
    shdr->sh_addr = 0;   // No virtual address
    shdr->sh_offset = 0; // NOTE: Remember to set this when compiling
    shdr->sh_size = 0;   // Data is completely empty 
    shdr->sh_link = 0;   // No link
    shdr->sh_info = 0;   // No info
    shdr->sh_addralign = 0; // No alignment
    shdr->sh_entsize = 0; // Not an entry table 

    // Return
    return shdr;
}


Elf_Shdr *Elf_SetSection(Elf_Builder *elf, Elf_Word id)
{
    elf->eb_current_section = id;
    return Elf_GetSection(elf);
}

void Elf_PushSection(Elf_Builder *elf)
{
    elf->eb_section_stack[elf->eb_section_head] = 
        elf->eb_current_section;
    elf->eb_section_head += 1;
}

void Elf_PopSection(Elf_Builder *elf)
{
    elf->eb_section_head -= 1;
    elf->eb_current_section = elf->eb_section_stack[elf->eb_section_head];
}

static void Elf_Buffer_CheckGrow(Elf_Buffer *buffer, size_t want) {
    while ((buffer->eb_size + want) >= buffer->eb_capacity) {
        buffer->eb_capacity *= 2;
    }
    buffer->eb_data = realloc(buffer->eb_data, (buffer->eb_capacity) * sizeof(unsigned char));
    assert(buffer->eb_data != NULL);
}

void Elf_Buffer_Init(Elf_Buffer* buffer) {
    buffer->eb_capacity = 10;
    buffer->eb_size = 0;
    buffer->eb_data = calloc(buffer->eb_capacity, sizeof(unsigned char));
    assert(buffer->eb_data != NULL);
}

void Elf_Buffer_PushByte(Elf_Buffer* buffer, unsigned char byte)
{
    Elf_Buffer_CheckGrow(buffer, sizeof(unsigned char));
    buffer->eb_data[buffer->eb_size] = byte;
    buffer->eb_size += 1;
}

void Elf_Buffer_PushString(Elf_Buffer* buffer, const char *str)
{
    while (*str) {
        Elf_Buffer_PushByte(buffer, *str);
        str ++;
    }
    Elf_Buffer_PushByte(buffer, '\0');
}

void Elf_Buffer_PushBytes(Elf_Buffer* buffer, void *bytes, size_t size)
{
    size_t i;
    unsigned char *data = (unsigned char*) bytes;
    for (int i = 0; i < size; i++)  {
        Elf_Buffer_PushByte(buffer, data[i]);
    }
}

void Elf_Buffer_PushSkip(Elf_Buffer* buffer, Elf_Word count, Elf_Byte fill)
{
    while (count > 0) {
        Elf_Buffer_PushByte(buffer, fill);
        count -= 1;
    }
}

Elf_Shdr* Elf_GetSectionAt(Elf_Builder *elf, int at)
{
    if (at < 0 || at >= Elf_GetSectionCount(elf))
        return NULL;
    return &(elf->eb_shdr[at]);
}

int Elf_GetSectionCount(Elf_Builder *elf)
{
    return elf->eb_ehdr.e_shnum;
}


void Elf_SetSectionByName(Elf_Builder *elf, const char *name)
{
}

Elf_Shdr *Elf_GetSection(Elf_Builder *elf)
{
    return &(elf->eb_shdr[elf->eb_current_section]);
}

Elf_Buffer *Elf_GetBuffer(Elf_Builder *elf)
{
    return &(elf->eb_buffer[elf->eb_current_section]);
}

void Elf_PushByte(Elf_Builder *elf, unsigned char byte)
{
    Elf_Buffer_PushByte(Elf_GetBuffer(elf), byte);
    Elf_GetSection(elf)->sh_size = Elf_GetBuffer(elf)->eb_size;
}

void Elf_PushString(Elf_Builder *elf, const char *str)
{
    Elf_Buffer_PushString(Elf_GetBuffer(elf), str);
    Elf_GetSection(elf)->sh_size = Elf_GetBuffer(elf)->eb_size;
}

void Elf_PushBytes(Elf_Builder *elf, void *bytes, size_t size)
{
    Elf_Buffer_PushBytes(Elf_GetBuffer(elf), bytes, size);
    Elf_GetSection(elf)->sh_size = Elf_GetBuffer(elf)->eb_size;
}

void Elf_PushHalf(Elf_Builder *elf, Elf_Half half)
{
    Elf_Buffer_PushHalf(Elf_GetBuffer(elf), half);
    Elf_GetSection(elf)->sh_size = Elf_GetBuffer(elf)->eb_size;
}

void Elf_PushSkip(Elf_Builder *elf, Elf_Word count, Elf_Byte fill)
{
    Elf_Buffer_PushSkip(Elf_GetBuffer(elf), count, fill);
    Elf_GetSection(elf)->sh_size = Elf_GetBuffer(elf)->eb_size;
}

const char *Elf_GetSectionName(Elf_Builder *elf)
{
    Elf_Shdr* shdr = Elf_GetSection(elf);
    Elf_PushSection(elf);
    Elf_SetSection(elf, elf->eb_ehdr.e_shstrndx);
    const char *result = (const char*)(Elf_GetBuffer(elf)->eb_data + shdr->sh_name);
    Elf_PopSection(elf);
    return result;
}

Elf_Section Elf_FindSection(Elf_Builder *elf, const char *name)
{
    Elf_Word index;
    Elf_PushSection(elf);
    int found = 0;
    for (index = 1; index <= Elf_GetSectionCount(elf); index++) {
        Elf_SetSection(elf, index);
        if (strcmp(Elf_GetSectionName(elf), name) == 0) {
            found = 1;
            break;
        }
    }
    Elf_PopSection(elf);
    if (! found)
        return 0;
    return index;
}

int Elf_SectionExists(Elf_Builder *elf, const char *name)
{
    return Elf_FindSection(elf, name) != 0;
}

Elf_Shdr *Elf_UseSection(Elf_Builder *elf, const char *name)
{
    if (! Elf_SectionExists(elf, name))
        Elf_CreateSection(elf, name);
    Elf_SetSection(elf, Elf_FindSection(elf, name));
    return Elf_GetSection(elf);
}

Elf_Xword Elf_GetSectionSize(Elf_Builder *elf)
{
    return Elf_GetBuffer(elf)->eb_size;
}

unsigned char *Elf_GetSectionData(Elf_Builder *elf)
{
    return Elf_GetBuffer(elf)->eb_data;
}

Elf_Sym *Elf_GetSymbol(Elf_Builder *elf, int index)
{
    Elf_PushSection(elf);
    Elf_UseSection(elf, ".symtab");
    unsigned char *data = Elf_GetSectionData(elf);
    Elf_PopSection(elf);
    return (Elf_Sym*)(data + index * sizeof(Elf_Sym));
}

const char *Elf_GetSymbolName(Elf_Builder *elf, int index)
{
    Elf_Sym *sym = Elf_GetSymbol(elf, index);
    Elf_PushSection(elf);
    Elf_UseSection(elf, ".strtab");
    char *result = Elf_GetSectionData(elf) + sym->st_name;
    Elf_PopSection(elf);
    return result;
}

Elf_Sym *Elf_CreateSymbol(Elf_Builder *elf, const char *name)
{
    // Get current section index
    Elf_Section section_ndx = elf->eb_current_section;

    // Add symbol name to string table (.strtab)
    //Elf_PushSection(elf);
    //Elf_UseSection(elf, ".strtab");
    //Elf_Word name_ndx = (Elf_Word)Elf_GetSection(elf)->sh_size;
    //Elf_PushString(elf, name);
    //Elf_PopSection(elf);
    Elf_Word name_ndx = Elf_AddUniqueString(elf, name);

    // Add symbol to symbol table (.symtab)
    Elf_PushSection(elf);
    Elf_UseSection(elf, ".symtab");
    // Remember where the symbol is located
    Elf_Word off = Elf_GetSectionSize(elf);
    Elf_Sym sym;
    sym.st_name = name_ndx; 
    sym.st_info = ELF_ST_INFO(STB_LOCAL, STT_NOTYPE); 
    sym.st_other = STV_DEFAULT;
    sym.st_shndx = section_ndx;
    sym.st_value = 0;
    sym.st_size = 0;
    Elf_PushBytes(elf, &sym, sizeof(sym));
    //Get symbol
    Elf_Sym *ret = (Elf_Sym*)(Elf_GetSectionData(elf) + off);
    Elf_PopSection(elf);

    return ret;
}

Elf_Shdr *Elf_FetchSection(Elf_Builder *elf, const char *name)
{
    
    Elf_PushSection(elf);
    Elf_Word ndx = Elf_FindSection(elf, name);
    Elf_SetSection(elf, ndx);
    Elf_Shdr *shdr = Elf_GetSection(elf);
    Elf_PopSection(elf);
    return shdr;
}

int Elf_GetSymbolCount(Elf_Builder *elf)
{
    Elf_PushSection(elf);
    Elf_UseSection(elf, ".symtab");
    Elf_Shdr *shdr = Elf_GetSection(elf);
    int count = shdr->sh_size / shdr->sh_entsize;
    Elf_PopSection(elf);
    return count;
}

Elf_Word Elf_FindSymbol(Elf_Builder *elf, const char *name)
{
    Elf_Word strtab_ndx = Elf_FindSection(elf, ".strtab");
    Elf_Word symtab_ndx = Elf_FindSection(elf, ".symtab");

    // Get strings
    Elf_PushSection(elf);
    Elf_SetSection(elf, strtab_ndx);
    const char *strings = (const char *) Elf_GetSectionData(elf);
    Elf_PopSection(elf);

    Elf_Word i, size = Elf_GetSymbolCount(elf);
    for (i = 0; i < size; i++) {
        Elf_Sym *sym = Elf_GetSymbol(elf, i);
        if (strcmp(strings + sym->st_name, name) == 0) {
            return i;
        }
    }
    return 0;
}

int Elf_SymbolExists(Elf_Builder *elf, const char *name)
{
    return Elf_FindSymbol(elf, name) != 0;
}


Elf_Sym *Elf_FetchSymbol(Elf_Builder *elf, const char *name)
{
    return Elf_GetSymbol(elf, Elf_FindSymbol(elf, name));
}

Elf_Sym *Elf_UseSymbol(Elf_Builder *elf, const char *name)
{
    if (! Elf_SymbolExists(elf, name))
        return Elf_CreateSymbol(elf, name);
    return Elf_FetchSymbol(elf, name);
}

Elf_Section Elf_GetCurrentSection(Elf_Builder *elf)
{
    return elf->eb_current_section;
}


void Elf_Buffer_PushHalf(Elf_Buffer* buffer, Elf_Half half)
{
    Elf_Buffer_PushByte(buffer, half & 0xff);
    Elf_Buffer_PushByte(buffer, half >> 8);
}

int Elf_GetRelaCount(Elf_Builder* elf)
{
    char rela_name[ELF_MAX_SECTION_NAME];
    sprintf(rela_name, ".rela%s", Elf_GetSectionName(elf));
    if (! Elf_SectionExists(elf, rela_name))
        return 0;
    Elf_PushSection(elf);
    int count = Elf_GetSectionSize(elf) / sizeof(Elf_Rela);
    Elf_PopSection(elf);
    return count;
}

static void Elf_UseRelaSection(Elf_Builder *elf)
{
    char rela_name[ELF_MAX_SECTION_NAME];
    sprintf(rela_name, ".rela%s", Elf_GetSectionName(elf));
    if (! Elf_SectionExists(elf, rela_name)) {
        // Create rela section if it doesn't exist
        Elf_Shdr *shdr = Elf_CreateSection(elf, rela_name);
        shdr->sh_type = SHT_RELA; // Set type to string table
        shdr->sh_flags = 0;  // No flags
        shdr->sh_addr = 0;   // No virtual address
        shdr->sh_offset = 0; // NOTE: Remember to set this when compiling
        shdr->sh_size = 0;   // Data is completely empty 
        shdr->sh_link = Elf_GetCurrentSection(elf); // Links to section that contains relas
        shdr->sh_info = 0;   // No info
        shdr->sh_addralign = 0; // No alignment
        shdr->sh_entsize = sizeof(Elf_Rela); // Not an entry table 
    }
    Elf_UseSection(elf, rela_name);
}

Elf_Rela *Elf_AddRela(Elf_Builder *elf, Elf_Word symndx)
{
    char rela_name[ELF_MAX_SECTION_NAME];
    sprintf(rela_name, ".rela%s", Elf_GetSectionName(elf));
    
    Elf_PushSection(elf);
    Elf_UseRelaSection(elf);

    // Add rela
    Elf_Rela *ret = (Elf_Rela*) (Elf_GetSectionData(elf) + Elf_GetSectionSize(elf));
    Elf_Rela rela;
    rela.r_offset = 0;
    rela.r_info = ELF_R_INFO(symndx, R_X86_64_NONE);
    rela.r_addend = 0;
    Elf_PushBytes(elf, &rela, sizeof(rela));
    Elf_PopSection(elf);
    return ret;
}

Elf_Rela *Elf_AddRelaSymb(Elf_Builder *elf, const char *symb) 
{
    if (! Elf_SymbolExists(elf, symb))
        Elf_CreateSymbol(elf, symb);
    Elf_Word symndx = Elf_FindSymbol(elf, symb);
    Elf_AddRela(elf, symndx);
}

void *Elf_GetSectionEntry(Elf_Builder *elf, Elf_Word at) 
{
    Elf_Word off = at * Elf_GetSection(elf)->sh_entsize;
    if (off > Elf_GetSectionSize(elf))
        return NULL;
    return (void*)(Elf_GetSectionData(elf) + off);
}


static const char *_Elf_GetRelaTypeName(Elf_Word type)
{
    return 
      type == R_X86_64_NONE      ? "R_X86_64_NONE"
    : type == R_X86_64_64        ? "R_X86_64_64"
    : type == R_X86_64_PC32      ? "R_X86_64_PC32"
    : type == R_X86_64_GOT32     ? "R_X86_64_GOT32"
    : type == R_X86_64_PLT32     ? "R_X86_64_PLT32"
    : type == R_X86_64_COPY      ? "R_X86_64_COPY"
    : type == R_X86_64_GLOB_DAT  ? "R_X86_64_GLOB_DAT"
    : type == R_X86_64_JUMP_SLOT ? "R_X86_64_JUMP_SLOT"
    : type == R_X86_64_RELATIVE  ? "R_X86_64_RELATIVE"
    : type == R_X86_64_GOTPCREL  ? "R_X86_64_GOTPCREL"
    : type == R_X86_64_32        ? "R_X86_64_32"
    : type == R_X86_64_32S       ? "R_X86_64_32S"
    : type == R_X86_64_16        ? "R_X86_64_16"
    : type == R_X86_64_PC16      ? "R_X86_64_PC16"
    : type == R_X86_64_8         ? "R_X86_64_8"
    : "R_UNKNOWN";
}

// Used for updating sh_offset in header files
static void _Elf_PrepareForWriting(Elf_Builder *elf)
{
    int i;
    Elf_Off off = sizeof(Elf_Ehdr);

    /* TODO: include program table */

    // Sections
    for (i = 1; i <= Elf_GetSectionCount(elf); i++) {
        Elf_PushSection(elf);
        Elf_SetSection(elf, i);
        Elf_GetSection(elf)->sh_offset = off;
        off += Elf_GetSection(elf)->sh_size;
        Elf_PopSection(elf);
    }
    elf->eb_ehdr.e_shoff = off;
}


#define NORMAL 0
#define FAINT 2
#define RED   31
#define GREEN 32
#define YELLOW 33
#define BLUE 34


static size_t _WriteHexBytesColor(void *data, size_t count, size_t index, FILE *output, int color)
{
    unsigned char *bytes = data;
    int i = 0, j = 0;
    const int row_count = 16;
    if (color > 0)
        fprintf(output, "\e[%dm", color);
    for (j = 0; j <= count / row_count; j++) {
        for (i = 0; i < row_count && i + j * row_count < count; i++, index++) {
            if (index % row_count == 0) {
                if (color > 0)
                    fprintf(output, "\e[0m", color);
                if (index != 0)
                    fprintf(output, "\n");
                fprintf(output, "%04d: ", index);
                if (color > 0)
                    fprintf(output, "\e[%dm", color);
            } else if (index % 4 == 0) {
                fprintf(output, " ");
            }
            fprintf(output, "%02x " , bytes[i + j * row_count]);
        }
    }
    if (color > 0)
        fprintf(output, "\e[0m", color);
    return count;
}

static size_t _WriteHexBytes(void *data, size_t count, size_t index, FILE *output)
{
    _WriteHexBytesColor(data, count, index, output, 0);
}



void Elf_WriteHexColor(Elf_Builder *elf, FILE *output, int color)
{
    _Elf_PrepareForWriting(elf);
    size_t index = 0;

    // Write ELF header
    index += _WriteHexBytesColor(&elf->eb_ehdr, sizeof(elf->eb_ehdr), index, output, color ? RED : 0);

    // Write sections data
    int i;
    for (i = 1; i <= Elf_GetSectionCount(elf); i++) {
        Elf_PushSection(elf);
        Elf_SetSection(elf, i);
        index += _WriteHexBytesColor(
            Elf_GetSectionData(elf),
            Elf_GetSectionSize(elf),
            index,
            output,
            color ? ((i % 2 == 0) ? NORMAL : FAINT) : 0
        );
        Elf_PopSection(elf);
    }

    // Write section header
    for (i = 0; i <= Elf_GetSectionCount(elf); i++) {
        Elf_PushSection(elf);
        Elf_SetSection(elf, i);
        index += _WriteHexBytesColor(
            Elf_GetSection(elf),
            sizeof(Elf_Shdr),
            index,
            output,
            color ? ((i % 2 == 0) ? YELLOW : GREEN) : 0
        );
        Elf_PopSection(elf);
    }
}

void Elf_WriteHex(Elf_Builder *elf, FILE *output)
{
    Elf_WriteHexColor(elf, output, 0);
}

#define ARR_SIZE(arr) (sizeof((arr)) / sizeof((arr)[0]))


void _Elf_Buffer_ReadHexBytes(Elf_Buffer *buffer, FILE *input)
{
    regex_t re_hexline; // Matches comment
    regex_t re_hexdigit; // Matches comment
    regmatch_t matches[10];
    if (regcomp(&re_hexline, "^([0-9a-fA-F]*:\\s*)(.*)", REG_EXTENDED)) {
        fprintf(stderr, "Error compiling regex for hex lines\n");
        return;
    }
    if (regcomp(&re_hexdigit, "\\s*([0-9a-fA-F]+)\\s*", REG_EXTENDED)) {
        fprintf(stderr, "Error compiling regex for hex digits\n");
        return;
    }

    int nread = 0;
    char *line = NULL;
    ssize_t linelen;
    int x = 4 + 1;
    int linenum = 0;
    while ((nread = getline(&line, &linelen, input)) != -1) {
        linenum += 1;
        int linelen = strlen(line) - 1;
        line[linelen] = '\0';

        // Read addr
        char *linebuf = line;
        if (regexec(&re_hexline, linebuf , ARR_SIZE(matches), matches, 0) == 0) {
            int k = 2;
            regoff_t eo = matches[k].rm_eo;
            regoff_t so = matches[k].rm_so;
            linebuf += so;
        }
        char *token = strtok(linebuf, " \t");
        while (token) {
            int byte = strtol(token, NULL, 16);
            token = strtok(NULL, " \t");
            Elf_Buffer_PushByte(buffer, byte);
        }
    }

    regfree(&re_hexline);
    if (line) {
        free(line);
    }
}

void Elf_ReadHexInit(Elf_Builder *elf, FILE *input)
{
    // Read header
    Elf_Buffer buffer;
    Elf_Buffer_Init(&buffer);
    _Elf_Buffer_ReadHexBytes(&buffer, input);
    memcpy(&elf->eb_ehdr, buffer.eb_data, sizeof(Elf_Ehdr));

    // TODO:  Program

    // Init setions
    elf->eb_section_head = 0;
    elf->eb_shdr_size = ELF_MAX_SECTIONS;
    elf->eb_shdr = calloc(elf->eb_shdr_size, sizeof(Elf_Shdr));
    assert(elf->eb_shdr != NULL);
    elf->eb_buffer = calloc(elf->eb_shdr_size, sizeof(Elf_Buffer));
    assert(elf->eb_buffer != NULL);
    for (int i = 0; i < elf->eb_shdr_size; i++) {
        Elf_Buffer_Init(&elf->eb_buffer[i]);
    }
    elf->eb_current_section = SHN_UNDEF;

    // Read section headers
    for (int i = 0; i <= elf->eb_ehdr.e_shnum; i++) {
        memcpy(
            &elf->eb_shdr[i], 
            buffer.eb_data + elf->eb_ehdr.e_shoff + i * sizeof(Elf_Shdr), 
            sizeof(Elf_Shdr)
        );
    }

    // Read section data
    for (int i = 1; i <= elf->eb_ehdr.e_shnum; i++) {
        Elf_Buffer_PushBytes(
            &elf->eb_buffer[i],
            buffer.eb_data + elf->eb_shdr[i].sh_offset,
            elf->eb_shdr[i].sh_size
        );
    }

    // Free memory
    Elf_Buffer_Destroy(&buffer);
}

void Elf_WriteDump(Elf_Builder* elf, FILE* output)
{
    int i;
    for (i = 1; i <= Elf_GetSectionCount(elf); i++) {
        Elf_PushSection(elf);
        Elf_SetSection(elf, i);
        Elf_Shdr *shdr = Elf_GetSection(elf);
        unsigned char *data = Elf_GetSectionData(elf);
        fprintf(output, "#%s\n", Elf_GetSectionName(elf));
        switch (shdr->sh_type) {
            case SHT_SYMTAB: {
                fprintf(output, "Num  Value   Size Type  Bind Ndx Name\n");
                int i, size = shdr->sh_size / shdr->sh_entsize;
                for (i = 0; i < size; i++) {
                    Elf_Sym *sym = Elf_GetSymbol(elf, i);
                    int type = ELF_ST_TYPE(sym->st_info);
                    int bind = ELF_ST_BIND(sym->st_info);
                    fprintf(output, "%3d: %08d %3d %5s %4s ", i, 
                        sym->st_value,
                        sym->st_size,
                        type == STT_OBJECT ? "OBJ"
                            : type == STT_FUNC ? "FUNC"
                            : type == STT_SECTION ? "SCTN"
                            : type == STT_COMMON ?  "COMN"
                            : "NOTYP",
                        bind == STB_GLOBAL ? "GLOB" 
                            : bind == STB_WEAK ? "WEAK"
                            : "LOC"
                    );
                    if (sym->st_shndx == SHN_UNDEF) {
                        fprintf(output, "UND");
                    } else {
                        fprintf(output, "%3d", sym->st_shndx);
                    }
                    fprintf(output, " %s", Elf_GetSymbolName(elf, i));
                    fputc('\n', output);
                }
            } break;
            case SHT_RELA: {
                // TODO
                fprintf(output, "Offset Type          Symbol  Addend\n");
                int i, size = Elf_GetSectionSize(elf) / sizeof(Elf_Rela);
                for (i = 0; i < size; i++) {
                    Elf_Rela *rela =  Elf_GetSectionEntry(elf, i);
                    Elf_Half type = ELF_R_TYPE(rela->r_info);
                    Elf_Word symndx = ELF_R_SYM(rela->r_info);
                    fprintf(output, "%3d: %15s %3d(%s) %4d\n", i,
                        _Elf_GetRelaTypeName(type),
                        symndx,
                        Elf_GetSymbolName(elf, symndx),
                        rela->r_addend
                    );

                }
            } break;
            case SHT_STRTAB: {
                int iter = 0;
                unsigned char* start = data;
                unsigned char* str = start;
                while (str - start < Elf_GetSectionSize(elf)) {
                    fprintf(output, "%3d: \"%s\"\n", iter, str);
                    iter += 1;
                    str += strlen(str) + 1;
                }
            } break;
            default: {
                int i, size = Elf_GetSectionSize(elf);
                for (i = 0; i < size; i++) {
                    fprintf(output, "%02x ", data[i]);
                    if ((i + 1) % 8 == 0) {
                        fputc('\n', output);
                    } else if ((i + 1) % 4 == 0) {
                        fputc(' ', output);
                    }
                }
                fputc('\n', output);
            } break;
        }
        Elf_PopSection(elf);
    }
}

Elf_Word Elf_FindString(Elf_Builder *elf, const char *str)
{
    Elf_PushSection(elf);
    Elf_UseSection(elf, ".strtab");
    Elf_Word start = 0;
    int found = 0;
    while ((! found) && start < Elf_GetSectionSize(elf)) {
        char *check_str = Elf_GetSectionData(elf) + start;
        Elf_Word len = strlen(check_str);
        if (strcmp(str, check_str) == 0) {
            found = 1;
            break;
        }
        start += len + 1;
    }
    Elf_PopSection(elf);
    return found ? start : 0;
}

int Elf_StringExists(Elf_Builder *elf, const char *str)
{
    return Elf_FindString(elf, str) != 0;
}

Elf_Word Elf_AddString(Elf_Builder* elf, const char *str)
{
    Elf_PushSection(elf);
    Elf_UseSection(elf, ".strtab");
    Elf_Word at = Elf_GetSectionSize(elf);
    Elf_PushString(elf, str);
    Elf_PopSection(elf);
    return at;
}

Elf_Word Elf_AddUniqueString(Elf_Builder *elf, const char *str)
{
    Elf_Word at = Elf_FindString(elf, str);
    if (at == 0) {
        return Elf_AddString(elf, str);
    }
    return at;
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


void Elf_Link(Elf_Builder *dest, Elf_Builder *src)
{
    for (int i = 1; i <= Elf_GetSectionCount(src); i++) {
        Elf_PushSection(src);
        Elf_SetSection(src, i);
        Elf_Shdr *shdr = Elf_GetSection(src);
        switch (shdr->sh_type) {
            case SHT_STRTAB: {
                Elf_Word off = 1;
                while (off < Elf_GetSectionSize(src))  {
                    char *str = Elf_GetSectionData(src) + 1;
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
                        // Get symbol section name in the source
                        Elf_PushSection(src);
                        Elf_SetSection(src, sym->st_shndx);
                        const char *section_name = Elf_GetSectionName(src);
                        Elf_PopSection(src);

                        // Import symbol to destination + copy relevant info
                        Elf_PushSection(dest);
                        Elf_UseSection(dest, section_name);
                        Elf_Sym *dest_sym = Elf_UseSymbol(dest, sym_name);
                        /* dest_sym->st_name - Set by Elf_UseSymbol() */
                        dest_sym->st_info = sym->st_info;
                        dest_sym->st_other = sym->st_other;
                        /* dest_sym->st_shndx - Set by  by Elf_UseSymbol() */
                        dest_sym->st_value= sym->st_value;
                        dest_sym->st_size = sym->st_size;
                        Elf_PopSection(dest);
                    }
                }
            } break;
            case SHT_RELA: {
                // Initialize rela section
                Elf_PushSection(dest);
                Elf_UseSection(dest, Elf_GetSectionName(src));
                Elf_GetSection(dest)->sh_type = SHT_RELA;
                Elf_GetSection(dest)->sh_entsize= sizeof(Elf_Rela);
                Elf_PopSection(dest);

                // Add Rela entries
                int i, size = Elf_GetSectionSize(src) / sizeof(Elf_Rela);
                for (i = 0; i < size; i++) {
                    Elf_Rela *rela = Elf_GetSectionEntry(src, i);
                    Elf_Half type = ELF_R_TYPE(rela->r_info);
                    Elf_Word symndx = ELF_R_SYM(rela->r_info);

                    // Get symbol and then section name
                    Elf_PushSection(src);
                    Elf_Sym *sym = Elf_GetSymbol(src, symndx);
                    Elf_SetSection(src, sym->st_shndx);
                    const char *section_name = Elf_GetSectionName(src);
                    const char *sym_name = Elf_GetSymbolName(src, symndx);
                    Elf_PopSection(src);

                    // Import symbol if necessar
                    // What we actually want is the new symndx in the `dest`
                    Elf_PushSection(dest);
                    Elf_UseSection(dest, section_name);
                    Elf_Sym *dest_sym = Elf_UseSymbol(dest, sym_name);
                    /* dest_sym->st_name - Set by Elf_UseSymbol() */
                    dest_sym->st_info = sym->st_info;
                    dest_sym->st_other = sym->st_other;
                    /* dest_sym->st_shndx - Set by  by Elf_UseSymbol() */
                    dest_sym->st_value= sym->st_value;
                    dest_sym->st_size = sym->st_size;

                    Elf_Word dest_symndx = Elf_FindSymbol(dest, sym_name);
                    Elf_PopSection(dest);

                    // Add rela to destination
                    Elf_Rela dest_rela;
                    dest_rela.r_offset = rela->r_offset;
                    dest_rela.r_info = ELF_R_INFO(dest_symndx, type);
                    dest_rela.r_addend = rela->r_addend;
                    Elf_PushSection(dest);
                    Elf_UseSection(dest, Elf_GetSectionName(src));
                    Elf_PushBytes(dest, &dest_rela, sizeof(dest_rela));
                    Elf_PopSection(dest);


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
    
}
