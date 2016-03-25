/*
	Glidix Loader

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

#include <stdarg.h>
#include <stdint.h>
#include <stddef.h>

/**
 * Comment out this line to disable debug messages.
 */
//#define	GXLD_DEBUG

#ifdef GXLD_DEBUG
#	define DEBUG	kprintf
#else
#	define DEBUG(...)
#endif

#define	VRAM_BASE 0xB8000
#define	NO_BUF_BLOCK 0xFFFFFFFFFFFFFFFF

typedef struct
{
	// cis_offset field is at 0x7C10.
	char padding[16];
	uint64_t cisOffset;
} GXFSBootHeader;
extern GXFSBootHeader gxfsBootHeader;

int numCylinders;
int headsPerCylinder;
int sectorsPerTrack;
uint8_t bootDrive;
char *iobuf;				// 8KB, will be allocated on the stack
uint64_t currentBlock = NO_BUF_BLOCK;
uint64_t startLBA;

extern char end;
uint32_t currentPlace = (uint32_t) &end;

typedef struct
{
	uint32_t			magic;
	uint32_t			flags;
	uint32_t			checksum;
	uint32_t			base;
	uint32_t			physbase;
	uint32_t			bss;
	uint32_t			end;
	uint32_t			entry;
} __attribute__ ((packed)) MultibootHeader;

typedef struct
{
	uint32_t flags;
	uint32_t memLower;
	uint32_t memUpper;
	uint32_t bootDevice;
	uint32_t cmdLine;
	uint32_t modsCount;
	uint32_t modsAddr;
	uint8_t  ignore[16];
	uint32_t mmapLen;
	uint32_t mmapAddr;
} __attribute__ ((packed)) MultibootInfo;

typedef struct
{
	uint32_t		size;
	uint64_t		baseAddr;
	uint64_t		len;
	uint32_t		type;
} __attribute__ ((packed)) MultibootMemoryMap;

typedef struct
{
	uint32_t		modStart;
	uint32_t		modEnd;
	uint32_t		str;
	uint32_t		zero;
} __attribute__ ((packed)) MultibootModule;

typedef struct
{
	uint8_t				size;
	uint8_t				zero;
	uint16_t			numSectors;
	uint16_t			offset;
	uint16_t			segment;
	uint64_t			lba;
} __attribute__ ((packed)) DAP;

typedef struct
{
	char				cisMagic[4];
	uint64_t			cisTotalIno;
	uint64_t			cisInoPerSection;
	uint64_t			cisTotalBlocks;
	uint64_t			cisBlocksPerSection;
	uint16_t			cisBlockSize;
	int64_t				cisCreateTime;
	uint64_t			cisFirstDataIno;
	uint64_t			cisOffSections;
	uint16_t			cisZero;
} __attribute__ ((packed)) gxfsCIS;

gxfsCIS cis;

typedef struct
{
	uint64_t			sdOffMapIno;
	uint64_t			sdOffMapBlocks;
	uint64_t			sdOffTabIno;
	uint64_t			sdOffTabBlocks;
} __attribute__ ((packed)) gxfsSD;

typedef struct
{
	uint64_t			fOff;
	uint64_t			fBlock;
	uint32_t			fExtent;
} __attribute__ ((packed)) gxfsFragment;

typedef struct
{
	uint16_t			inoMode;
	uint64_t			inoSize;
	uint8_t				inoLinks;
	int64_t				inoCTime;
	int64_t				inoATime;
	int64_t				inoMTime;
	uint16_t			inoOwner;
	uint16_t			inoGroup;
	gxfsFragment			inoFrags[14];
	uint64_t			inoExFrag;
	char				inoPad[32];
} __attribute__ ((packed)) gxfsInode;

typedef struct
{
	uint64_t			deInode;
	uint8_t				deNextSz;
	uint8_t				deNameLen;
	char				deName[];
} __attribute__ ((packed)) gxfsDirent;

typedef struct
{
	uint32_t			dhCount;
	uint8_t				dhFirstSz;
} __attribute__ ((packed)) gxfsDirHeader;

typedef struct
{
	uint16_t			xftCount;
	uint64_t			xftNext;
	gxfsFragment			xftFrags[];
} __attribute__ ((packed)) gxfsXFT;

int gxfsNumSections;
gxfsSD *sections;

/**
 * Registers for when we return to rmode to do a BIOS call.
 */
