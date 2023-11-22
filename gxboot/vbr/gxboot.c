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

#include "gxboot.h"
#include "mem.h"

int consoleX, consoleY;
char *vidmem = (char*) 0xB8000;
uint64_t blockBase;			/* LBA of start of block table */

void termput(const char *str)
{
	for (; *str!=0; str++)
	{
		if (*str == '\n')
		{
			consoleX = 0;
			consoleY++;
	
			if (consoleY == 25)
			{
				consoleY--;
				
				int i;
				for (i=0; i<80*24*2; i++)
				{
					vidmem[i] = vidmem[i+80*2];
				};
				
				for (i=80*24*2; i<80*25*2; i+=2)
				{
					vidmem[i] = 0x20;
					vidmem[i+1] = 0x07;
				};
			};
		}
		else
		{
			vidmem[2 * (consoleY * 80 + consoleX)] = *str;
			vidmem[2 * (consoleY * 80 + consoleX) + 1] = 0x07;
			
			if ((++consoleX) == 80)
			{
				consoleX = 0;
				consoleY++;
			};
			
			if (consoleY == 25)
			{
				consoleY--;
				
				int i;
				for (i=0; i<80*24*2; i++)
				{
					vidmem[i] = vidmem[i+80*2];
				};
				
				for (i=80*24*2; i<80*25*2; i+=2)
				{
					vidmem[i] = 0x20;
					vidmem[i+1] = 0x07;
				};
			};
		};
	};
};

void termputd(uint32_t num)
{
	char buffer[64];
	char *put = &buffer[63];
	*put = 0;
	
	do
	{
		*--put = '0' + (num % 10);
		num /= 10;
	} while (num != 0);
	
	termput(put);
};

void termputp64(uint64_t addr)
{
	termput("0x");
	char buffer[64];
	char *put = &buffer[63];
	*put = 0;
	
	do
	{
		uint64_t digit = (addr % 16);
		if (digit < 10)
		{
			*--put = '0' + digit;
		}
		else
		{
			*--put = 'A' + digit - 10;
		};
		addr /= 16;
	} while (addr != 0);
	
	termput(put);
};

const char *strings;
Elf64_Sym *symtab;
int numSyms;

uint64_t getSymbol(const char *name)
{
	int i;
	for (i=0; i<numSyms; i++)
	{
		const char *symname = &strings[symtab[i].st_name];
		if (strcmp(symname, name) == 0)
		{
			return symtab[i].st_value;
		};
	};
	
	return 0;
};

typedef struct
{
	uint16_t width, height;
} ScreenSize;

// video modes that we accept; those are the safe modes that all monitors should support
static ScreenSize okSizes[] = {
	{720, 480},
	{640, 480},
	
	// LIST TERMINATOR
	{0, 0}
};

static int fixedWidth;
static int fixedHeight;

static int isOkSize(uint16_t width, uint16_t height)
{
	if (fixedWidth != 0)
	{
		return (width == fixedWidth) && (height == fixedHeight);
	};
	
	ScreenSize *scan;
	for (scan=okSizes; scan->width!=0; scan++)
	{
		if (scan->width == width && scan->height == height)
		{
			return 1;
		};
	};
	
	return 0;
};

char *nextToken(char delim, char **saveptr)
{
	char *pos = *saveptr;
	while (*pos == delim) pos++;
	
	if (*pos == 0) return NULL;
	
	char *result = pos;
	while (*pos != delim && *pos != 0) pos++;
	while (*pos == delim) *pos++ = 0;
	*saveptr = pos;
	
	return result;
};

static int parseInt(const char *spec)
{
	int result = 0;
	
	while (*spec != 0)
	{
		if (*spec < '0' || *spec > '9')
		{
			return -1;
		};
		
		result = result * 10 + ((*spec) - '0');
		spec++;
	};
	
	return result;
};

static void parseConfigLine(char *line)
{
	if (line[0] == '#') return;
	
	char *cmd = nextToken(' ', &line);
	if (cmd == NULL) return;
	
	if (strcmp(cmd, "vesa-res") == 0)
	{
		char *widthSpec = nextToken('x', &line);
		char *heightSpec = nextToken('x', &line);
		
		if (widthSpec == NULL || heightSpec == NULL)
		{
			termput("ERROR: vesa-res: invalid syntax\n");
			return;
		};
		
		fixedWidth = parseInt(widthSpec);
		fixedHeight = parseInt(heightSpec);
		
		if (fixedWidth < 1 || fixedHeight < 1)
		{
			termput("ERROR: vesa-res: invalid resolution specified");
			fixedWidth = 0;
			return;
		};
	}
	else
	{
		termput("ERROR: invalid command: "); termput(cmd); termput("\n");
	};
};

