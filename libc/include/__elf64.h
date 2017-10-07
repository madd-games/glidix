/*
	Glidix Runtime

	Copyright (c) 2014-2017, Madd Games.
	All rights reserved.
	
	Redistribution and use in source and binary forms, with or without
	modification, are permitted provided that the following conditions are met:
	
	* Redistributions of source code must retain the above copyright notice, this
	  list of conditions and the following disclaimer.
	
	* Redistributions in binary form must reproduce the above copyright notice,
	  this list of conditions and the following disclaimer in the documentation
	  and/or other materials provided with the distribution.
	
	THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS"
	AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
	IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE
	DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT HOLDER OR CONTRIBUTORS BE LIABLE
	FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
	DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR
	SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER
	CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY,
	OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
	OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
*/

#ifndef ELF64_H
#define ELF64_H

#include <stdint.h>
#include <stddef.h>

typedef	uint64_t			Elf64_Addr;
typedef	uint16_t			Elf64_Half;
typedef	uint64_t			Elf64_Off;
typedef	int32_t				Elf64_Sword;
typedef	int64_t				Elf64_Sxword;
typedef	uint32_t			Elf64_Word;
typedef	uint64_t			Elf64_Xword;
typedef	int64_t				Elf64_Sxword;

#define	EI_MAG0				0
#define	EI_MAG1				1
#define	EI_MAG2				2
#define	EI_MAG3				3
#define	EI_CLASS			4
#define	EI_DATA				5
#define	EI_VERSION			6
#define	EI_OSABI			7
#define	EI_ABIVERSION			8
#define	EI_PAD				9
#define	EI_NIDENT			16

#define	ELFCLASS32			1
#define	ELFCLASS64			2

#define	ELFDATA2LSB			1
#define	ELFDATA2MSB			2

#define	ET_NONE				0
#define	ET_REL				1		/* for modules */
#define	ET_EXEC				2
#define	ET_DYN				3

#define	EM_X86_64			62

#define	PT_NULL				0
#define	PT_LOAD				1
#define	PT_DYNAMIC			2
#define	PT_INTERP			3
#define	PT_NOTE				4

#define	PF_X				0x1
#define	PF_W				0x2
#define	PF_R				0x4

#define	SHT_NULL			0
#define	SHT_PROGBITS			1
#define	SHT_SYMTAB			2
#define	SHT_STRTAB			3
#define	SHT_RELA			4
#define	SHT_HASH			5
#define	SHT_DYNAMIC			6
#define	SHT_NOTE			7
#define	SHT_NOBITS			8
#define	SHT_REL				9
#define	SHT_SHLIB			10
#define	SHT_DYNSYM			11

#define ELF64_R_SYM(i)			((i) >> 32)
#define ELF64_R_TYPE(i)			((i) & 0xffffffffL)
#define ELF64_R_INFO(s, t)		(((s) << 32) + ((t) & 0xffffffffL))

#define	R_X86_64_NONE			0
#define	R_X86_64_64			1
#define	R_X86_64_COPY			5
#define	R_x86_64_GLOB_DAT		6
#define	R_X86_64_JUMP_SLOT		7
#define	R_X86_64_RELATIVE		8

#define DT_NULL		0	/* Terminating entry. */
#define DT_NEEDED	1	/* String table offset of a needed shared
				   library. */
#define DT_PLTRELSZ	2	/* Total size in bytes of PLT relocations. */
#define DT_PLTGOT	3	/* Processor-dependent address. */
#define DT_HASH		4	/* Address of symbol hash table. */
#define DT_STRTAB	5	/* Address of string table. */
#define DT_SYMTAB	6	/* Address of symbol table. */
#define DT_RELA		7	/* Address of ElfNN_Rela relocations. */
#define DT_RELASZ	8	/* Total size of ElfNN_Rela relocations. */
#define DT_RELAENT	9	/* Size of each ElfNN_Rela relocation entry. */
#define DT_STRSZ	10	/* Size of string table. */
#define DT_SYMENT	11	/* Size of each symbol table entry. */
#define DT_INIT		12	/* Address of initialization function. */
#define DT_FINI		13	/* Address of finalization function. */
#define DT_SONAME	14	/* String table offset of shared object
				   name. */
#define DT_RPATH	15	/* String table offset of library path. [sup] */
#define DT_SYMBOLIC	16	/* Indicates "symbolic" linking. [sup] */
#define DT_REL		17	/* Address of ElfNN_Rel relocations. */
#define DT_RELSZ	18	/* Total size of ElfNN_Rel relocations. */
#define DT_RELENT	19	/* Size of each ElfNN_Rel relocation. */
#define DT_PLTREL	20	/* Type of relocation used for PLT. */
#define DT_DEBUG	21	/* Reserved (not used). */
#define DT_TEXTREL	22	/* Indicates there may be relocations in
				   non-writable segments. [sup] */
