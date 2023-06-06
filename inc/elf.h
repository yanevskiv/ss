#ifndef _SS_ELF_H_
#define _SS_ELF_H_
#include <stdio.h>
#include <stdint.h>

#define ELF_MAX_SECTIONS 128
#define ELF_STACK_SIZE 32
#define ELF_MAX_SECTION_NAME 256

// Ident
#define EI_NIDENT (16)

#define EI_MAG0 0 /* File identification byte 0 index */
#define EI_MAG1 1 /* File identification byte 1 index */
#define EI_MAG2 2 /* File identification byte 2 index */
#define EI_MAG3 3 /* File identification byte 3 index */
#define EI_CLASS 4 /* File class byte index */
#define EI_DATA 5 /* Data encoding byte index */
#define EI_VERSION 6 /* File version byte index */
#define EI_OSABI 7 /* OS ABI identification */
#define EI_ABIVERSION 8 /* ABI version */
#define EI_PAD 9 /* Byte index of padding bytes */


#define ELFCLASSNONE 0 /* Invalid class */
#define ELFCLASS32 1 /* 32-bit objects */
#define ELFCLASS64 2 /* 64-bit objects */

#define ELFDATANONE 0 /* Invalid data encoding */
#define ELFDATA2LSB 1 /* 2's complement, little endian */
#define ELFDATA2MSB 2 /* 2's complement, big endian */

#define EV_NONE 0 /* Invalid ELF version */
#define EV_CURRENT 1 /* Current version */

#define ELFOSABI_NONE 0 /* UNIX System V ABI */
#define ELFOSABI_SYSV 0 /* Alias. */
#define ELFOSABI_HPUX 1 /* HP-UX */
#define ELFOSABI_NETBSD 2 /* NetBSD. */
#define ELFOSABI_GNU 3 /* Object uses GNU ELF extensions. */

// Type
#define ET_NONE 0 /* No file type */
#define ET_REL 1 /* Relocatable file */
#define ET_EXEC 2 /* Executable file */
#define ET_DYN 3 /* Shared object file */
#define ET_CORE 4 /* Core file */
#define ET_LOOS 0xfe00 /* OS-specific range start */
#define ET_HIOS 0xfeff /* OS-specific range end */
#define ET_LOPROC 0xff00 /* Processor-specific range start */
#define ET_HIPROC 0xffff /* Processor-specific range end */

// Machine
#define EM_NONE 0 /* No machine */
#define EM_386 3 /* Intel 80386 */
#define EM_860 7 /* Intel 80860 */
#define EM_ARM    40 /* ARM */
#define EM_MIPS_X 51 /* Stanford MIPS-X */
#define EM_X86_64 62 /* AMD x86-64 architecture */
#define EM_RISCV 243 /* RISC-V */

// Section types
#define SHT_NULL      0     /* Section header table entry unused */
#define SHT_PROGBITS  1     /* Program data */
#define SHT_SYMTAB    2     /* Symbol table */
#define SHT_STRTAB    3     /* String table */
#define SHT_RELA      4     /* Relocation entries with addends */
#define SHT_HASH      5     /* Symbol hash table */
#define SHT_DYNAMIC   6     /* Dynamic linking information */
#define SHT_NOTE      7     /* Notes */
#define SHT_NOBITS    8     /* Program space with no data (bss) */
#define SHT_REL       9     /* Relocation entries, no addends */
#define SHT_SHLIB     10    /* Reserved */
#define SHT_DYNSYM    11    /* Dynamic linker symbol table */

// Section flags
#define SHF_WRITE (1 << 0) /* Writable */
#define SHF_ALLOC (1 << 1) /* Occupies memory during execution */
#define SHF_EXECINSTR (1 << 2) /* Executable */
#define SHF_MERGE (1 << 4) /* Might be merged */
#define SHF_STRINGS (1 << 5) /* Contains nul-terminated strings */
#define SHF_INFO_LINK (1 << 6) /* 'sh_info' contains SHT index */
#define SHF_GROUP (1 << 9) /* Section is member of a group. */
#define SHF_TLS (1 << 10) /* Section hold thread-local data. */
#define SHF_MASKOS 0x0ff00000 /* OS-specific. */
#define SHF_MASKPROC 0xf0000000 /* Processor-specific */