void bmain()
{
	fixedWidth = 0;
	consoleX = 0;
	consoleY = 0;
	memset(vidmem, 0, 80*25*2);
	
	int i;

	memInit();
	fsInit();

	dtermput("Loading /boot/loader.conf... ");
	FileHandle conf;
	if (openFile(&conf, "/boot/loader.conf") != 0)
	{
		dtermput("FAILED\n");
		dtermput("NOTE: /boot/loader.conf not found: using default settings\n");
	}
	else
	{
		dtermput("OK\n");
		
		char *data = (char*) balloc(1, conf.size);
		readFile(&conf, data, conf.size, 0);
		
		char *line;
		for (line=nextToken('\n', &data); line!=NULL; line=nextToken('\n', &data))
		{
			termput(line);
			termput("\n");
			parseConfigLine(line);
		};
	};
	
	dtermput("Finding a suitable video mode... ");
	if (vbeInfoBlock.sig != (*((const uint32_t*)"VESA")))
	{
		dtermput("FAILED\n");
		termput("ERROR: Invalid VBE information block");
		return;
	};
	
	if (vbeInfoBlock.version < 0x0200)
	{
		dtermput("FAILED\n");
		termput("ERROR: VBE 2.0 not supported");
		return;
	};
	
	// find the desired video mode
	uint16_t videoMode = 0xFFFF;
	uint16_t bestWidth = 0;
	uint32_t modesAddr = ((uint32_t) vbeInfoBlock.modeListSegment << 4) + (uint32_t) vbeInfoBlock.modeListOffset;
	uint16_t *modes;
	
	for (modes=(uint16_t*)modesAddr; *modes!=0xFFFF; modes++)
	{
		if (vbeGetModeInfo(*modes) != 0)
		{
			continue;
		};
		
		if ((vbeModeInfo.attributes & 0x90) != 0x90)
		{
			// this is not a graphics mode with LFB
			continue;
		};
		
		if (vbeModeInfo.memory_model != 6)
		{
			// not a direct color mode
			continue;
		};

		if (vbeModeInfo.bpp != 32)
		{
			// not 32-bit
			continue;
		};

		if (vbeModeInfo.red_mask != 8 || vbeModeInfo.green_mask != 8 || vbeModeInfo.blue_mask != 8)
		{
			// channels are not all 8-bit
			continue;
		};
		
		if (!isOkSize(vbeModeInfo.width, vbeModeInfo.height))
		{
			// wrong resolution
			continue;
		};

		// all good
		if (vbeModeInfo.width > bestWidth)
		{
			videoMode = *modes;
			bestWidth = vbeModeInfo.width;
		};
	};
	
	if (videoMode == 0xFFFF)
	{
		dtermput("FAILED\n");
		termput("ERROR: No acceptable video mode found\n");
		
		termput("Listing all supported modes:\n");
		for (modes=(uint16_t*)modesAddr; *modes!=0xFFFF; modes++)
		{
			if (vbeGetModeInfo(*modes) != 0)
			{
				continue;
			};
			
			if ((vbeModeInfo.attributes & 0x90) != 0x90)
			{
				// this is not a graphics mode with LFB
				continue;
			};
			
			if (vbeModeInfo.memory_model != 6)
			{
				// not a direct color mode
				continue;
			};

			if (vbeModeInfo.bpp != 32)
			{
				// not 32-bit
				continue;
			};

			if (vbeModeInfo.red_mask != 8 || vbeModeInfo.green_mask != 8 || vbeModeInfo.blue_mask != 8)
			{
				// channels are not all 8-bit
				continue;
			};
			
			termputd(vbeModeInfo.width); termput("x"); termputd(vbeModeInfo.height); termput("\n");
		};
	
		return;
	};

	dtermput("OK\n");
	
	dtermput("Finding /boot/vmglidix.tar... ");
	FileHandle initrd;
	if (openFile(&initrd, "/boot/vmglidix.tar") != 0)
	{
		dtermput("FAILED\n");
		termput("ERROR: Cannot find /boot/vmglidix.tar\n");
		return;
	};
	
	dtermput("OK\n");
	
	dtermput("Looking for kernel.so... ");
	uint64_t pos = 0;
	uint64_t size;
	int found = 0;
	
	while (pos < initrd.size)
	{
		TARFileHeader header;
		
		readFile(&initrd, &header, sizeof(TARFileHeader), pos);
		
		size = 0;
		const char *scan;
		for (scan=header.size; *scan!=0; scan++)
		{
			size = (size << 3) + ((*scan)-'0');
		};
		
		if (strcmp(header.filename, "kernel.so") == 0)
		{
			found = 1;
			break;
		};
		
		size = (size + 511) & ~511;
		pos += 512 * (size + 1);
	};
	
	pos += 512;
	if (!found)
	{
		dtermput("FAILED\n");
		termput("ERROR: Cannot find kernel.so in /boot/vmglidix.tar!\n");
		return;
	};
	
	dtermput("OK\n");
	
	Elf64_Ehdr header;
	readFile(&initrd, &header, sizeof(Elf64_Ehdr), pos);
	
	dtermput("Validating ELF64 header... ");
	if (memcmp(header.e_ident, "\x7f" "ELF", 4) != 0)
	{
		dtermput("FAILED\n");
		termput("ERROR: Invalid ELF64 signature in kernel.so\n");
		return;
	};

	if (header.e_ident[EI_CLASS] != ELFCLASS64)
	{
		dtermput("FAILED\n");
		termput("ERROR: kernel.so is not an ELF64 file");
		return;
	};

	if (header.e_ident[EI_DATA] != ELFDATA2LSB)
	{
		dtermput("FAILED\n");
		termput("ERROR: kernel.so is not little endian");
		return;
	};

	if (header.e_ident[EI_VERSION] != 1)
	{
		dtermput("FAILED\n");
		termput("kernel.so is not ELF64 version 1");
		return;
	};

	if (header.e_type != ET_EXEC)
	{
		dtermput("FAILED\n");
		termput("ERROR: kernel.so is not an executable");
		return;
	};

	if (header.e_phentsize != sizeof(Elf64_Phdr))
	{
		dtermput("FAILED\n");
		termput("ERROR: kernel.so program header size unexpected");
		return;
	};
	
	dtermput("OK\n");

	dtermput("Loading initrd to memory... ");
	
	void *initrdStart = balloc(0x1000, initrd.size);
	memset(initrdStart, 0, initrd.size);
	
	readFile(&initrd, initrdStart, initrd.size, 0);
	dtermput("OK\n");
	
	char *elfPtr = (char*) initrdStart + pos;
	Elf64_Ehdr *elfHeader = (Elf64_Ehdr*) elfPtr;
	Elf64_Phdr *pheads = (Elf64_Phdr*) &elfPtr[elfHeader->e_phoff];
	
	// now load the segments into memory
	for (i=0; i<elfHeader->e_phnum; i++)
	{
		if (pheads[i].p_type == PT_LOAD)
		{
			void *buffer = balloc(0x1000, pheads[i].p_memsz);
			memset(buffer, 0, pheads[i].p_memsz);
			memcpy(buffer, elfPtr + pheads[i].p_offset, pheads[i].p_filesz);
			
			mmap(pheads[i].p_vaddr, (uint32_t) buffer, pheads[i].p_memsz);
		}
		else if (pheads[i].p_type == PT_GLIDIX_MMAP)
		{
			if (pheads[i].p_paddr & 0xFFF)
			{
				termput("ERROR: kernel.so contains a program header with non-page-aligned physical address!");
				return;
			};
			
			mmap(pheads[i].p_vaddr, pheads[i].p_paddr, pheads[i].p_memsz);
		}
		else if (pheads[i].p_type == PT_GLIDIX_INITRD)
		{
			if (pheads[i].p_vaddr & 0xFFF)
			{
				termput("ERROR: kernel.so contains a program header with non-page-aligned virtual address!");
				return;
			};
		
			mmap(pheads[i].p_vaddr, (uint32_t) initrdStart, pheads[i].p_memsz);
		};
	};
	
	// find the string and symbol table sections
	Elf64_Shdr *sections = (Elf64_Shdr*) (elfPtr + elfHeader->e_shoff);
	
	// find the symbol table
	symtab = NULL;
	numSyms = 0;
	strings = NULL;

	for (i=0; i<elfHeader->e_shnum; i++)
	{
		if (sections[i].sh_type == SHT_SYMTAB)
		{
			symtab = (Elf64_Sym*) (elfPtr + sections[i].sh_offset);
			numSyms = sections[i].sh_size / sections[i].sh_entsize;
			strings = elfPtr + sections[sections[i].sh_link].sh_offset;
		};
	};
	
	if (symtab == NULL)
	{
		termput("ERROR: kernel.so does not contain a symbol table!");
		return;
	};
	
	// get the bottom of the stack
	uint64_t stack = getSymbol("_stackBottom");
	termput("_stackBottom at ");
	termputp64(stack);
	termput("\n");
	
	if (stack == 0)
	{
		termput("ERROR: kernel.so does not contain the _stackBottom symbol!");
		return;
	};
	
	// allocate 2KB for the memory map
	// TODO: Once we map all physical memory, just pass the appropriate virtual
	// address, pointing to the map generated in memInit().
#if 0
	uint64_t mmapBase = (stack - 0x800ULL) & ~0xFULL;
	uint32_t mmapSize = 0;
	
	// build the memory map
	MemoryMap *mmapPut = (MemoryMap*) virt2phys(mmapBase);
	if (mmapPut == NULL)
	{
		termput("ERROR: cannot resolve address for memory map! ");
		termputp64(mmapBase);
		return;
	};
	
	uint32_t entIndex = 0;

	dtermput("Loading memory map... ");
	do
	{
		int ok = 1;
		entIndex = biosGetMap(entIndex, &mmapPut->baseAddr, &ok);
		if (!ok) break;
		
		mmapPut->size = 20;
		mmapPut++;
		mmapSize += 24;
	} while (entIndex != 0);
	
	dtermput("OK\n");
#endif
	
	// Now allocate the information structure.
	uint64_t kernelInfoVirt = (stack - sizeof(KernelInfo)) & (~0xFULL);
	KernelInfo *kinfo = (KernelInfo*) virt2phys(kernelInfoVirt);
	
	if (kinfo == NULL)
	{
		termput("ERROR: cannot resolve address for kernel info!");
		return;
	};
	
	kinfo->features = KB_FEATURE_BOOTID | KB_FEATURE_VIDEO;
	kinfo->kernelMain = getSymbol("kmain");
	if (kinfo->kernelMain == 0)
	{
		termput("ERROR: kernel.so does not contain the kmain symbol!");
		return;
	};
	
	kinfo->gdtPointerVirt = getSymbol("GDTPointer");
	if (kinfo->gdtPointerVirt == 0)
	{
		termput("ERROR: kernel.so does not contain the GDTPointer symbol!");
		return;
	};
	
	// get information on the chosen mode.
	if (vbeGetModeInfo(videoMode) != 0)
	{
		termput("ERROR: Failed to read video mode information!\n");
		return;
	};

	// map the framebuffer
	mmap(
		PHYS_MAP_BASE + (vbeModeInfo.physbase & ~0xFFF),
		vbeModeInfo.physbase & ~0xFFF,
		vbeModeInfo.height * vbeModeInfo.pitch + 0x1000
	);

	void *backbuffer = balloc(0x1000, vbeModeInfo.height * vbeModeInfo.pitch);
	uint64_t backvirt = PHYS_MAP_BASE + (uint64_t) (uint32_t) backbuffer;
	
	kinfo->pml4Phys = (uint32_t) pml4;
	kinfo->mmapSize = memGetBiosMapSize();
	kinfo->mmapVirt = PHYS_MAP_BASE + BIOSMAP_ADDR;
	kinfo->initrdSize = initrd.size;
	kinfo->end = (uint64_t) (uint32_t) balloc(0x1000, 0);
	kinfo->initrdSymtabOffset = (uint64_t) (uint32_t) ((char*) symtab - (char*) initrdStart);
	kinfo->initrdStrtabOffset = (uint64_t) (uint32_t) ((char*) strings - (char*) initrdStart);
	kinfo->numSymbols = numSyms;
	memcpy(kinfo->bootID, fsBootID, 16);
	
	kinfo->framebuffer = PHYS_MAP_BASE + vbeModeInfo.physbase;
	kinfo->backbuffer = backvirt;
	kinfo->screenWidth = (uint32_t) vbeModeInfo.width;
	kinfo->screenHeight = (uint32_t) vbeModeInfo.height;
	kinfo->pixelFormat.bpp = 4;
	kinfo->pixelFormat.redMask = (0xFF << vbeModeInfo.red_position);
	kinfo->pixelFormat.greenMask = (0xFF << vbeModeInfo.green_position);
	kinfo->pixelFormat.blueMask = (0xFF << vbeModeInfo.blue_position);
	kinfo->pixelFormat.alphaMask = ~(kinfo->pixelFormat.redMask | kinfo->pixelFormat.greenMask | kinfo->pixelFormat.blueMask);
	kinfo->pixelFormat.pixelSpacing = 0;
	kinfo->pixelFormat.scanlineSpacing = vbeModeInfo.pitch - vbeModeInfo.width * 4;
	
	while (1);

	// switch modes
	if (vbeSwitchMode(videoMode) != 0)
	{
		termput("ERROR: Failed to switch video mode!\n");
		return;
	};

	// identity-map our area
	// we allocate this AFTER passing the end pointer to the kerne l, because it's temporary.
	// the kernel will destroy those page tables once we pass control to it, and they are no
	// longer needed, so we save some memory
	mmap(0x5000, 0x5000, 0xFB000);

	// go to 64-bit mode
	go64(kinfo, kernelInfoVirt);
};