#define DT_JMPREL	23	/* Address of PLT relocations. */
#define	DT_BIND_NOW	24	/* [sup] */
#define	DT_INIT_ARRAY	25	/* Address of the array of pointers to
				   initialization functions */
#define	DT_FINI_ARRAY	26	/* Address of the array of pointers to
				   termination functions */
#define	DT_INIT_ARRAYSZ	27	/* Size in bytes of the array of
				   initialization functions. */
#define	DT_FINI_ARRAYSZ	28	/* Size in bytes of the array of
				   terminationfunctions. */
#define	DT_RUNPATH	29	/* String table offset of a null-terminated
				   library search path string. */
#define	DT_FLAGS	30	/* Object specific flag values. */
#define	DT_ENCODING	32	/* Values greater than or equal to DT_ENCODING
				   and less than DT_LOOS follow the rules for
				   the interpretation of the d_un union
				   as follows: even == 'd_ptr', even == 'd_val'
				   or none */
#define	DT_PREINIT_ARRAY 32	/* Address of the array of pointers to
				   pre-initialization functions. */
#define	DT_PREINIT_ARRAYSZ 33	/* Size in bytes of the array of
				   pre-initialization functions. */

typedef	uint64_t			Elf64_Addr;
typedef	uint16_t			Elf64_Half;
typedef	uint64_t			Elf64_Off;
typedef	int32_t				Elf64_Sword;
typedef	int64_t				Elf64_Sxword;
typedef	uint32_t			Elf64_Word;
typedef	uint64_t			Elf64_Xword;

#define	SHN_UNDEF		0
#define	SHN_ABS			0xFFF1
#define	SHN_COMMON		0xFFF2

/**
 * Glidix annotations (in .glidix.annot section).
 */
#define	GAT_APPTYPE		0x01	/* application type */

/**
 * Applications types (values for GAT_APPTYPE).
 */
#define	APT_CONSOLE		0x00	/* console (default) */
#define	APT_GUI			0x01	/* GUI */

#define	EI_MAG0				0
#define	EI_MAG1				1
#define	EI_MAG2				2
#define	EI_MAG3				3
#define	EI_CLASS			4
#define	EI_DATA				5
#define	EI_VERSION			6
#define	EI_OSABI			7
#define	EI_ABIVERSION			8
#define	EI_PAD				9
#define	EI_NIDENT			16

#define	ELFCLASS32			1
#define	ELFCLASS64			2

#define	ELFDATA2LSB			1
#define	ELFDATA2MSB			2

#define	ET_NONE				0
#define	ET_REL				1		/* for modules */
#define	ET_EXEC				2
#define	ET_DYN				3

#define	EM_X86_64			62

#define	PT_NULL				0
#define	PT_LOAD				1
#define	PT_DYNAMIC			2
#define	PT_INTERP			3
#define	PT_NOTE				4
#define	PT_SHLIB			5
#define	PT_PHDR				6

#define	PF_X				0x1
#define	PF_W				0x2
#define	PF_R				0x4

#define	SHT_NULL			0
#define	SHT_PROGBITS			1
#define	SHT_SYMTAB			2
#define	SHT_STRTAB			3
#define	SHT_RELA			4
#define	SHT_HASH			5
#define	SHT_DYNAMIC			6
#define	SHT_NOTE			7
#define	SHT_NOBITS			8
#define	SHT_REL				9
#define	SHT_SHLIB			10
#define	SHT_DYNSYM			11

#define DT_NULL		0	/* Terminating entry. */
#define DT_NEEDED	1	/* String table offset of a needed shared
				   library. */
#define DT_PLTRELSZ	2	/* Total size in bytes of PLT relocations. */
#define DT_PLTGOT	3	/* Processor-dependent address. */
#define DT_HASH		4	/* Address of symbol hash table. */
#define DT_STRTAB	5	/* Address of string table. */
#define DT_SYMTAB	6	/* Address of symbol table. */
#define DT_RELA		7	/* Address of ElfNN_Rela relocations. */
#define DT_RELASZ	8	/* Total size of ElfNN_Rela relocations. */
#define DT_RELAENT	9	/* Size of each ElfNN_Rela relocation entry. */
#define DT_STRSZ	10	/* Size of string table. */
#define DT_SYMENT	11	/* Size of each symbol table entry. */
#define DT_INIT		12	/* Address of initialization function. */
#define DT_FINI		13	/* Address of finalization function. */
#define DT_SONAME	14	/* String table offset of shared object
				   name. */
#define DT_RPATH	15	/* String table offset of library path. [sup] */
#define DT_SYMBOLIC	16	/* Indicates "symbolic" linking. [sup] */
#define DT_REL		17	/* Address of ElfNN_Rel relocations. */
#define DT_RELSZ	18	/* Total size of ElfNN_Rel relocations. */
#define DT_RELENT	19	/* Size of each ElfNN_Rel relocation. */
#define DT_PLTREL	20	/* Type of relocation used for PLT. */
#define DT_DEBUG	21	/* Reserved (not used). */
#define DT_TEXTREL	22	/* Indicates there may be relocations in
				   non-writable segments. [sup] */
