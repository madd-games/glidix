/*
	Glidix bootloader (gxboot)

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

#ifndef GXBOOT_H
#define GXBOOT_H

#define	GXBOOT_DEBUG

#ifdef GXBOOT_DEBUG
#	define	dtermput		termput
#	define	dtermputp64		termputp64
#	define 	dtermputd		termputd
#else
#	define	dtermput(...)
#	define	dtermputp64(...)
#	define	dtermputd(...)
#endif

#define	NULL				((void*)0)

#define	PT_PRESENT			(1 << 0)
#define	PT_WRITE			(1 << 1)

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

#define	PT_NULL				0
#define	PT_LOAD				1
#define	PT_DYNAMIC			2
#define	PT_INTERP			3
#define	PT_NOTE				4
#define	PT_SHLIB			5
#define	PT_PHDR				6

/* glidix boot-specific program headers */
#define	PT_GLIDIX_MMAP			0x60000000	/* map memory but do not modify */
#define	PT_GLIDIX_INITRD		0x60000001	/* put the initrd here */

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
#define	R_X86_64_GLOB_DAT		6
#define	R_X86_64_JUMP_SLOT		7
#define	R_X86_64_RELATIVE		8

typedef unsigned char			uint8_t;
typedef	unsigned short			uint16_t;
typedef	unsigned int			uint32_t;
typedef	unsigned long long		uint64_t;

typedef struct
{
	uint64_t sbhMagic;
	uint8_t  sbhBootID[16];
	uint64_t sbhFormatTime;
	uint64_t sbhWriteFeatures;
	uint64_t sbhReadFeatures;
	uint64_t sbhOptionalFeatures;
	uint64_t sbhResv[2];
	uint64_t sbhChecksum;
} GXFS_SuperblockHeader;

typedef struct
{
	uint64_t ihNext;
} GXFS_InodeHeader;

typedef struct
{
	uint32_t rhType;
	uint32_t rhSize;
} GXFS_RecordHeader;

typedef struct
{
	uint32_t drType;
	uint32_t drRecordSize;
	uint64_t drInode;
	uint8_t drInoType;
	char drName[];
} GXFS_DentRecord;

typedef struct
{
	uint32_t arType;
	uint32_t arRecordSize;
	uint64_t arLinks;
	uint32_t arFlags;
	uint16_t arOwner;
	uint16_t arGroup;
	uint64_t arSize;
	uint64_t arATime;
	uint64_t arMTime;
	uint64_t arCTime;
	uint64_t arBTime;
	uint32_t arANano;
	uint32_t arMNano;
	uint32_t arCNano;
	uint32_t arBNano;
	uint64_t arIXPerm;
	uint64_t arOXPerm;
	uint64_t arDXPerm;
} GXFS_AttrRecord;

typedef struct
{
	uint32_t trType;
	uint32_t trSize;
	uint64_t trDepth;
	uint64_t trHead;
} GXFS_TreeRecord;

typedef struct
{
	char					year[4];
	char					month[2];			// 1-12
	char					day[2];				// 1-31
	char					hour[2];			// 0-23
	char					minute[2];			// 0-59
	char					second[2];			// 0-59
	char					centiseconds[2];		// 0-99		(ignored really)
	uint8_t					timezone;			// from 0=GMT-12, to 100=GMT+13, in 15-minute intervals
} __attribute__ ((packed)) ISOPrimaryDateTime;

typedef struct
{
	uint8_t					type;				// == 1
	char					magic[5];			// "CD001"
	uint8_t					version;			// == 1
	uint8_t					unused;				// == 0
	char					bootsysname[32];		// ignore this
	char					volumeID[32];
	uint8_t					zeroes[8];
	uint32_t				volumeBlockCount;
	uint32_t				ignore1;
	uint8_t					ignore2[32];
	uint16_t				volumeCount;
	uint16_t				ignore3;
	uint16_t				volumeIndex;
	uint16_t				ignore4;
	uint16_t				blockSize;
	uint16_t				ignore5;
	uint8_t					ignore6[24];			// path table, we don't care
	uint8_t					rootDir[34];			// cast contents to ISODirentHeader.
	char					volumeSetID[128];
	char					publisherID[128];
	char					dataPreparerID[128];
	char					appID[128];
	char					copyrightFile[38];
	char					abstractFile[36];
	char					biblioFile[37];
	ISOPrimaryDateTime			dtCreation;
	ISOPrimaryDateTime			dtModification;
	ISOPrimaryDateTime			dtObsolete;
	ISOPrimaryDateTime			dtCanBeUsed;
	uint8_t					fileStructVersion;		// == 1
	uint8_t					ignore7;
} __attribute__ ((packed)) ISOPrimaryVolumeDescriptor;

