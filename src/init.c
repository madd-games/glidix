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

#include <glidix/console.h>
#include <glidix/common.h>
#include <glidix/idt.h>
#include <glidix/physmem.h>
#include <glidix/memory.h>
#include <glidix/pagetab.h>
#include <glidix/isp.h>
#include <glidix/string.h>
#include <glidix/port.h>
#include <glidix/sched.h>
#include <glidix/mount.h>
#include <glidix/initrdfs.h>
#include <glidix/procfs.h>
#include <glidix/procmem.h>
#include <glidix/vfs.h>
#include <glidix/ktty.h>
#include <glidix/symtab.h>
#include <glidix/module.h>
#include <glidix/devfs.h>
#include <glidix/pci.h>
#include <glidix/storage.h>
#include <glidix/fsdriver.h>
#include <glidix/time.h>
#include <glidix/interp.h>
#include <glidix/fpu.h>
#include <glidix/apic.h>
#include <glidix/acpi.h>
#include <glidix/netif.h>
#include <glidix/ipreasm.h>
#include <glidix/ethernet.h>
#include <glidix/dma.h>
#include <glidix/shmem.h>
#include <glidix/cpu.h>
#include <glidix/ptty.h>

extern int _bootstrap_stack;
extern int end;
extern uint32_t quantumTicks;
KernelStatus kernelStatus;

void expandHeap();
extern int masterHeader;
void trapKernel();

int strncmp(const char *s1, const char *s2, size_t n)
{
	for ( ; n > 0; s1++, s2++, --n)
		if (*s1 != *s2)
			return ((*(unsigned char *)s1 < *(unsigned char *)s2) ? -1 : +1);
		else if (*s1 == '\0')
			return 0;
	return 0;
};

int isprint(int c)
{
	return ((c >= ' ' && c <= '~') ? 1 : 0);
};

char* strncpy(char *s1, const char *s2, size_t n)
{
	char *s = s1;
	while (n > 0 && *s2 != '\0') {
		*s++ = *s2++;
		--n;
	}
	while (n > 0) {
		*s++ = '\0';
		--n;
	}
	return s1;
};

int isdigit(int c)
{
	return((c>='0') && (c<='9'));
};

int isspace(int c)
{
	return (c == ' ' || c == '\t' || c == '\n' || c == '\12');
};

int isxdigit(int c)
{
	return isdigit(c) || ((c >= 'a') && (c <= 'f')) || ((c >= 'A') && (c <= 'F'));
};

int isupper(int c)
{
	return (c >= 'A') && (c <= 'Z');
};

int islower(int c)
{
	return (c >= 'a') && (c <= 'z');
};

int toupper(int c)
{
	if ((c >= 'a') && (c <= 'z'))
	{
		return c-'a'+'A';
	};
	
	return c;
};

int tolower(int c)
{
	if ((c >= 'A') && (c <= 'Z'))
	{
		return c-'A'+'a';
	};
	
	return c;
};

char * strncat(char *dst, const char *src, size_t n)
{
	if (n != 0) {
		char *d = dst;
		register const char *s = src;

		while (*d != 0)
			d++;
		do {
			if ((*d = *s++) == 0)
				break;
			d++;
		} while (--n != 0);
		*d = 0;
	}
	return (dst);
};

int isalpha(int c)
{
	return((c >='a' && c <='z') || (c >='A' && c <='Z'));
};

