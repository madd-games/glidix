#include <sys/glidix.h>
#include <stdio.h>
#include "elf64.h"
#include <string.h>

#define	PREFERRED_LOAD_ADDR 0x100000000

typedef struct
{
	Elf64_Rela*				rela;
	size_t					szRela;
	size_t					szRelaEntry;
	Elf64_Rela*				pltRela;
	size_t					pltRelaSize;
	Elf64_Sym*				symtab;
	char*					strtab;
	uint64_t				loadAddr;
	Elf64_Word*				hashtab;
	unsigned int				symbolCount;
} Library;

int isSymbolNeededFor(Elf64_Xword relocType)
{
	switch (relocType)
	{
	case R_X86_64_RELATIVE:
		return 0;
	default:
		return 1;
	};
};

int relocate(Library *lib, Elf64_Rela *table, size_t num)
{
	size_t i;
	for (i=0; i<num; i++)
	{
		Elf64_Rela *rela = &table[i];
		Elf64_Xword type = ELF64_R_TYPE(rela->r_info);
		Elf64_Xword symidx = ELF64_R_SYM(rela->r_info);

		Elf64_Sym *symbol = &lib->symtab[symidx];
		const char *symname = &lib->strtab[symbol->st_name];
		//printf("symname@%p (symbol %d at %p), value=%p\n", symname, symidx, symbol, (void*)symbol->st_value);
		if (symbol->st_name == 0) symname = "<noname>";

		void *symaddr = NULL;
		if ((symbol->st_shndx == 0) && isSymbolNeededFor(type))
		{
			printf("undefined reference to '%s'\n", symname);
			return -1;
		}
		else
		{
			symaddr = (void*) (lib->loadAddr + symbol->st_value);
		};

		void *reladdr = (void*) (lib->loadAddr + rela->r_offset);
		switch (type)
		{
		case R_X86_64_64:
			//printf("64 (%p) = '%s' + %d (%p)\n", reladdr, symname, rela->r_addend, (void*)((uint64_t)symaddr + rela->r_addend));
			*((uint64_t*)reladdr) = (uint64_t) symaddr + rela->r_addend;
			break;
		case R_X86_64_JUMP_SLOT:
			//printf("JUMP_SLOT (%p) = '%s' (%p)\n", reladdr, symname, symaddr);
			*((uint64_t*)reladdr) = (uint64_t) symaddr;
			break;
		case R_X86_64_RELATIVE:
			//printf("RELATIVE (%p) = %p\n", reladdr, (void*)(lib->loadAddr + rela->r_addend));
			*((uint64_t*)reladdr) = (uint64_t) (lib->loadAddr + rela->r_addend);
			break;
		case R_x86_64_GLOB_DAT:
			//printf("GLOB_DAT (%p) = '%s' (%p)\n", reladdr, symname, symaddr);
			*((uint64_t*)reladdr) = (uint64_t) symaddr;
			break;
		default:
			printf("bad relocation\n");
			return 1;
		};
	};

	return 0;
};

