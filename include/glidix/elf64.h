/*
	Glidix kernel

	Copyright (c) 2014, Madd Games.
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

#ifndef __glidix_elf64_h
#define __glidix_elf64_h

#include <glidix/common.h>

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
#define	ET_EXEC				2
#define	DT_DYN				3

#define	PT_NULL				0
#define	PT_LOAD				1

#define	PF_X				0x1
#define	PF_W				0x2
#define	PF_R				0x4

/**
 * Please remember that all the structures here are NOT packed, because ELF64 has all the
 * types aligned.
 */

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

int elfExec(Regs *regs, const char *path);

#endif