// Section Header table
#define SHN_UNDEF 0 /* Undefined section */
#define SHN_LORESERVE 0xff00 /* Start of reserved indices */
#define SHN_LOPROC 0xff00 /* Start of processor-specific */
#define SHN_HIPROC 0xff1f /* End of processor-specific */
#define SHN_LOOS 0xff20 /* Start of OS-specific */
#define SHN_HIOS 0xff3f /* End of OS-specific */
#define SHN_ABS 0xfff1 /* Associated symbol is absolute */
#define SHN_COMMON 0xfff2 /* Associated symbol is common */
#define SHN_XINDEX 0xffff /* Index is in extra table. */
#define SHN_HIRESERVE 0xffff /* End of reserved indices */

// Symbol table binding
#define ELF_ST_INFO(bind, type) (((bind) << 4) + ((type) & 0xf))
#define ELF_ST_BIND(val) (((unsigned char) (val)) >> 4)
#define ELF_ST_TYPE(val) ((val) & 0xf)
#define STB_LOCAL 0 /* Local symbol */
#define STB_GLOBAL 1 /* Global symbol */
#define STB_WEAK 2 /* Weak symbol */

// Symbol table type
#define STT_NOTYPE 0 /* Symbol type is unspecified */
#define STT_OBJECT 1 /* Symbol is a data object */
#define STT_FUNC 2 /* Symbol is a code object */
#define STT_SECTION 3 /* Symbol associated with a section */
#define STT_COMMON 5 /* Symbol is a common data object */

// Symbol visibility (other)
#define STV_DEFAULT 0 /* Default symbol visibility rules */
#define STV_INTERNAL 1 /* Processor specific hidden class */
#define STV_HIDDEN 2 /* Sym unavailable in other modules */
#define STV_PROTECTED 3 /* Not preemptible, not exported */

// Relocation types
#define ELF_R_SYM(i) ((i) >> 32)
#define ELF_R_TYPE(i) ((i) & 0xffffffff)
#define ELF_R_INFO(sym,type) ((((Elf_Xword) (sym)) << 32) + (type))
#define R_X86_64_NONE       0   /* No reloc */
#define R_X86_64_64         1   /* Direct 64 bit  */
#define R_X86_64_PC32       2   /* PC relative 32 bit signed */
#define R_X86_64_GOT32      3   /* 32 bit GOT entry */
#define R_X86_64_PLT32      4   /* 32 bit PLT address */
#define R_X86_64_COPY       5   /* Copy symbol at runtime */
#define R_X86_64_GLOB_DAT   6   /* Create GOT entry */
#define R_X86_64_JUMP_SLOT  7   /* Create PLT entry */
#define R_X86_64_RELATIVE   8   /* Adjust by program base */
#define R_X86_64_GOTPCREL   9   /* 32 bit signed PC relative offset to GOT */
#define R_X86_64_32         10  /* Direct 32 bit zero extended */
#define R_X86_64_32S        11  /* Direct 32 bit sign extended */
#define R_X86_64_16         12  /* Direct 16 bit zero extended */
#define R_X86_64_PC16       13  /* 16 bit sign extended pc relative */
#define R_X86_64_8          14  /* Direct 8 bit sign extended  */


/* Type for 8-bit quantity */
typedef uint8_t Elf_Byte;

/* Type for a 16-bit quantity.  */
typedef uint16_t Elf_Half;

/* Types for signed and unsigned 32-bit quantities.  */
typedef uint32_t Elf_Word;
typedef int32_t  Elf_Sword;

/* Types for signed and unsigned 64-bit quantities.  */
typedef uint64_t Elf_Xword;
typedef int64_t  Elf_Sxword;

/* Type of addresses.  */
typedef uint64_t Elf_Addr;

/* Type of file offsets.  */
typedef uint64_t Elf_Off;

/* Type for section indices, which are 16-bit quantities.  */
typedef uint16_t Elf_Section;

/* Type for version symbol information.  */
typedef Elf_Half Elf_Versym;


typedef struct
{
  unsigned char e_ident[EI_NIDENT]; /* Magic number and other info */
  Elf_Half  e_type;         /* Object file type */
  Elf_Half  e_machine;      /* Architecture */
  Elf_Word  e_version;      /* Object file version */
  Elf_Addr  e_entry;        /* Entry point virtual address */
  Elf_Off   e_phoff;        /* Program header table file offset */
  Elf_Off   e_shoff;        /* Section header table file offset */
  Elf_Word  e_flags;        /* Processor-specific flags */
  Elf_Half  e_ehsize;       /* ELF header size in bytes */
  Elf_Half  e_phentsize;    /* Program header table entry size */
  Elf_Half  e_phnum;        /* Program header table entry count */
  Elf_Half  e_shentsize;    /* Section header table entry size */
  Elf_Half  e_shnum;        /* Section header table entry count */
  Elf_Half  e_shstrndx;     /* Section header string table index */
} Elf_Ehdr;