typedef union
{
	struct
	{
		uint32_t	eax;
		uint32_t	ebx;
		uint32_t	ecx;
		uint32_t	edx;
	} e;
	
	struct
	{
		uint16_t	ax, ign0;	// 0
		uint16_t	bx, ign1;	// 4
		uint16_t	cx, ign2;	// 8
		uint16_t	dx, ign3;	// 12
		uint16_t	si;		// 16
		uint16_t	di;		// 18
		uint16_t	es;		// 20
		uint16_t	flags;		// 22; return only
	} x;
	
	struct
	{
		uint8_t		al, ah;	uint16_t ign0;
		uint8_t		bl, bh;	uint16_t ign1;
		uint8_t		cl, ch;	uint16_t ign2;
		uint8_t		dl, dh;	uint16_t ign3;
	} h;
} Regs;

/**
 * In stage2_pm.asm.
 */
void bios_call(int intno, Regs *regs);

static struct
{
	uint32_t curX, curY;
	uint32_t curColor;
	uint8_t putcon;
} consoleState;

void clearScreen();

void memcpy(void *dest_, const void *src_, size_t count)
{
	char *dest = (char*) dest_;
	const char *src = (const char*) src_;
	
	while (count--)
	{
		*dest++ = *src++;
	};
};

void memset(void *dest_, char byte, size_t count)
{
	char *dest = (char*) dest_;
	while (count--) *dest++ = byte;
};

int memcmp(const void *a_, const void *b_, size_t count)
{
	const char *a = (const char*) a_;
	const char *b = (const char*) b_;
	
	while (count--)
	{
		if (*a++ != *b++) return 1;
	};
	
	return 0;
};

void initConsole()
{
	clearScreen();
};

static inline void outb(uint16_t port, uint8_t val)
{
	asm volatile ( "outb %0, %1" : : "a"(val), "Nd"(port) );
}

static void updateVGACursor()
{
	uint32_t pos = consoleState.curY * 80 + consoleState.curX;
	outb(0x3D4, 0x0F);
	outb(0x3D5, pos & 0xFF);
	outb(0x3D4, 0x0E);
	outb(0x3D5, (pos >> 8) & 0xFF);
};

void clearScreen()
{
	consoleState.curX = 0;
	consoleState.curY = 0;
	consoleState.curColor = 0x07;
	consoleState.putcon = 1;
	
	uint8_t *videoram = (uint8_t*) VRAM_BASE;
	uint32_t i;
	
	for (i=0; i<80*25; i++)
	{
		videoram[2*i+0] = 0;
		videoram[2*i+1] = 0x07;
	};
	
	updateVGACursor();
};

void setCursorPos(uint8_t x, uint8_t y)
{
	consoleState.curX = x;
	consoleState.curY = y;
	updateVGACursor();
};

static void scroll()
{
	uint8_t *vidmem = (uint8_t*) VRAM_BASE;

	uint32_t i;
	for (i=0; i<2*80*24; i++)
	{
		vidmem[i] = vidmem[i+160];
	};

	for (i=80*24; i<80*25; i++)
	{
		vidmem[2*i+0] = 0;
		vidmem[2*i+1] = 0x07;
	};

	consoleState.curY--;
	updateVGACursor();
};

static void kputch(char c)
{
	if (consoleState.curY == 25) scroll();
	
	if (c == '\n')
	{
		consoleState.curX = 0;
		consoleState.curY++;
		if (consoleState.curY == 25) scroll();
	}
	else if (c == '\r')
	{
		consoleState.curX = 0;
	}
	else if (c == '\b')
	{
		if (consoleState.curX == 0)
		{
			if (consoleState.curY == 0) return;
			consoleState.curY--;
			consoleState.curX = 80;
		};
		consoleState.curX--;
		uint8_t *vidmem = (uint8_t*) (VRAM_BASE + 2 * (consoleState.curY * 80 + consoleState.curX));
		*vidmem++ = 0;
		*vidmem++ = consoleState.curColor;
	}
	else if (c == '\t')
	{
		consoleState.curX = (consoleState.curX/8+1)*8;
		if (consoleState.curX >= 80)
		{
			consoleState.curY++;
			consoleState.curX -= 80;
			if (consoleState.curY == 25) scroll();
		};
	}
	else
	{
		uint8_t *vidmem = (uint8_t*) (VRAM_BASE + 2 * (consoleState.curY * 80 + consoleState.curX));
		*vidmem++ = c;
		*vidmem++ = consoleState.curColor;
		consoleState.curX++;

		if (consoleState.curX == 80)
		{
			consoleState.curX = 0;
			consoleState.curY++;
		};
	};
};

