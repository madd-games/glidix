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

#include <acpi/acpi.h>
#include <glidix/memory.h>
#include <glidix/console.h>
#include <glidix/time.h>
#include <glidix/port.h>
#include <glidix/isp.h>
#include <glidix/string.h>
#include <glidix/sched.h>
#include <glidix/common.h>
#include <glidix/pci.h>
#include <glidix/pagetab.h>
#include <glidix/physmem.h>
#include <glidix/idt.h>

/**
 * Implements the ACPICA Operating System Layer (OSL) functions.
 */
 
static PAGE_ALIGN PDPT pdptAcpi;
static uint64_t nextFreePage = 0;
static Spinlock acpiMemoryLock;

void* AcpiOsAllocate(ACPI_SIZE size)
{
	return kmalloc(size);
};

void AcpiOsFree(void *ptr)
{
	kfree(ptr);
};

void AcpiOsVprintf(const char *fmt, va_list ap)
{
	(void)fmt;
	//kvprintf(fmt, ap);
};

void AcpiOsPrintf(const char *fmt, ...)
{
	(void)fmt;
	va_list ap;
	va_start(ap, fmt);
	//kvprintf(fmt, ap);
	va_end(ap);
};

void AcpiOsSleep(UINT64 ms)
{
	sleep(ms);
};

ACPI_PHYSICAL_ADDRESS AcpiOsGetRootPointer()
{
	ACPI_SIZE Ret;
	AcpiFindRootPointer(&Ret);
	return Ret;
};

ACPI_STATUS AcpiOsWritePort(ACPI_IO_ADDRESS port, UINT32 value, UINT32 width)
{
	switch (width)
	{
	case 8:
		outb(port, (uint8_t) value);
		return AE_OK;
	case 16:
		outw(port, (uint16_t) value);
		return AE_OK;
	case 32:
		outd(port, (uint32_t) value);
		return AE_OK;
	default:
		return AE_ERROR;
	};
};

ACPI_STATUS AcpiOsReadPort(ACPI_IO_ADDRESS port, UINT32 *value, UINT32 width)
{
	*value = 0;
	switch (width)
	{
	case 8:
		*((uint8_t*)value) = inb(port);
		return AE_OK;
	case 16:
		*((uint16_t*)value) = inw(port);
		return AE_OK;
	case 32:
		*((uint32_t*)value) = ind(port);
		return AE_OK;
	default:
		return AE_ERROR;
	};
};

void AcpiOsStall(UINT32 ms)
{
	int then = getUptime() + ms;
	while (getUptime() < then);
};

ACPI_STATUS AcpiOsReadMemory(ACPI_PHYSICAL_ADDRESS addr, UINT64 *value, UINT32 width)
{
	*value = 0;
	size_t len = (size_t)width / 8;
	pmem_read(value, (uint64_t) addr, len);
	return AE_OK;
};

ACPI_STATUS AcpiOsWriteMemory(ACPI_PHYSICAL_ADDRESS addr, UINT64 value, UINT32 width)
{
	size_t len = (size_t)width / 8;
	pmem_write((uint64_t) addr, &value, len);
	return AE_OK;
};

void* AcpiOsAllocateZeroed(ACPI_SIZE size)
{
	void *ret = kmalloc(size);
	memset(ret, 0, size);
	return ret;
};

ACPI_STATUS AcpiOsCreateLock(ACPI_SPINLOCK *spinlock)
{
	/**
	 * And the kmalloc() will return a block of 16 bytes to store a single spinlock (one byte). It will
	 * also fragment the kernel heap. Perhaps we should have a function for such "microallocations"?
	 */
	*spinlock = (Spinlock*) kmalloc(sizeof(Spinlock));
	spinlockRelease(*spinlock);
	return AE_OK;
};

void AcpiOsDeleteLock(ACPI_SPINLOCK spinlock)
{
	kfree(spinlock);
};

ACPI_CPU_FLAGS AcpiOsAcquireLock(ACPI_SPINLOCK spinlock)
{
	spinlockAcquire(spinlock);
	return 0;
};