unsigned long strtoul(const char *nptr, char **endptr, int base)
{
	register const char *s = nptr;
	register unsigned long acc;
	register int c;
	register unsigned long cutoff;
	register int neg = 0, any, cutlim;

	/*
	 * See strtol for comments as to the logic used.
	 */
	do {
		c = *s++;
	} while (isspace(c));
	if (c == '-') {
		neg = 1;
		c = *s++;
	} else if (c == '+')
		c = *s++;
	if ((base == 0 || base == 16) &&
	    c == '0' && (*s == 'x' || *s == 'X')) {
		c = s[1];
		s += 2;
		base = 16;
	} else if ((base == 0 || base == 2) &&
	    c == '0' && (*s == 'b' || *s == 'B')) {
		c = s[1];
		s += 2;
		base = 2;
	}
	if (base == 0)
		base = c == '0' ? 8 : 10;
	cutoff = (unsigned long)ULONG_MAX / (unsigned long)base;
	cutlim = (unsigned long)ULONG_MAX % (unsigned long)base;
	for (acc = 0, any = 0;; c = *s++) {
		if (isdigit(c))
			c -= '0';
		else if (isalpha(c))
			c -= isupper(c) ? 'A' - 10 : 'a' - 10;
		else
			break;
		if (c >= base)
			break;
		if (any < 0 || acc > cutoff || (acc == cutoff && c > cutlim))
			any = -1;
		else {
			any = 1;
			acc *= base;
			acc += c;
		}
	}
	if (any < 0) {
		acc = ULONG_MAX;
	} else if (neg)
		acc = -acc;
	if (endptr != 0)
		*endptr = (char *)(any ? s - 1 : nptr);
	return (acc);
};

char *strstr(const char *in, const char *str)
{
    char c;
    size_t len;

    c = *str++;
    if (!c)
        return (char *) in;	// Trivial empty string case

    len = strlen(str);
    do {
        char sc;

        do {
            sc = *in++;
            if (!sc)
                return (char *) 0;
        } while (sc != c);
    } while (strncmp(in, str, len) != 0);

    return (char *) (in - 1);
};

extern char GDTPointer[10];
extern TSS _tss;
extern PER_CPU TSS* localTSSPtr;
extern char GDT64;
extern PER_CPU char* localGDTPtr;

