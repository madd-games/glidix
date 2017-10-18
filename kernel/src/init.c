/*
	Glidix kernel

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
#include <glidix/fpu.h>
#include <glidix/apic.h>
#include <glidix/acpi.h>
#include <glidix/netif.h>
#include <glidix/ipreasm.h>
#include <glidix/ethernet.h>
#include <glidix/dma.h>
#include <glidix/cpu.h>
#include <glidix/ptty.h>
#include <glidix/ramfs.h>
#include <glidix/usb.h>
#include <glidix/pageinfo.h>
#include <glidix/ftree.h>
#include <glidix/msr.h>

#define ACPI_OSC_QUERY_INDEX				0
#define ACPI_OSC_SUPPORT_INDEX				1
#define ACPI_OSC_CONTROL_INDEX				2

#define ACPI_OSC_QUERY_ENABLE				0x1

#define ACPI_OSC_SUPPORT_SB_PR3_SUPPORT			0x4
#define ACPI_OSC_SUPPORT_SB_APEI_SUPPORT		0x10

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

#define	PLACEMENT_HEAP_SIZE		(1 * 1024 * 1024)

char placeHeap[PLACEMENT_HEAP_SIZE];
SECTION(".lowmem") char lowmem[1*1024*1024];		/* see linker script as to why */

KernelBootInfo *bootInfo;

void tssPrepare();
void _syscall_entry();
void kmain(KernelBootInfo *info)
{
	bootInfo = info;
	kernelStatus = KERNEL_RUNNING;
	initConsole();
	kprintf("Successfully booted into 64-bit mode\n");
	
	kprintf_debug(" *** TO TRAP THE KERNEL *** \n");
	kprintf_debug(" set r15=rip\n");
	kprintf_debug(" set rip=0x%p\n", &trapKernel);
	kprintf_debug(" *** END OF INFO *** \n");

	// make sure that we were given the boto ID (an old bootloader might not do it)
	// this is required to detect the boot device
	if ((info->features & KB_FEATURE_BOOTID) == 0)
	{
		panic("the boot ID has not been passed by the bootloader");
	};
	
	kprintf("Initializing the IDT... ");
	initIDT();
	DONE();
	
	uint64_t mmapAddr = (uint64_t) info->mmap;
	uint64_t mmapEnd = mmapAddr + info->mmapSize;
	kprintf("Memory map address: 0x%016lX memory map size = %d\n", mmapAddr, info->mmapSize);
	MultibootMemoryMap *mmap = (MultibootMemoryMap*) mmapAddr;
	kprintf("Size\tBase\tLen\tType\n");
	while ((uint64_t)mmap < mmapEnd)
	{
		kprintf("%u\t0x%016lX\t%lu\t%d\n", mmap->size, mmap->baseAddr, mmap->len, mmap->type);
		mmap = (MultibootMemoryMap*) ((uint64_t) mmap + mmap->size + 4);
	};

	kprintf("Checking amount of memory... ");
	uint64_t memSize = 0;
	mmap = (MultibootMemoryMap*) mmapAddr;
	while ((uint64_t)mmap < mmapEnd)
	{
		uint64_t end = mmap->baseAddr + mmap->len;
		if (end > memSize) memSize = end;
		mmap = (MultibootMemoryMap*) ((uint64_t) mmap + mmap->size + 4);
	};
	kprintf("%dMB\n", (int)(memSize/1024/1024));
	
	kprintf("Initializing memory allocation phase 1 (base=0x%016lX)... ", (uint64_t)placeHeap);
	initMemoryPhase1((uint64_t)placeHeap, PLACEMENT_HEAP_SIZE);
	DONE();

	kprintf("Initializing the physical memory manager (%d pages)... ", (int)(memSize/0x1000));
	initPhysMem(memSize/0x1000, (MultibootMemoryMap*) mmapAddr, mmapEnd, info->end);
	DONE();

	kprintf("Initializing the ISP... ");
	ispInit();
	DONE();
	
	//kprintf("Initializing per-CPU variable area... ");
	//initPerCPU();
	//DONE();
	
	localTSSPtr = &_tss;
	localGDTPtr = &GDT64;
	
	kprintf("Initializing memory allocation phase 2... ");
	initMemoryPhase2();
	DONE();

	kprintf("Initializing the frame bitmap... ");
	initPhysMem2();
	DONE();

	kprintf("Initializing DMA... ");
	dmaInit();
	DONE();

	kprintf("Initializing pageinfo... ");
	piInit();
	DONE();
	
	initModuleInterface();

	kprintf("Getting ACPI info... ");
	acpiInit();
	msrWrite(0x1B, 0xFEE00000 | (1 << 11));
	apic->sivr = 0x1FF;

	kprintf("Initializing the FPU... ");
	fpuInit();
	DONE();

	kprintf("Initializing the VFS... ");
	vfsInit();
	DONE();

	kprintf("Initializing the initrdfs... ");
	initInitrdfs(info);
	DONE();

	kprintf("Initializing the procfs... ");
	initProcfs();
	DONE();

	kprintf("Initializing the devfs... ");
	initDevfs();
	DONE();

	kprintf("Initializing PCI... ");
	pciInit();
	DONE();

	kprintf("Initializing USB...");
	usbInit();
	DONE();
	
	kprintf("Initializing the FS driver interface... ");
	initFSDrivers();
	DONE();

	kprintf("Initializing the PIT... ");
	uint16_t divisor = 1193180 / 1000;		// 1000 Hz
	outb(0x43, 0x36);
	uint8_t l = (uint8_t)(divisor & 0xFF);
	uint8_t h = (uint8_t)( (divisor>>8) & 0xFF );
	outb(0x40, l);
	outb(0x40, h);
	DONE();
	
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
	initPerCPU2();
	initSched();
	// "Done" will be displayed by initSched(), and then kmain2() will be called.
};