typedef struct
{
	uint8_t					size;
	uint8_t					xattrSize;
	uint32_t				startLBA;
	uint32_t				ignore1;
	uint32_t				fileSize;
	uint32_t				ignore2;
	uint8_t					year;				// since 1990
	uint8_t					month;				// 1-12
	uint8_t					day;				// 1-31
	uint8_t					hour;				// 0-23
	uint8_t					minute;				// 0-59
	uint8_t					second;				// 0-59
	uint8_t					timezone;
	uint8_t					flags;
	uint8_t					zeroes[2];
	uint32_t				ignore3;
	uint8_t					filenameLen;
} __attribute__ ((packed)) ISODirentHeader;

typedef struct
{
	uint8_t					size;
	uint8_t					unused;
	uint16_t				numSectors;
	uint16_t				offset;
	uint16_t				segment;
	uint64_t				lba;
} DAP;

typedef struct
{
	char					filename[100];
	char					mode[8];
	char					uid[8];
	char					gid[8];
	char					size[12];
} TARFileHeader;

typedef struct
{
	/**
	 * Start with stuff defined for all filesystems.
	 */
	uint64_t				size;
	
	/**
	 * Now filesystem-specific stuff.
	 */
#if defined(GXBOOT_FS_GXFS)
	uint64_t				depth;
	uint64_t				head;
	uint64_t				bufferBase;
	uint8_t					buffer[4096];
#elif defined(GXBOOT_FS_ELTORITO)
	uint64_t				startLBA;
	uint64_t				currentLBA;
	uint8_t					buffer[2048];
#else
#error Unknown boot filesystem!
#endif
} FileHandle;

typedef struct
{
	uint32_t				sig;
	uint16_t				version;
	uint32_t				oemStrFar;
	uint8_t				caps[4];
	uint16_t				modeListOffset;
	uint16_t				modeListSegment;
	uint16_t				totalMemory;
} __attribute__ ((packed)) VbeInfoBlock;
extern VbeInfoBlock vbeInfoBlock;

typedef struct
{
	uint16_t				attributes;
	uint8_t				winA, winB;
	uint16_t				granularity;
	uint16_t				winsize;
	uint16_t				segA, segB;
	uint32_t				realFctPtr;
	uint16_t				pitch;

	uint16_t				width, height;
	uint8_t				Wchar, Ychar, planes, bpp, banks;
	uint8_t				memory_model, bank_size, image_pages;
	uint8_t				reserved0;

	uint8_t				red_mask, red_position;
	uint8_t				green_mask, green_position;
	uint8_t				blue_mask, blue_position;
	uint8_t				rsv_mask, rsv_position;
	uint8_t				directcolor_attributes;
 
	uint32_t				physbase;
	uint32_t				reserved1;
	uint16_t				reserved2;
} __attribute__ ((packed)) VbeModeInfo;
extern VbeModeInfo vbeModeInfo;

/**
 * A buffer filled with the boot ID by fsInit().
 */
extern uint8_t fsBootID[16];

typedef	uint64_t				Elf64_Addr;
typedef	uint16_t				Elf64_Half;
typedef	uint64_t				Elf64_Off;
typedef	signed int			Elf64_Sword;
typedef	signed long long		Elf64_Sxword;
typedef	uint32_t				Elf64_Word;
typedef	uint64_t				Elf64_Xword;

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
	Elf64_Addr			p_paddr;
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
	uint32_t			size;
	uint64_t			baseAddr;
	uint64_t			len;
	uint32_t			type;
} __attribute__ ((packed)) MemoryMap;