Library lib;
int libOpen(const char *path)
{
	//printf("Attempting to load /mnt/hello.so, preferably at %p\n", PREFERRED_LOAD_ADDR);
	_glidix_libinfo info;
	int status = _glidix_libopen(path, PREFERRED_LOAD_ADDR, &info);
	//printf("Status: %d\n", status);

	if (status == -1) return -1;

	printf("Dynamic info size:          %u\n", (unsigned int) info.dynSize);
	printf("Address of dynamic section: %p\n", info.dynSection);
	printf("Load address:               %p\n", (void*)info.loadAddr);
	printf("Next good load addr:        %p\n", (void*)info.nextLoadAddr);
	printf("Text base:                  %p\n", (void*)(info.textIndex * 0x1000));
	printf("Data base:                  %p\n", (void*)(info.dataIndex * 0x1000));
	printf("\nAnalyzing dynamic info...\n");

	Elf64_Dyn *dyn = (Elf64_Dyn*) info.dynSection;
	memset(&lib, 0, sizeof(Library));
	lib.loadAddr = info.loadAddr;
	while (dyn->d_tag != DT_NULL)
	{
		if (dyn->d_tag == DT_RELA)
		{
			lib.rela = (Elf64_Rela*) (info.loadAddr + dyn->d_un.d_ptr);
		}
		else if (dyn->d_tag == DT_RELASZ)
		{
			lib.szRela = dyn->d_un.d_val;
		}
		else if (dyn->d_tag == DT_RELAENT)
		{
			lib.szRelaEntry = dyn->d_un.d_val;
		}
		else if (dyn->d_tag == DT_JMPREL)
		{
			lib.pltRela = (Elf64_Rela*) (info.loadAddr + dyn->d_un.d_ptr);
		}
		else if (dyn->d_tag == DT_PLTRELSZ)
		{
			lib.pltRelaSize = dyn->d_un.d_val;
		}
		else if (dyn->d_tag == DT_STRTAB)
		{
			lib.strtab = (char*) (info.loadAddr + dyn->d_un.d_ptr);
		}
		else if (dyn->d_tag == DT_SYMTAB)
		{
			lib.symtab = (Elf64_Sym*) (info.loadAddr + dyn->d_un.d_ptr);
		}
		else if (dyn->d_tag == DT_HASH)
		{
			lib.hashtab = (Elf64_Word*) (info.loadAddr + dyn->d_un.d_ptr);
		}
		else if (dyn->d_tag == DT_SYMENT)
		{
			if (dyn->d_un.d_val != sizeof(Elf64_Sym))
			{
				printf("Symbol size mismatch\n");
				return -1;
			};
		};

		dyn++;
	};

	printf("Relocation table: %p\n", lib.rela);
	printf("Relocation size:  %d\n", (unsigned int) lib.szRela);
	printf("Reloc entry size: %d\n", (unsigned int) lib.szRelaEntry);
	printf("PLT Reloc table:  %p\n", lib.pltRela);
	printf("PLT Reloc size:   %d\n", (unsigned int) lib.pltRelaSize);
	printf("String table:     %p\n", lib.strtab);
	printf("Symbol table:     %p\n", lib.symtab);
	printf("Hash table:       %p\n", lib.hashtab);
	printf("Sizeof Elf64_Sym: %d\n", sizeof(Elf64_Sym));

	if (lib.hashtab != NULL)
	{
		lib.symbolCount = (unsigned int) lib.hashtab[1];
	};
	printf("Dyn. symbols:     %d\n", (unsigned int) lib.symbolCount);

	if (lib.pltRela != NULL)
	{
		if (relocate(&lib, lib.pltRela, lib.pltRelaSize / sizeof(Elf64_Rela)))
		{
			//_glidix_libclose(&info);
			return -1;
		};
	};

	printf("Relocating the RELA section\n");
	if (lib.rela != NULL)
	{
		if (relocate(&lib, lib.rela, lib.szRela / lib.szRelaEntry))
		{
			//_glidix_libclose(&info);
			return -1;
		};
	};

	return 0;
};

uint64_t getSymbolAddr(const char *name)
{
	// we pass through global symbols then go to weak symbols
	unsigned char pass;
	for (pass=1; pass<3; pass++)
	{
		size_t i;
		for (i=0; i<lib.symbolCount; i++)
		{
			Elf64_Sym *sym = &lib.symtab[i];
			const char *thisName = &lib.strtab[sym->st_name];

			if (strcmp(thisName, name) == 0)
			{
				unsigned char binding = (sym->st_info >> 4) & 0xF;
				if (binding == pass)
				{
					return lib.loadAddr + sym->st_value;
				};
			};
		};
	};

	return 0;
};

typedef int (*factorial_t)(int);

int main()
{
	if (libOpen("/mnt/libc.so") != 0)
	{
		printf("Library loading failure\n");
		return 1;
	};
	uint64_t func = getSymbolAddr("open");
	printf("Address of 'open' = %p\n", func);
	return 0;
};