static void kputs(const char *str)
{
	while (*str != 0)
	{
		kputch(*str++);
	};

	updateVGACursor();
};

static void put_d(int d)
{
	if (d < 0)
	{
		kputch('-');
		d = -d;
	};

	char buffer[20];
	buffer[19] = 0;

	char *ptr = &buffer[18];
	do
	{
		*ptr = '0' + (d % 10);
		ptr--;
		d /= 10;
	} while (d != 0);

	kputs(ptr+1);
};

static void put_x(int d)
{
	if (d < 0)
	{
		kputch('-');
		d = -d;
	};

	char buffer[20];
	buffer[19] = 0;

	char *ptr = &buffer[18];
	do
	{
		if ((d % 16) < 10) *ptr = '0' + (d % 16);
		else *ptr = 'a' + ((d % 16) - 10);
		ptr--;
		d /= 16;
	} while (d != 0);

	kputs(ptr+1);
};

static void put_a(uint64_t addr)
{
	kputs("0x");
	int count = 16;
	while (count--)
	{
		uint8_t hexd = (uint8_t) (addr >> 60);
		if (hexd < 10)
		{
			kputch('0'+hexd);
		}
		else
		{
			kputch('A'+(hexd-10));
		};
		addr <<= 4;
	};

	updateVGACursor();
};

void kvprintf_gen(uint8_t putcon, const char *fmt, va_list ap)
{
	consoleState.putcon = putcon;

	while (*fmt != 0)
	{
		char c = *fmt++;
		if (c != '%')
		{
			kputch(c);
		}
		else
		{
			c = *fmt++;
			if (c == '%')
			{
				kputch('%');
			}
			else if (c == 's')
			{
				const char *str = va_arg(ap, char*);
				kputs(str);
			}
			else if (c == 'd')
			{
				int d = va_arg(ap, int);
				put_d(d);
			}
			else if (c == 'x')
			{
				int d = va_arg(ap, int);
				put_x(d);
			}
			else if ((c == 'a') || (c == 'p'))			// 64-bit unsigned
			{
				uint64_t d = va_arg(ap, uint64_t);
				put_a(d);
			}
			else if (c == 'c')
			{
				char pc = (char) va_arg(ap, int);
				kputch(pc);
			}
			else if (c == '$')
			{
				c = *fmt++;
				consoleState.curColor = c;
			}
			else if (c == '#')
			{
				consoleState.curColor = 0x07;
			};
		};
	};

	updateVGACursor();
};

void kvprintf(const char *fmt, va_list ap)
{
	kvprintf_gen(1, fmt, ap);
};

void kprintf(const char *fmt, ...)
{	
	va_list ap;
	va_start(ap, fmt);
	kvprintf(fmt, ap);
	va_end(ap);
};

void read_sectors(uint64_t lba, uint8_t count, void *buffer)
{
	DAP dap;
	dap.size = 16;
	dap.zero = 0;
	dap.numSectors = count;
	dap.offset = (uint16_t) ((uint32_t) buffer & 0xF);
	dap.segment = (uint16_t) ((uint32_t) buffer >> 4);
	dap.lba = lba;
	
	Regs regs;
	regs.h.ah = 0x42;
	regs.h.dl = bootDrive;
	regs.x.si = (uint16_t) (uint32_t) &dap;
	bios_call(0x13, &regs);
};

void read_disk(uint64_t pos, uint64_t size, void *buffer)
{
	char *put = (char*) buffer;
	while (size > 0)
	{
		uint64_t block = (pos >> 9) & ~15UL;
		if (block != currentBlock)
		{
			read_sectors(block+startLBA, 16, iobuf);
			currentBlock = block;
		};
		
		uint32_t offset = (uint32_t) (pos & 0x1FFF);
		
		uint64_t willRead = 0x2000 - offset;
		if (willRead > size) willRead = size;
		
		memcpy(put, &iobuf[offset], (uint32_t) willRead);
		size -= willRead;
		pos += willRead;
		put += willRead;
	};
};

void *alloc(uint32_t size)
{
	uint32_t ret = currentPlace;
	currentPlace += size;
	return (void*) ret;
};

void read_gxfs_block(char *buffer, uint64_t block)
{
	uint64_t section = block / cis.cisBlocksPerSection;
	uint64_t blockInSection = block % cis.cisBlocksPerSection;
	
	uint64_t blockPos = sections[section].sdOffTabBlocks + cis.cisBlockSize * blockInSection;
	read_disk(blockPos, cis.cisBlockSize, buffer);
};