void AcpiOsReleaseLock(ACPI_SPINLOCK spinlock, ACPI_CPU_FLAGS flags)
{
	(void)flags;
	spinlockRelease(spinlock);
};

BOOLEAN AcpiOsReadable(void *mem, ACPI_SIZE size)
{
	// this should not actually be called
	return 0;
};

ACPI_THREAD_ID AcpiOsGetThreadId()
{
	return (ACPI_THREAD_ID) getCurrentThread();
};

UINT64 AcpiOsGetTimer()
{
	return getNanotime() / 100;
};

ACPI_STATUS AcpiOsCreateSemaphore(UINT32 maxUnits, UINT32 initUnits, ACPI_SEMAPHORE *semptr)
{
	(void)maxUnits;
	if (semptr == NULL)
	{
		return AE_BAD_PARAMETER;
	};
	
	*semptr = (ACPI_SEMAPHORE) kmalloc(sizeof(Semaphore));
	semInit2(*semptr, initUnits);
	return AE_OK;
};

ACPI_STATUS AcpiOsDeleteSemaphore(ACPI_SEMAPHORE sem)
{
	if (sem == NULL) return AE_BAD_PARAMETER;
	kfree(sem);
	return AE_OK;
};

/**
 * Stupidly, it's not like our average semWaitTimeout() - we can't return less resources than were requested.
 */
ACPI_STATUS AcpiOsWaitSemaphore(ACPI_SEMAPHORE sem, UINT32 units, UINT16 timeout)
{
	int count = (int) units;
	if (sem == NULL)
	{
		kprintf("AcpiOsWaitSemaphore called with NULL");
		while(1);
		return AE_BAD_PARAMETER;
	};
	
	if (units == 0)
	{
		return AE_OK;
	};
	
	if (timeout == 0)
	{
		int got = semWaitNoblock(sem, count);
		if (got < count)
		{
			semSignal2(sem, got);
			return AE_TIME;
		}
		else
		{
			return AE_OK;
		};
	};
	
	uint64_t nanoTimeout = (uint64_t) timeout * 1000000;
	if (timeout == 0xFFFF)
	{
		nanoTimeout = 0;
	};
	
	uint64_t deadline = getNanotime() + nanoTimeout;
	int acquiredSoFar = 0;
	while (1)
	{
		int got = semWaitTimeout(sem, count, nanoTimeout);
		if (got < 0)
		{
			semSignal2(sem, acquiredSoFar);
			return AE_TIME;
		};
		
		acquiredSoFar += got;
		count -= got;
		
		if (count == 0) break;
		
		uint64_t now = getNanotime();
		if (now < deadline)
		{
			nanoTimeout = deadline - now;
		}
		else
		{
			semSignal2(sem, acquiredSoFar);
			return AE_TIME;
		};
	};
	
	return AE_OK;
};

ACPI_STATUS AcpiOsSignalSemaphore(ACPI_SEMAPHORE sem, UINT32 units)
{
	if (sem == NULL)
	{
		return AE_BAD_PARAMETER;
	};
	
	if (units == 0)
	{
		return AE_OK;
	};
	
	semSignal2(sem, (int) units);
	return AE_OK;
};

ACPI_STATUS AcpiOsGetLine(char *buffer, UINT32 len, UINT32 *read)
{
	*read = 0;
	return AE_OK;
};

ACPI_STATUS AcpiOsSignal(UINT32 func, void *info)
{
	if (func == ACPI_SIGNAL_FATAL)
	{
		ACPI_SIGNAL_FATAL_INFO *fin = (ACPI_SIGNAL_FATAL_INFO*) info;
		panic("ACPI fatal: type %d, code %d, arg %d", fin->Type, fin->Code, fin->Argument);
	}
	else
	{
		kprintf("ACPI breakpoint: %s\n", (char*)info);
	};
	
	return AE_OK;
};