static void spawnProc(void *stack)
{
	DONE();

	kprintf("Setting up the terminal... ");
	setupTerminal(getCurrentThread()->ftab);
	DONE();

	tssPrepare();
	
	getCurrentThread()->creds->sid = 1;
	getCurrentThread()->creds->pgid = 1;
	
	static char initExecpars[] = "init\0\0USERNAME=root\0SHELL=/bin/sh\0\0";
	kyield();
	kprintf("Executing /initrd/init...\n");
	elfExec("/initrd/init", initExecpars, sizeof(initExecpars));
	panic("failed to execute /initrd/init");
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
	signalPidEx(1, &info, SP_NOPERM);
	return 0;
};

static const UINT8 uuidOffset[16] =
{
    6,4,2,0,11,9,16,14,19,21,24,26,28,30,32,34
};
static UINT8 hex2num(char hex)
{
	if ((hex >= 'A') && (hex <= 'F'))
	{
		return (UINT8) (hex-'A');
	}
	else
	{
		return (UINT8) (hex-'0');
	};
};
static void str2uuid(char *InString, UINT8 *UuidBuffer)
{
	UINT32                  i;

	for (i = 0; i < UUID_BUFFER_LENGTH; i++)
	{
		UuidBuffer[i] = (hex2num (
			InString[uuidOffset[i]]) << 4);

		UuidBuffer[i] |= hex2num (
			InString[uuidOffset[i] + 1]);
	};
};