void read_frags(char *buffer, gxfsFragment *frags, size_t count)
{
	size_t i;
	for (i=0; i<count; i++)
	{
		char *put = &buffer[frags[i].fOff];
		
		size_t j;
		for (j=0; j<frags[i].fExtent; j++)
		{
			uint64_t block = frags[i].fBlock + j;
			read_gxfs_block(put, block);
			put += cis.cisBlockSize;
		};
	};
};

/**
 * Read an inode into memory. This allocates a buffer large enough to hold the
 * data, and returns the size in *size. The pointer to the inode data is returned.
 */
void* read_inode(uint64_t ino, size_t *size)
{
	uint64_t section = (ino-1) / cis.cisInoPerSection;
	uint64_t inodeInSection = (ino-1) % cis.cisInoPerSection;
	
	uint64_t inodePos = sections[section].sdOffTabIno + sizeof(gxfsInode) * inodeInSection;
	
	gxfsInode inode;
	read_disk(inodePos, sizeof(gxfsInode), &inode);
	*size = (size_t) inode.inoSize;
	char *buffer = (char*) alloc(*size);
	
	read_frags(buffer, inode.inoFrags, 14);
	
	char *xftBuffer = (char*) __builtin_alloca(cis.cisBlockSize);
	
	uint64_t xft = inode.inoExFrag;
	while (xft != 0)
	{
		read_gxfs_block(xftBuffer, xft);
		gxfsXFT *table = (gxfsXFT*) xftBuffer;
		read_frags(buffer, table->xftFrags, table->xftCount);
		xft = table->xftNext;
	};
	
	return buffer;
};

size_t strlen(const char *str)
{
	size_t sz = 0;
	while (*str++ != 0) sz++;
	return sz;
};

/**
 * Scan through a directory to find an entry with the given name. Returns its inode number on
 * success, 0 on error.
 */
uint64_t find_entry(void *dir, const char *name)
{
	size_t namesz = strlen(name);
	
	gxfsDirHeader *head = (gxfsDirHeader*) dir;
	uint8_t nextEntrySize = head->dhFirstSz;
	dir = &head[1];
	
	uint32_t i;
	for (i=0; i<head->dhCount; i++)
	{
		gxfsDirent *ent = (gxfsDirent*) dir;
		if (ent->deInode != 0)
		{
			if (ent->deNameLen == namesz)
			{
				if (memcmp(ent->deName, name, namesz) == 0)
				{
					return ent->deInode;
				};
			};
		};
		
		char *next = (char*) dir + nextEntrySize;
		nextEntrySize = ent->deNextSz;
		dir = next;
	};
	
	return 0;
};

typedef struct
{
	uint64_t		baseAddr;
	uint64_t		len;
	uint32_t		type;
	uint32_t		acpiStuff;
} __attribute__ ((packed)) BiosMap;

MultibootMemoryMap* make_memory_map()
{
	MultibootMemoryMap *ret = (MultibootMemoryMap*) alloc(0);
	
	BiosMap biosmap;
	Regs regs;
	regs.e.ebx = 0;
	regs.e.edx = 0x534D4150;
	regs.x.es = 0;
	regs.x.di = (uint16_t) (uint32_t) &biosmap;
	
	while (1)
	{
		regs.e.eax = 0xE820;
		regs.e.ecx = 24;
		bios_call(0x15, &regs);
		
		if (regs.x.flags & 1)
		{
			// carry flag set
			break;
		};
		
		if (regs.e.ebx == 0)
		{
			break;
		};
		
		MultibootMemoryMap *mmap = (MultibootMemoryMap*) alloc(24);
		mmap->size = 20;		// has to be 4 bytes less than the structure for some reason
		mmap->baseAddr = biosmap.baseAddr;
		mmap->len = biosmap.len;
		mmap->type = biosmap.type;
	};
	
	return ret;
};