typedef struct
{
  Elf_Word  sh_name;        /* Section name (string tbl index) */
  Elf_Word  sh_type;        /* Section type */
  Elf_Xword sh_flags;       /* Section flags */
  Elf_Addr  sh_addr;        /* Section virtual addr at execution */
  Elf_Off   sh_offset;      /* Section file offset */
  Elf_Xword sh_size;        /* Section size in bytes */
  Elf_Word  sh_link;        /* Link to another section */
  Elf_Word  sh_info;        /* Additional section information */
  Elf_Xword sh_addralign;   /* Section alignment */
  Elf_Xword sh_entsize;     /* Entry size if section holds table */

  /* ... Additionally ... */
} Elf_Shdr;

typedef struct
{
  Elf_Word  st_name;        /* Symbol name (string tbl index) */
  unsigned char st_info;    /* Symbol type and binding */
  unsigned char st_other;   /* Symbol visibility */
  Elf_Section   st_shndx;   /* Section index */
  Elf_Addr  st_value;       /* Symbol value */
  Elf_Xword st_size;        /* Symbol size */
} Elf_Sym;

typedef struct
{
  Elf_Half si_boundto;      /* Direct bindings, symbol bound to */
  Elf_Half si_flags;            /* Per symbol flags */
} Elf_Syminfo;

typedef struct
{
  Elf_Addr  r_offset;       /* Address */
  Elf_Xword r_info;         /* Relocation type and symbol index */
} Elf_Rel;


typedef struct
{
  Elf_Addr   r_offset;       /* Address */
  Elf_Xword  r_info;         /* Relocation type and symbol index */
  Elf_Sxword r_addend;       /* Addend */
} Elf_Rela;

typedef struct
{
  Elf_Word  p_type;         /* Segment type */
  Elf_Word  p_flags;        /* Segment flags */
  Elf_Off   p_offset;       /* Segment file offset */
  Elf_Addr  p_vaddr;        /* Segment virtual address */
  Elf_Addr  p_paddr;        /* Segment physical address */
  Elf_Xword p_filesz;       /* Segment size in file */
  Elf_Xword p_memsz;        /* Segment size in memory */
  Elf_Xword p_align;        /* Segment alignment */
} Elf_Phdr;

typedef struct
{
  Elf_Sxword    d_tag;          /* Dynamic entry type */
  union {
      Elf_Xword d_val;      /* Integer value */
      Elf_Addr d_ptr;           /* Address value */
    } d_un;
} Elf_Dyn;

//typedef struct
//{
//  Elf_Half    vd_version;     /* Version revision */
//  Elf_Half    vd_flags;       /* Version information */
//  Elf_Half    vd_ndx;         /* Version Index */
//  Elf_Half    vd_cnt;         /* Number of associated aux entries */
//  Elf_Word    vd_hash;        /* Version name hash value */
//  Elf_Word    vd_aux;         /* Offset in bytes to verdaux array */
//  Elf_Word    vd_next;        /* Offset in bytes to next verdef
//                     entry */
//} Elf_Verdef;

typedef struct {
    unsigned char *eb_data;
    Elf_Xword eb_size;
    Elf_Xword eb_capacity;
} Elf_Buffer;

typedef struct {
    Elf_Ehdr eb_ehdr;
    Elf_Shdr *eb_shdr;
    Elf_Phdr *eb_phdr;
    Elf_Buffer *eb_buffer;
    size_t eb_shdr_size;
    size_t eb_phdr_size;
    Elf_Section eb_current_section;
    Elf_Word eb_current_segment;
    Elf_Section eb_section_stack[ELF_STACK_SIZE];
    Elf_Half eb_section_head;
} Elf_Builder;

// Buffer (DONE)
void Elf_Buffer_Init(Elf_Buffer* buffer);
void Elf_Buffer_Destroy(Elf_Buffer* buffer);
void Elf_Buffer_PushByte(Elf_Buffer* buffer, unsigned char byte);
void Elf_Buffer_PushString(Elf_Buffer* buffer, const char *str);
void Elf_Buffer_PushBytes(Elf_Buffer* buffer, void* bytes, size_t size);
void Elf_Buffer_PushHalf(Elf_Buffer* buffer, Elf_Half half);
void Elf_Buffer_PushSkip(Elf_Buffer* buffer, Elf_Word count, Elf_Byte fill);