void kmain2()
{	
	initMount();
	initSymtab();
	ramfsInit();
	
	kprintf("Initializing ACPICA...\n");
	
	ACPI_STATUS status = AcpiInitializeSubsystem();
	if (ACPI_FAILURE(status))
	{
		panic("AcpiInitializeSubsystem failed");
	};

	AcpiInstallInterface("Windows 2009");
	
	status = AcpiReallocateRootTable();
	if (ACPI_FAILURE(status))
	{
		panic("AcpiReallocateRootTable failed\n");
	};
	
	status = AcpiInstallAddressSpaceHandler(ACPI_ROOT_OBJECT, ACPI_ADR_SPACE_SYSTEM_MEMORY, ACPI_DEFAULT_HANDLER, NULL, NULL);
	if (ACPI_FAILURE(status))
	{
		panic("AcpiInstallAddressSpaceHandler failed");
	};

	status = AcpiInstallAddressSpaceHandler(ACPI_ROOT_OBJECT, ACPI_ADR_SPACE_SYSTEM_IO, ACPI_DEFAULT_HANDLER, NULL, NULL);
	if (ACPI_FAILURE(status))
	{
		panic("AcpiInstallAddressSpaceHandler failed");
	};

	status = AcpiInstallAddressSpaceHandler(ACPI_ROOT_OBJECT, ACPI_ADR_SPACE_PCI_CONFIG, ACPI_DEFAULT_HANDLER, NULL, NULL);
	if (ACPI_FAILURE(status))
	{
		panic("AcpiInstallAddressSpaceHandler failed");
	};

	initSched2();
	
	// this must come after AcpiInitializeSubsystem() because ACPI calls
	// AcpiOsInitialize() which maps more stuff into the PML4
	initMultiProc();
	
	kprintf("Initializing SDI... ");
	sdInit();
	DONE();
	
	kprintf("Initializing file tree subsystem... ");
	ftInit();
	DONE();
	
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

	uint32_t capabilities[2];
	capabilities[ACPI_OSC_QUERY_INDEX] = ACPI_OSC_QUERY_ENABLE;
	capabilities[ACPI_OSC_SUPPORT_INDEX] = ACPI_OSC_SUPPORT_SB_PR3_SUPPORT;
	
	ACPI_OBJECT_LIST input;
	ACPI_OBJECT inParams[4];
	uint8_t uuid[16];
	ACPI_BUFFER output;
	
	str2uuid("0811B06E-4A27-44F9-8D60-3CBBC22E7B48", uuid);
	output.Length = ACPI_ALLOCATE_BUFFER;
	output.Pointer = NULL;
	
	input.Count = 4;
	input.Pointer = inParams;

	inParams[0].Type = ACPI_TYPE_BUFFER;
	inParams[0].Buffer.Length = 16;
	inParams[0].Buffer.Pointer = uuid;

	inParams[1].Type = ACPI_TYPE_INTEGER;
	inParams[1].Integer.Value = 1;

	inParams[2].Type = ACPI_TYPE_INTEGER;
	inParams[2].Integer.Value = 2;

	inParams[3].Type = ACPI_TYPE_BUFFER;
	inParams[3].Buffer.Length = 8;
	inParams[3].Buffer.Pointer = (UINT8*) capabilities;

	ACPI_HANDLE rootHandle;
	if (ACPI_FAILURE(AcpiGetHandle(NULL, "\\_SB", &rootHandle)))
	{
		panic("Failed to get \\_SB object!");
	};
	
	status = AcpiEvaluateObject(rootHandle, "_OSC", &input, &output);
	
	// ---
	
	ACPI_OBJECT arg1;
	arg1.Type = ACPI_TYPE_INTEGER;
	arg1.Integer.Value = 1;		/* IOAPIC */
	
	ACPI_OBJECT_LIST args;
	args.Count = 1;
	args.Pointer = &arg1;
	
	AcpiEvaluateObject(ACPI_ROOT_OBJECT, "_PIC", &args, NULL);
	
	kprintf("Installing power button handler...\n");
	if (AcpiInstallFixedEventHandler(ACPI_EVENT_POWER_BUTTON, onPowerButton, NULL) != AE_OK)
	{
		panic("failed to register power button event");
	};
	
	kprintf("Enumerating PCI with ACPI...\n");
	pciInitACPI();
	
	kprintf("Initializing the RTC... ");
	initRTC();
	DONE();
	
	kprintf("Initializing the network interface... ");
	ipreasmInit();
	initNetIf();
	DONE();

	kprintf("Initializing the ptty interface... ");
	pttyInit();
	DONE();
	
	kprintf("Starting the spawn process... ");
	MachineState state;
	memset(&state.fpuRegs, 0, 512);
	void *spawnStack = kmalloc(0x200000);
	state.fsbase = 0;
	state.gsbase = 0;
	state.rip = (uint64_t) &spawnProc;
	state.rsp = (uint64_t) spawnStack + 0x200000 - 16 - 8;
	((uint64_t*)state.rsp)[0] = 0;
	((uint64_t*)state.rsp)[1] = 0;
	state.rdi = (uint64_t) spawnStack;
	state.rbp = 0;
	Regs regs;
	memset(&regs, 0, sizeof(Regs));
	regs.cs = 8;
	regs.ds = 16;
	regs.rflags = getFlagsRegister() | (1 << 9);
	regs.ss = 0;
	threadClone(&regs, 0, &state);
	// "Done" is displayed by the spawnProc() and that's our job done pretty much.
	// Mark this thread as waiting so that it never wastes any CPU time.
	getCurrentThread()->flags = THREAD_WAITING;
};