void kmain(MultibootInfo *info)
{
	cli();
	kernelStatus = KERNEL_RUNNING;
	initConsole();
	kprintf("Successfully booted into 64-bit mode\n");
	if (info->modsCount != 1)
	{
		panic("the initrd was not loaded");
	};
	
	// map the last entry of the PML4 to itself.
	PML4 *pml4 = (PML4*) 0xFFFF800000001000;
	pml4->entries[511].present = 1;
	pml4->entries[511].pdptPhysAddr = 1;	// 0x1000
	pml4->entries[511].rw = 1;
	refreshAddrSpace();
	
	kprintf_debug(" *** TO TRAP THE KERNEL *** \n");
	kprintf_debug(" set r15=rip\n");
	kprintf_debug(" set rip=%a\n", &trapKernel);
	kprintf_debug(" *** END OF INFO *** \n");

	kprintf("Initializing the IDT... ");
	initIDT();
	kprintf("%$\x02" "Done%#\n");

	uint64_t mmapAddr = (uint64_t) info->mmapAddr + 0xFFFF800000000000;
	uint64_t mmapEnd = mmapAddr + info->mmapLen;
	kprintf("Memory map address: %a memory map size = %d\n", mmapAddr, info->mmapLen);
	MultibootMemoryMap *mmap = (MultibootMemoryMap*) mmapAddr;
	kprintf("Size\tBase\tLen\tType\n");
	while ((uint64_t)mmap < mmapEnd)
	{
		kprintf("%d\t%a\t%d\t%d\n", mmap->size, mmap->baseAddr, mmap->len, mmap->type);
		mmap = (MultibootMemoryMap*) ((uint64_t) mmap + mmap->size + 4);
	};

	MultibootModule *mod = (MultibootModule*) ((uint64_t) info->modsAddr + 0xFFFF800000000000);
	uint64_t end = (uint64_t) mod->modEnd + 0xFFFF800000000000;

	kprintf("Checking amount of memory... ");
	uint64_t memSize = 0;
	mmap = (MultibootMemoryMap*) mmapAddr;
	while ((uint64_t)mmap < mmapEnd)
	{
		uint64_t end = mmap->baseAddr + mmap->len;
		if (end > memSize) memSize = end;
		mmap = (MultibootMemoryMap*) ((uint64_t) mmap + mmap->size + 4);
	};
	kprintf("%$\x01%dMB%#\n", (int)(memSize/1024/1024));
	
	kprintf("Initializing memory allocation phase 1 (base=%a)... ", end);
	initMemoryPhase1(end);
	kprintf("%$\x02" "Done%#\n");

	kprintf("Initializing the physical memory manager (%d pages)... ", (int)(memSize/0x1000));
	initPhysMem(memSize/0x1000, (MultibootMemoryMap*) mmapAddr, mmapEnd);
	kprintf("%$\x02" "Done%#\n");

	kprintf("Initializing the ISP... ");
	ispInit();
	kprintf("%$\x02" "Done%#\n");

	kprintf("Initializing per-CPU variable area... ");
	initPerCPU();
	DONE();
	
	localTSSPtr = &_tss;
	localGDTPtr = &GDT64;
	
	kprintf("Initializing memory allocation phase 2... ");
	initMemoryPhase2();
	kprintf("%$\x02" "Done%#\n");

	kprintf("Initializing the frame bitmap... ");
	initPhysMem2();
	kprintf("%$\x02" "Done%#\n");

	kprintf("Initializing DMA... ");
	dmaInit();
	DONE();
	
	initModuleInterface();

	kprintf("Getting ACPI info... ");
	acpiInit();
	msrWrite(0x1B, 0xFEE00000 | (1 << 11) /*| (1 << 8)*/ );
	apic->sivr = 0x1FF;

	kprintf("Initializing the FPU... ");
	fpuInit();
	DONE();

	kprintf("Initializing the VFS... ");
	vfsInit();
	kprintf("%$\x02" "Done%#\n");

	kprintf("Initializing the initrdfs... ");
	initInitrdfs(info);
	kprintf("%$\x02" "Done%#\n");

	kprintf("Initializing the procfs... ");
	initProcfs();
	kprintf("%$\x02" "Done%#\n");

	kprintf("Initializing the devfs... ");
	initDevfs();
	kprintf("%$\x02" "Done%#\n");

	kprintf("Initializing PCI... ");
	pciInit();
	kprintf("%$\x02" "Done%#\n");

	kprintf("Initializing the FS driver interface... ");
	initFSDrivers();
	kprintf("%$\x02" "Done%#\n");

	kprintf("Initializing the PIT... ");
	uint16_t divisor = 1193180 / 1000;		// 1000 Hz
	outb(0x43, 0x36);
	uint8_t l = (uint8_t)(divisor & 0xFF);
	uint8_t h = (uint8_t)( (divisor>>8) & 0xFF );
	outb(0x40, l);
	outb(0x40, h);
	kprintf("%$\x02" "Done%#\n");

	kprintf("Initializing the APIC timer...");
	sti();
	apic->timerDivide = 3;
	apic->timerInitCount = 0xFFFFFFFF;
	sleep(35);
	apic->lvtTimer = 0;
	quantumTicks = 0xFFFFFFFF - apic->timerCurrentCount;
	apic->timerInitCount = 0;
	// put the timer in single-shot mode at the appropriate interrupt vector.
	apic->lvtTimer = I_APIC_TIMER;
	DONE();

	kprintf("Initializing the scheduler and syscalls... ");
	initSched();
	// "Done" will be displayed by initSched(), and then kmain2() will be called.
};

extern void _jmp_usbs(void *stack);
static void spawnProc(void *stack)
{
	kprintf("%$\x02" "Done%#\n");

	initInterp();

	kprintf("Allocating memory for bootstrap... ");
	FrameList *fl = palloc(2);
	AddSegment(getCurrentThread()->pm, 1, fl, PROT_READ | PROT_WRITE | PROT_EXEC);
	pdownref(fl);
	SetProcessMemory(getCurrentThread()->pm);
	kprintf("%$\x02" "Done%#\n");

	kprintf("Setting up the terminal... ");
	setupTerminal(getCurrentThread()->ftab);
	kprintf("%$\x02" "Done%#\n");

	kprintf("Loading /initrd/usbs... ");
	int err;
	File *file = vfsOpen("/initrd/usbs", VFS_CHECK_ACCESS, &err);
	if (file == NULL)
	{
		kprintf("%$\x04" "Failed%#\n");
		panic("failed to open /initrd/usbs");
	};
	ssize_t count = vfsRead(file, (void*) 0x1000, 0x1000);
	if (count < 1)
	{
		kprintf("%$\x04" "Failed%#\n");
		panic("read() /initrd/usbs: %d\n", count);
	};
	vfsClose(file);
	kprintf("%$\x02" "%d bytes%#\n", count);

	getCurrentThread()->creds->sid = 1;
	getCurrentThread()->creds->pgid = 1;
	
	kprintf("Control will be transferred to usbs now.\n");
	_jmp_usbs(stack);
};