ACPI_STATUS AcpiOsExecute(ACPI_EXECUTE_TYPE type, ACPI_OSD_EXEC_CALLBACK func, void *ctx)
{
	// coincidently, ACPICA uses the same type of callback function as CreateKernelThread
	(void)type;
	if (func == NULL) return AE_BAD_PARAMETER;
	KernelThreadParams pars;
	memset(&pars, 0, sizeof(KernelThreadParams));
	pars.stackSize = DEFAULT_STACK_SIZE;
	pars.name = "ACPICA thread";
	CreateKernelThread(func, &pars, ctx);
	return AE_OK;
};

ACPI_STATUS AcpiOsInitialize()
{
	memset(&pdptAcpi, 0, sizeof(PDPT));
	pdptAcpi.entries[511].present = 1;
	pdptAcpi.entries[511].rw = 1;
	pdptAcpi.entries[511].pdPhysAddr = ((uint64_t)&pdptAcpi - 0xFFFF800000000000) >> 12;
	PML4 *pml4 = getPML4();
	pml4->entries[262].present = 1;
	pml4->entries[262].rw = 1;
	pml4->entries[262].pdptPhysAddr = ((uint64_t)&pdptAcpi - 0xFFFF800000000000) >> 12;
	refreshAddrSpace();
	spinlockRelease(&acpiMemoryLock);
	return AE_OK;
};

ACPI_STATUS AcpiOsTerminate()
{
	return AE_OK;
};

ACPI_STATUS AcpiOsPredefinedOverride(const ACPI_PREDEFINED_NAMES *PredefinedObject, ACPI_STRING *NewValue)
{
	*NewValue = NULL;
	return AE_OK;
};

ACPI_STATUS AcpiOsTableOverride(ACPI_TABLE_HEADER *ExistingTable, ACPI_TABLE_HEADER **NewTable)
{
	*NewTable = NULL;
	return AE_OK;
};

void AcpiOsWaitEventsComplete()
{
};

ACPI_STATUS AcpiOsPhysicalTableOverride(ACPI_TABLE_HEADER *ExistingTable, ACPI_PHYSICAL_ADDRESS *NewAddress, UINT32 *NewTableLength)
{
	*NewAddress = 0;
	return AE_OK;
};

ACPI_STATUS AcpiOsReadPciConfiguration(ACPI_PCI_ID *id, UINT32 reg, UINT64 *value, UINT32 width)
{
	uint32_t regAligned = reg & ~3;
	uint32_t offsetIntoReg = reg & 3;
	uint32_t addr = (id->Bus << 16) | (id->Device << 11) | (id->Function << 8) | (1 << 31) | regAligned;
	
	outd(PCI_CONFIG_ADDR, addr);
	uint32_t regval = ind(PCI_CONFIG_DATA);
	
	char *fieldptr = (char*) &regval + offsetIntoReg;
	size_t count = width/8;	
	*value = 0;
	memcpy(value, fieldptr, count);
	return AE_OK;
};

ACPI_STATUS AcpiOsWritePciConfiguration(ACPI_PCI_ID *id, UINT32 reg, UINT64 value, UINT32 width)
{
	uint32_t regAligned = reg & ~3;
	uint32_t offsetIntoReg = reg & 3;
	uint32_t addr = (id->Bus << 16) | (id->Device << 11) | (id->Function << 8) | (1 << 31) | regAligned;
	
	outd(PCI_CONFIG_ADDR, addr);
	uint32_t regval = ind(PCI_CONFIG_DATA);
	
	char *fieldptr = (char*) &regval + offsetIntoReg;
	size_t count = width/8;	
	//*value = 0;
	//memcpy(value, fieldptr, count);
	memcpy(fieldptr, &value, count);
	
	outd(PCI_CONFIG_ADDR, addr);
	outd(PCI_CONFIG_DATA, regval);
	
	return AE_OK;
};

static void* AcpiIntContexts[16];
static ACPI_OSD_HANDLER AcpiIntHandlers[16];

void AcpiOsGenericInt(int irq)
{
	AcpiIntHandlers[irq](AcpiIntContexts[irq]);
};