void jump_to_kernel(MultibootInfo *info, uint32_t kernelEntry);
void entry32(uint32_t startLBA32, uint32_t bootDrive32)
{
	startLBA = (uint64_t) startLBA32;
	initConsole();

	DEBUG("Glidix Loader 1.0\n");
	DEBUG("CIS located at offset: %a\n", gxfsBootHeader.cisOffset);
	DEBUG("Boot drive: %a, Partition start LBA: %a\n", (uint64_t) bootDrive32, (uint64_t) startLBA32);

	bootDrive = (uint8_t) bootDrive32;
	iobuf = alloc(0x2000);

	// check if INT 13H extensions are present
	Regs regs;
	regs.h.ah = 0x41;
	regs.h.dl = bootDrive;
	regs.x.bx = 0x55AA;
	bios_call(0x13, &regs);
	
	if (regs.x.bx != 0xAA55)
	{
		kprintf("ERROR: INT 13H Extensions not present!");
		return;
	};
	
	DEBUG("Reading CIS...\n");
	read_disk(gxfsBootHeader.cisOffset, sizeof(gxfsCIS), &cis);
	
	if (memcmp(cis.cisMagic, "GXFS", 4) != 0)
	{
		kprintf("ERROR: The CIS has an invalid magic.\n");
		return;
	};
	
	uint64_t numSections = cis.cisTotalIno / cis.cisInoPerSection;
	if (numSections != cis.cisTotalBlocks / cis.cisBlocksPerSection)
	{
		kprintf("ERROR: Section count mismatch.");
		return;
	};
	
	DEBUG("Number of sections: %d\n", (int) numSections);
	gxfsNumSections = (int) numSections;
	
	DEBUG("Reading section definitions...\n");
	sections = (gxfsSD*) alloc(sizeof(gxfsSD) * gxfsNumSections);
	read_disk(cis.cisOffSections, sizeof(gxfsSD) * gxfsNumSections, sections);
	
	DEBUG("Blocks per section: %d\n", (int) cis.cisBlocksPerSection);
	DEBUG("Inodes per section: %d\n", (int) cis.cisInoPerSection);
	
	DEBUG("Looking for /boot...\n");
	size_t size;
	void *rootDir = read_inode(2, &size);
	
	uint64_t bootInode = find_entry(rootDir, "boot");
	if (bootInode == 0)
	{
		kprintf("ERROR: Cannot find /boot!\n");
		return;
	};
	
	DEBUG("Loading /boot (inode %d)...\n", (int) bootInode);
	void *bootDir = read_inode(bootInode, &size);
	
	uint64_t inodeKernel = find_entry(bootDir, "vmglidix");
	uint64_t inodeRamdisk = find_entry(bootDir, "vmglidix.tar");
	
	if (inodeKernel == 0)
	{
		kprintf("ERROR: Cannot find /boot/vmglidix!");
		return;
	};
	
	if (inodeRamdisk == 0)
	{
		kprintf("ERROR: Cannot find /boot/vmglidix.tar!");
		return;
	};
	
	DEBUG("Kernel inode: %d\n", (int) inodeKernel);
	DEBUG("Ramdisk inode: %d\n", (int) inodeRamdisk);
	
	// load the kernel at the 1MB mark
	currentPlace = 0x100000;
	read_inode(inodeKernel, &size);
	
	MultibootHeader *header = (MultibootHeader*) 0x100000;
	if (header->magic != 0x1BADB002)
	{
		DEBUG("ERROR: The kernel has invalid multiboot header magic value\n");
		return;
	};
	
	memset((void*) header->bss, 0, header->end - header->bss);
	
	currentPlace = (uint32_t) header->end;
	
	/**
	 * We only report 1MB of lower memory and 1MB of upper memory.
	 * The Glidix kernel doesn't care; it uses the memory map to
	 * find the real maount of usable memory.
	 */
	MultibootInfo *info = (MultibootInfo*) alloc(sizeof(MultibootInfo));
	info->flags = (1 << 0) | (1 << 1) | (1 << 3) | (1 << 6);
	info->memLower = 1024;
	info->memUpper = 1024;
	info->bootDevice = bootDrive;
	info->cmdLine = 0;
	info->modsCount = 1;
	info->mmapAddr = (uint32_t) make_memory_map();
	info->mmapLen = (uint32_t) alloc(0) - info->mmapAddr;
	
	MultibootModule *mod = (MultibootModule*) alloc(sizeof(MultibootModule));
	info->modsAddr = (uint32_t) mod;
	
	mod->modStart = (uint32_t) read_inode(inodeRamdisk, &size);
	mod->modEnd = (uint32_t) alloc(0);
	mod->str = 0;
	mod->zero = 0;
	
	DEBUG("Loading finished at %a\n", (uint64_t) alloc(0));
	DEBUG("Booting kernel (%a)...\n", (uint64_t) header->entry);
	jump_to_kernel(info, header->entry);
};