#define DT_JMPREL	23	/* Address of PLT relocations. */
#define	DT_BIND_NOW	24	/* [sup] */
#define	DT_INIT_ARRAY	25	/* Address of the array of pointers to
				   initialization functions */
#define	DT_FINI_ARRAY	26	/* Address of the array of pointers to
				   termination functions */
#define	DT_INIT_ARRAYSZ	27	/* Size in bytes of the array of
				   initialization functions. */
#define	DT_FINI_ARRAYSZ	28	/* Size in bytes of the array of
				   terminationfunctions. */
#define	DT_RUNPATH	29	/* String table offset of a null-terminated
				   library search path string. */
#define	DT_FLAGS	30	/* Object specific flag values. */
#define	DT_ENCODING	32	/* Values greater than or equal to DT_ENCODING
				   and less than DT_LOOS follow the rules for
				   the interpretation of the d_un union
				   as follows: even == 'd_ptr', even == 'd_val'
				   or none */
#define	DT_PREINIT_ARRAY 32	/* Address of the array of pointers to
				   pre-initialization functions. */
#define	DT_PREINIT_ARRAYSZ 33	/* Size in bytes of the array of
				   pre-initialization functions. */

#define ELF64_R_SYM(i)			((i) >> 32)
#define ELF64_R_TYPE(i)			((i) & 0xffffffffL)
#define ELF64_R_INFO(s, t)		(((s) << 32) + ((t) & 0xffffffffL))

#define	R_X86_64_NONE			0
#define	R_X86_64_64			1
#define	R_X86_64_COPY			5
#define	R_X86_64_GLOB_DAT		6
#define	R_X86_64_JUMP_SLOT		7
#define	R_X86_64_RELATIVE		8

#define	AT_NULL				0
#define	AT_IGNORE			1
#define	AT_EXECFD			2

#define	ELF64_S_BINDING(i)		((i >> 4) & 0xF)
#define	ELF64_S_TYPE(i)			(i & 0xF)

#define	STB_LOCAL			0
#define	STB_GLOBAL			1
#define	STB_WEAK			2

typedef struct
{
	unsigned char			e_ident[EI_NIDENT];
	Elf64_Half			e_type;
	Elf64_Half			e_machine;
	Elf64_Word			e_version;
	Elf64_Addr			e_entry;
	Elf64_Off			e_phoff;
	Elf64_Off			e_shoff;
	Elf64_Word			e_flags;
	Elf64_Half			e_ehsize;
	Elf64_Half			e_phentsize;
	Elf64_Half			e_phnum;
	Elf64_Half			e_shentsize;
	Elf64_Half			e_shnum;
	Elf64_Half			e_shstrndx;
} Elf64_Ehdr;

typedef struct
{
	Elf64_Word			p_type;
	Elf64_Word			p_flags;
	Elf64_Off			p_offset;
	Elf64_Addr			p_vaddr;
	Elf64_Addr			p_paddr;		// ignore this
	Elf64_Xword			p_filesz;
	Elf64_Xword			p_memsz;
	Elf64_Xword			p_align;
} Elf64_Phdr;

typedef struct
{
	Elf64_Word			sh_name;
	Elf64_Word			sh_type;
	Elf64_Xword			sh_flags;
	Elf64_Addr			sh_addr;
	Elf64_Off			sh_offset;
	Elf64_Xword			sh_size;
	Elf64_Word			sh_link;
	Elf64_Word			sh_info;
	Elf64_Xword			sh_addralign;
	Elf64_Xword			sh_entsize;
} Elf64_Shdr;

typedef struct
{
	Elf64_Addr			r_offset;
	Elf64_Xword			r_info;
	Elf64_Sxword			r_addend;
} Elf64_Rela;

typedef struct
{
	Elf64_Word			st_name;
	unsigned char			st_info;
	unsigned char			st_other;
	Elf64_Half			st_shndx;
	Elf64_Addr			st_value;
	Elf64_Xword			st_size;
} Elf64_Sym;

typedef struct
{
	Elf64_Sxword			d_tag;
	union
	{
		Elf64_Xword		d_val;
		Elf64_Addr		d_ptr;
	} d_un;
} Elf64_Dyn;

typedef struct
{
	uint32_t			a_type;
	union
	{
		uint64_t		a_val;
		void*			a_ptr;
	} a_un;
} Elf64_Auxv;

/**
 * Glidix annotation (in the ".glidix.annot" section).
 */
typedef struct
{
	unsigned char			gan_type;
	unsigned char			gan_val;
} Elf64_GlidixAnnot;

#endif