// Create ELF (DONE)
void Elf_Init(Elf_Builder* builder);
void Elf_Destroy(Elf_Builder* builder);

// Section (DONE)
void Elf_PushSection(Elf_Builder* builder);
void Elf_PopSection(Elf_Builder* builder);
Elf_Shdr *Elf_CreateSection(Elf_Builder* builder, const char *name);
Elf_Shdr *Elf_SetSection(Elf_Builder* builder, Elf_Word  id);
Elf_Shdr *Elf_UseSection(Elf_Builder *builder, const char *name);
const char *Elf_GetSectionName(Elf_Builder* builder);
Elf_Section Elf_FindSection(Elf_Builder *builder, const char *name);
int Elf_SectionExists(Elf_Builder *builder, const char *name);
Elf_Shdr *Elf_GetSection(Elf_Builder* builder);
int Elf_GetSectionCount(Elf_Builder* builder);
Elf_Shdr *Elf_FetchSection(Elf_Builder* builder, const char *name);
Elf_Section Elf_GetCurrentSection(Elf_Builder* builder);

// Section data (DONE)
void *Elf_GetSectionEntry(Elf_Builder *builder, Elf_Word at);
Elf_Buffer *Elf_GetBuffer(Elf_Builder *builder);
Elf_Xword Elf_GetSectionSize(Elf_Builder *builder);
unsigned char *Elf_GetSectionData(Elf_Builder *builder);
void Elf_PushByte(Elf_Builder* builder, unsigned char byte);
void Elf_PushWord(Elf_Builder* builder, Elf_Word word);
void Elf_PushString(Elf_Builder* builder, const char *str);
void Elf_PushBytes(Elf_Builder* builder, void* bytes, size_t size);
void Elf_PushHalf(Elf_Builder* builder, Elf_Half half);
void Elf_PushSkip(Elf_Builder* builder, Elf_Word count, Elf_Byte fill);

// Symbols (DONE)
void *Elf_GetEntry(Elf_Builder* builder, Elf_Word at);
int Elf_GetSymbolCount(Elf_Builder* builder);
Elf_Sym* Elf_CreateSymbol(Elf_Builder* builder, const char *name);
void Elf_SetSymbol(Elf_Builder* builder, const char* name, Elf_Addr value);
Elf_Sym* Elf_GetSymbol(Elf_Builder* builder, int index);
Elf_Word Elf_FindSymbol(Elf_Builder* builder, const char *name);
const char *Elf_GetSymbolName(Elf_Builder* builder, int index);
int Elf_SymbolExists(Elf_Builder* builder, const char *name);
Elf_Sym* Elf_FetchSymbol(Elf_Builder* builder, const char *name);
Elf_Sym* Elf_UseSymbol(Elf_Builder* builder, const char *name);

// Output (DONE)
void Elf_ReadHexInit(Elf_Builder* builder, FILE* input);
void Elf_WriteDump(Elf_Builder* builder, FILE* output);
void Elf_WriteHex(Elf_Builder* builder, FILE* output);

// Rel/Rela (DONE)
//void Elf_UseRelaSection(Elf_Builder* builder);
Elf_Rela *Elf_AddRela(Elf_Builder* buidler, Elf_Word symndx);
Elf_Rela *Elf_AddRelaSymb(Elf_Builder* buidler, const char *symname);
Elf_Rela *Elf_GetRela(Elf_Builder* builder, int index);
int Elf_GetRelaCount(Elf_Builder* builder);

// Linking (TODO)
void Elf_Link(Elf_Builder* dest, Elf_Builder* src);

Elf_Word Elf_FindString(Elf_Builder* builder, const char *str);
int Elf_StringExists(Elf_Builder* builder, const char *str);
Elf_Word Elf_AddString(Elf_Builder* builder, const char *str);
Elf_Word Elf_AddUniqueString(Elf_Builder* builder, const char *str);
const char *Elf_GetString(Elf_Builder* builder, Elf_Word at);
//int Elf_GetStringCount(Elf_Builder* builder);
//const char *Elf_GetStringAt(Elf_Builder* builder);

#endif /* _ELF_H_ */ 
