/*
	Glidix kernel

	Copyright (c) 2014-2016, Madd Games.
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

#include <glidix/shared.h>
#include <glidix/memory.h>
#include <glidix/vfs.h>
#include <glidix/sched.h>
#include <glidix/string.h>
#include <glidix/elf64.h>
#include <glidix/console.h>

int libOpen(const char *path, uint64_t loadAddr, libInfo *info)
{
	int error;
	File *fp = vfsOpen(path, VFS_CHECK_ACCESS, &error);

	Elf64_Ehdr elfHeader;
	if (vfsRead(fp, &elfHeader, sizeof(Elf64_Ehdr)) < sizeof(Elf64_Ehdr))
	{
		vfsClose(fp);
		return -1;
	};

	if (fp->seek == NULL)
	{
		vfsClose(fp);
		return -1;
	};

	if (memcmp(elfHeader.e_ident, "\x7f" "ELF", 4) != 0)
	{
		vfsClose(fp);
		return -1;
	};

	if (elfHeader.e_ident[EI_CLASS] != ELFCLASS64)
	{
		vfsClose(fp);
		return -1;
	};

	if (elfHeader.e_ident[EI_DATA] != ELFDATA2LSB)
	{
		vfsClose(fp);
		return -1;
	};

	if (elfHeader.e_ident[EI_VERSION] != 1)
	{
		vfsClose(fp);
		return -1;
	};

	if (elfHeader.e_type != ET_DYN)
	{
		vfsClose(fp);
		return -1;
	};

	if (elfHeader.e_phentsize != sizeof(Elf64_Phdr))
	{
		vfsClose(fp);
		return -1;
	};

	if (elfHeader.e_phentsize % sizeof(Elf64_Phdr))
	{
		vfsClose(fp);
		return -1;
	};

	unsigned int numProgHeads = elfHeader.e_phnum;
	size_t sizeProgHeads = elfHeader.e_phentsize * numProgHeads;
	Elf64_Phdr *progHeads = (Elf64_Phdr*) kmalloc(sizeProgHeads);

	fp->seek(fp, elfHeader.e_phoff, SEEK_SET);
	if (vfsRead(fp, progHeads, sizeProgHeads) < sizeProgHeads)
	{
		kfree(progHeads);
		vfsClose(fp);
		return -1;
	};

	// collect the segments
	Elf64_Phdr *hdrText = NULL;
	Elf64_Phdr *hdrData = NULL;
	Elf64_Phdr *hdrDynamic = NULL;

	unsigned int i;
	for (i=0; i<numProgHeads; i++)
	{
		Elf64_Phdr *hdr = &progHeads[i];
		if ((hdr->p_type == PT_NULL) || (hdr->p_type == PT_INTERP) || (hdr->p_type == PT_NOTE))
		{
			continue;
		};

		if (hdr->p_type == PT_LOAD)
		{
			if (hdr->p_flags & PF_W)
			{
				if (hdrData == NULL)
				{
					hdrData = hdr;
				}
				else
				{
					kfree(progHeads);
					vfsClose(fp);
					return -1;
				};
			}
			else
			{
				if (hdrText == NULL)
				{
					hdrText = hdr;
				}
				else
				{
					kfree(progHeads);
					vfsClose(fp);
					return -1;
				};
			};
		}
		else if (hdr->p_type == PT_DYNAMIC)
		{
			if (hdrDynamic == NULL)
			{
				hdrDynamic = hdr;
			}
			else
			{
				kfree(progHeads);
				vfsClose(fp);
				return -1;
			};
		};
	};

	if ((hdrText == NULL) || (hdrData == NULL) || (hdrDynamic == NULL))
	{
		kfree(progHeads);
		vfsClose(fp);
		return -1;
	};

	if (loadAddr % 0x200000)
	{
		loadAddr = (loadAddr & ~0x1FFFFF) + 0x200000;
	};

	size_t textPageCount = ((hdrText->p_vaddr + hdrText->p_memsz) / 0x1000) - (hdrText->p_vaddr / 0x1000) + 1;
	size_t dataPageCount = ((hdrData->p_vaddr + hdrData->p_memsz) / 0x1000) - (hdrData->p_vaddr / 0x1000) + 1;

	FrameList *flText = palloc(textPageCount);
	FrameList *flData = palloc(dataPageCount);

	size_t indexBase = loadAddr / 0x1000;
	size_t indexText = indexBase + hdrText->p_vaddr / 0x1000;
	size_t indexData = indexBase + hdrData->p_vaddr / 0x1000;

	ProcMem *pm = getCurrentThread()->pm;
	if (AddSegment(pm, indexText, flText, PROT_READ | PROT_EXEC) != 0)
	{
		pdownref(flText);
		pdownref(flData);
		kfree(progHeads);
		vfsClose(fp);
		return -1;
	};

	if (AddSegment(pm, indexData, flData, PROT_READ | PROT_WRITE) != 0)
	{
		DeleteSegment(pm, indexText);
		pdownref(flText);
		pdownref(flData);
		kfree(progHeads);
		vfsClose(fp);
		return -1;
	};

	pdownref(flText);
	pdownref(flData);
	SetProcessMemory(pm);

	// load segments into memory.
	memset((void*)(loadAddr + hdrText->p_vaddr), 0, hdrText->p_memsz);
	memset((void*)(loadAddr + hdrData->p_vaddr), 0, hdrData->p_memsz);

	fp->seek(fp, hdrText->p_offset, SEEK_SET);
	vfsRead(fp, (void*)(loadAddr + hdrText->p_vaddr), hdrText->p_filesz);
	fp->seek(fp, hdrData->p_offset, SEEK_SET);
	vfsRead(fp, (void*)(loadAddr + hdrData->p_vaddr), hdrData->p_filesz);

	info->dynSize = hdrDynamic->p_memsz;
	info->dynSection = (Elf64_Dyn*) (loadAddr + hdrDynamic->p_vaddr);
	info->loadAddr = loadAddr;
	info->nextLoadAddr = loadAddr + hdrData->p_vaddr + hdrData->p_memsz;
	info->textIndex = indexText;
	info->dataIndex = indexData;

	kfree(progHeads);
	vfsClose(fp);

	return 0;
};

void libClose(libInfo *info)
{
	ProcMem *pm = getCurrentThread()->pm;
	DeleteSegment(pm, info->textIndex);
	DeleteSegment(pm, info->dataIndex);
};