extern uint64_t getFlagsRegister();

static UINT32 onPowerButton(void *ignore)
{	
	siginfo_t info;
	info.si_signo = SIGHUP;
	info.si_code = 0;
	info.si_errno = 0;
	info.si_pid = 0;
	info.si_uid = 0;
	info.si_addr = NULL;
	info.si_status = 0;
	info.si_band = 0;
	info.si_value.sival_int = 0;
	signalPidEx(1, &info);
	return 0;
};

void kmain2()
{
	initMount();
	initSymtab();

	kprintf("Initializing ACPICA...\n");
	ACPI_STATUS status = AcpiInitializeSubsystem();
	if (ACPI_FAILURE(status))
	{
		panic("AcpiInitializeSubsystem failed");
	};
	
	// this must come after AcpiInitializeSubsystem() because ACPI calls
	// AcpiOsInitialize() which maps more stuff into the PML4
	initMultiProc();

	initSched2();
	
	kprintf("Initializing SDI... ");
	sdInit();
	kprintf("%$\x02" "Done%#\n");
	
	status = AcpiInitializeTables(NULL, 16, FALSE);
	if (ACPI_FAILURE(status))
	{
		panic("AcpiInitializeTables failed");
	};
	
	kprintf("Loading ACPI tables...\n");
	status = AcpiLoadTables();
	if (ACPI_FAILURE(status))
	{
		panic("AcpiLoadTables failed");
	};
	
	kprintf("Initializing all ACPI subsystems...\n");
	status = AcpiEnableSubsystem(ACPI_FULL_INITIALIZATION);
	if (ACPI_FAILURE(status))
	{
		panic("AcpiEnableSubsystem failed");
	};
	
	kprintf("Initializing ACPI objects...\n");
	status = AcpiInitializeObjects(ACPI_FULL_INITIALIZATION);
	if (ACPI_FAILURE(status))
	{
		panic("AcpiInitializeObjects failed");
	};
	
	kprintf("Installing power button handler...\n");
	if (AcpiInstallFixedEventHandler(ACPI_EVENT_POWER_BUTTON, onPowerButton, NULL) != AE_OK)
	{
		panic("failed to register power button event");
	};
	
	kprintf("Enumerating PCI with ACPI...\n");
	pciInitACPI();
	
	kprintf("Initializing the RTC... ");
	initRTC();
	kprintf("%$\x02" "Done%#\n");
	
	kprintf("Initializing the network interface... ");
	ipreasmInit();
	initNetIf();
	kprintf("%$\x02" "Done%#\n");
	
	kprintf("Initializing the shared memory API... ");
	shmemInit();
	DONE();

	kprintf("Initializing the ptty interface... ");
	pttyInit();
	DONE();
	
	kprintf("Starting the spawn process... ");
	MachineState state;
	memset(&state.fpuRegs, 0, 512);
	void *spawnStack = kmalloc(0x1000);
	state.rip = (uint64_t) &spawnProc;
	state.rsp = (uint64_t) spawnStack + 0x1000 - 16;
	((uint64_t*)state.rsp)[0] = 0;
	((uint64_t*)state.rsp)[1] = 0;
	state.rdi = (uint64_t) spawnStack;
	state.rbp = 0;
	Regs regs;
	regs.cs = 8;
	regs.ds = 16;
	regs.rflags = getFlagsRegister() | (1 << 9);
	regs.ss = 0;
	threadClone(&regs, 0, &state);
	// "Done" is displayed by the spawnProc() and that's our job done pretty much.
	// Mark this thread as waiting so that it never wastes any CPU time.
	getCurrentThread()->flags = THREAD_WAITING;
};