ACPI_STATUS AcpiOsInstallInterruptHandler(UINT32 irq, ACPI_OSD_HANDLER handler, void *context)
{
	AcpiIntContexts[irq] = context;
	AcpiIntHandlers[irq] = handler;
	registerIRQHandler(irq, AcpiOsGenericInt);
	return AE_OK;
};

ACPI_STATUS AcpiOsRemoveInterruptHandler(UINT32 intno, ACPI_OSD_HANDLER handler)
{
	(void)intno;
	(void)handler;
	return AE_OK;
};

static void splitPageIndex(uint64_t index, uint64_t *dirIndex, uint64_t *tableIndex, uint64_t *pageIndex)
{
	*pageIndex = (index & 0x1FF);
	index >>= 9;
	*tableIndex = (index & 0x1FF);
	index >>= 9;
	*dirIndex = (index & 0x1FF);
};

static PTe *acgetPage(uint64_t index)
{
	uint64_t dirIndex, tableIndex, pageIndex;
	splitPageIndex(index, &dirIndex, &tableIndex, &pageIndex);
	uint64_t base = 0xFFFF830000000000;

	int makeDir=0, makeTable=0;

	PDPT *pdpt = (PDPT*) (base + 0x7FFFFFF000);
	if (!pdpt->entries[dirIndex].present)
	{
		pdpt->entries[dirIndex].present = 1;
		uint64_t frame = phmAllocFrame();
		pdpt->entries[dirIndex].pdPhysAddr = frame;
		pdpt->entries[dirIndex].rw = 1;
		refreshAddrSpace();
		makeDir = 1;
	};

	PD *pd = (PD*) (base + 0x7FFFE00000 + dirIndex * 0x1000);
	if (makeDir) memset(pd, 0, 0x1000);
	if (!pd->entries[tableIndex].present)
	{
		pd->entries[tableIndex].present = 1;
		uint64_t frame = phmAllocFrame();
		pd->entries[tableIndex].ptPhysAddr = frame;
		pd->entries[tableIndex].rw = 1;
		refreshAddrSpace();
		makeTable = 1;
	};

	PT *pt = (PT*) (base + 0x7FC0000000 + dirIndex * 0x200000 + tableIndex * 0x1000);
	if (makeTable) memset(pt, 0, 0x1000);
	return &pt->entries[pageIndex];
};

void *AcpiOsMapMemory(ACPI_PHYSICAL_ADDRESS phaddr, ACPI_SIZE len)
{
	spinlockAcquire(&acpiMemoryLock);
	uint64_t startPhys = phaddr >> 12;
	uint64_t endPhys = (phaddr+len) >> 12;
	uint64_t outAddr = 0xFFFF830000000000 + nextFreePage * 0x1000 + (phaddr & 0xFFF);
	uint64_t frame;
	
	for (frame=startPhys; frame<=endPhys; frame++)
	{
		PTe *pte = acgetPage(nextFreePage++);
		pte->present = 1;
		pte->framePhysAddr = frame;
	};
	
	refreshAddrSpace();
	spinlockRelease(&acpiMemoryLock);
	return (void*) outAddr;
};

void AcpiOsUnmapMemory(void *laddr, ACPI_SIZE len)
{
	spinlockAcquire(&acpiMemoryLock);
	uint64_t startLog = ((uint64_t)laddr-0xFFFF830000000000) >> 12;
	uint64_t endLog = ((uint64_t)laddr-0xFFFF830000000000+len) >> 12;
	uint64_t idx;
	
	for (idx=startLog; idx<=endLog; idx++)
	{
		PTe *pte = acgetPage(idx);
		pte->present = 0;
		pte->framePhysAddr = 0;
	};
	
	refreshAddrSpace();
	spinlockRelease(&acpiMemoryLock);
};

void* mapPhysMemory(uint64_t phaddr, uint64_t len)
{
	return AcpiOsMapMemory(phaddr, len);
};

void unmapPhysMemory(void *laddr, uint64_t len)
{
	AcpiOsUnmapMemory(laddr, len);
};
