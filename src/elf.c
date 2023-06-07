#include <assert.h>
#include <stdlib.h>
#include <string.h>
#include <elf.h>
#include <regex.h>

#define ARR_SIZE(arr) (sizeof((arr)) / sizeof((arr)[0]))
#define NORMAL 0
#define FAINT  2
#define RED    31
#define GREEN  32
#define YELLOW 33
#define BLUE   34

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
    free(elf->eb_shdr);
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