typedef struct
{
	uint64_t			baseAddr;
	uint64_t			len;
} MemorySlice;

/**
 * Pixel format as defined by DDI and the kernel; passed to describe the framebuffer.
 */
typedef struct
{
	/**
	 * BYTES per pixel (1, 2, 3 or 4).
	 */
	uint32_t					bpp;
	
	/**
	 * Red, green, blue and alpha masks. For less than 32 bits per pixel,
	 * only the low bits shall be used. If a mask is zero, that means the
	 * component is missing. For example, alpha may not be supported.
	 */
	uint32_t					redMask;
	uint32_t					greenMask;
	uint32_t					blueMask;
	uint32_t					alphaMask;
	
	/**
	 * Number of unused bytes between pixels and scanlines.
	 */
	uint32_t					pixelSpacing;
	uint32_t					scanlineSpacing;
} PixelFormat;

/**
 * Kernel information structure; this is passed to the kernel.
 */
#define	KB_FEATURE_BOOTID		(1 << 0)
#define	KB_FEATURE_VIDEO		(1 << 1)
typedef struct
{
	uint64_t				features;			/* 0x00 */
	uint64_t				kernelMain;			/* 0x08 */
	uint64_t				gdtPointerVirt;			/* 0x10 */
	uint32_t				pml4Phys;			/* 0x18 */
	uint32_t				mmapSize;			/* 0x1C */
	uint64_t				mmapVirt;			/* 0x20 */
	uint64_t				initrdSize;			/* 0x28 */
	uint64_t				end;				/* 0x30 */
	uint64_t				initrdSymtabOffset;		/* 0x38 */
	uint64_t				initrdStrtabOffset;		/* 0x40 */
	uint64_t				numSymbols;			/* 0x48 */
	
	/* only when KB_FEATURE_BOOTID is set */
	uint8_t					bootID[16];			/* 0x50 */
	
	/* only when KB_FEATURE_VIDEO is set */
	uint64_t				framebuffer;			/* 0x60 */
	uint64_t				backbuffer;			/* 0x68 */
	uint32_t				screenWidth;			/* 0x70 */
	uint32_t				screenHeight;			/* 0x74 */
	PixelFormat				pixelFormat;			/* 0x78 */
} KernelInfo;

extern DAP dap;
extern uint8_t sectorBuffer[];

void memset(void *buffer, unsigned char b, unsigned int size);
void memcpy(void *dest, const void *src, unsigned int size);
int  memcmp(const void *a, const void *b, unsigned int size);
int  strcmp(const char *a, const char *b);

/**
 * Call the BIOS to read a sector from disk. Implemented in entry.asm.
 * Arguments taken from DAP, returned in sectorBuffer. Calls INT 0x18 if
 * read failed.
 */
void biosRead();

/**
 * Get information about a video mode. Implemented in entry.asm
 * Returns 0 on success, VBE error number (AH) on error.
 */
int vbeGetModeInfo(uint16_t modeNumber);

/**
 * Switch VBE mode. Implemented in entry.asm. Returns 0 on success, VBE
 * error number (AH) on error.
 */
int vbeSwitchMode(uint16_t modeNumber);

/**
 * Initialize the filesystem driver.
 */
void fsInit();

/**
 * Open a file. Returns 0 on success, -1 on error.
 */
int openFile(FileHandle *fh, const char *path);

/**
 * Read a file.
 */
void readFile(FileHandle *fh, void *buffer, uint64_t size, uint64_t offset);

/**
 * Get a BIOS memory map entry. Returns the index of the next entry. Sets 'ok' to 0 if the
 * carry flag has been set.
 */
uint32_t biosGetMap(uint32_t index, void *put, int *ok);

/**
 * Jump to the 64-bit kernel.
 */
void go64(KernelInfo *kinfo, uint64_t kinfoVirt);

/**
 * Write to the terminal.
 */
void termput(const char *str);

/**
 * Print a decimal number.
 */
void termputd(uint32_t num);

/**
 * Print a 64-bit hex number.
 */
void termputp64(uint64_t addr);

#endif
