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
#include <glidix/mutex.h>

/**
 * Implements the ACPICA Operating System Layer (OSL) functions.
 */

//#define	TRACE()			kprintf("[ACGLIDIX] %s\n", __func__);
#define	TRACE()

static PAGE_ALIGN PDPT pdptAcpi;
static uint64_t nextFreePage = 0;
static Mutex acpiMemoryLock;

void* AcpiOsAllocate(ACPI_SIZE size)
{
	TRACE();
	return kmalloc(size);
};

void AcpiOsFree(void *ptr)
{
	TRACE();
	kfree(ptr);
};

void AcpiOsVprintf(const char *fmt, va_list ap)
{
	kvprintf(fmt, ap);
};

void AcpiOsPrintf(const char *fmt, ...)
{
	va_list ap;
	va_start(ap, fmt);
	kvprintf(fmt, ap);
	va_end(ap);
};

void AcpiOsSleep(UINT64 ms)
{
	TRACE();
	sleep(ms);
};

ACPI_PHYSICAL_ADDRESS AcpiOsGetRootPointer()
{
	TRACE();
	ACPI_SIZE Ret;
	AcpiFindRootPointer(&Ret);
	return Ret;
};

ACPI_STATUS AcpiOsWritePort(ACPI_IO_ADDRESS port, UINT32 value, UINT32 width)
{
	TRACE();
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
	TRACE();
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
	// microseconds for some reason
	TRACE();
	int then = getUptime() + (ms/1000 + 1);
	while (getUptime() < then);
};

ACPI_STATUS AcpiOsReadMemory(ACPI_PHYSICAL_ADDRESS addr, UINT64 *value, UINT32 width)
{
	TRACE();
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
	TRACE();
	void *ret = kmalloc(size);
	memset(ret, 0, size);
	return ret;
};

ACPI_STATUS AcpiOsCreateLock(ACPI_SPINLOCK *spinlock)
{
	TRACE();
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
	TRACE();
	kfree(spinlock);
};

ACPI_CPU_FLAGS AcpiOsAcquireLock(ACPI_SPINLOCK spinlock)
{
	TRACE();
	cli();
	spinlockAcquire(spinlock);
	return 0;
};

void AcpiOsReleaseLock(ACPI_SPINLOCK spinlock, ACPI_CPU_FLAGS flags)
{
	TRACE();
	(void)flags;
	spinlockRelease(spinlock);
	sti();
};

BOOLEAN AcpiOsReadable(void *mem, ACPI_SIZE size)
{
	TRACE();
	// this should not actually be called
	return 0;
};

ACPI_THREAD_ID AcpiOsGetThreadId()
{
	TRACE();
	return (ACPI_THREAD_ID) getCurrentThread();
};

UINT64 AcpiOsGetTimer()
{
	TRACE();
	return getNanotime() / 100;
};

ACPI_STATUS AcpiOsCreateSemaphore(UINT32 maxUnits, UINT32 initUnits, ACPI_SEMAPHORE *semptr)
{
	TRACE();
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
	TRACE();
	if (sem == NULL) return AE_BAD_PARAMETER;
	kfree(sem);
	return AE_OK;
};

ACPI_STATUS AcpiOsWaitSemaphore(ACPI_SEMAPHORE sem, UINT32 units, UINT16 timeout)
{
	TRACE();
	//kprintf("AcpiOsWaitSemaphore(%p, %u, %hu)\n", sem, units, timeout);
	//stackTraceHere();
	/*
	if (sem == (void*)0xFFFF810000290E80)
	{
		kprintf("Semaphore %p waited by %p\n", sem, getCurrentThread());
		stackTraceHere();
		
		//uint64_t sleepUntil = getNanotime() + NT_SECS(2);
		//while (getNanotime() < sleepUntil);
	};*/
	
	
	int count = (int) units;
	if (sem == NULL)
	{
		return AE_BAD_PARAMETER;
	};
	
	if (units == 0)
	{
		return AE_OK;
	};

	int flags;
	uint64_t nanoTimeout;
	
	if (timeout == 0)
	{
		flags = SEM_W_NONBLOCK;
		nanoTimeout = 0;
	}
	else if (timeout == 0xFFFF)
	{
		flags = 0;
		nanoTimeout = 0;
	}
	else
	{
		flags = 0;
		nanoTimeout = (uint64_t) timeout * 1000000;
	};
	
	uint64_t deadline = getNanotime() + nanoTimeout;
	int acquiredSoFar = 0;
	
	while (count > 0)
	{
		int got = semWaitGen(sem, count, flags, nanoTimeout);
		if (got < 0)
		{
			semSignal2(sem, acquiredSoFar);
			return AE_TIME;
		};
		
		acquiredSoFar += got;
		count -= got;
		
		uint64_t now = getNanotime();
		if (now < deadline)
		{
			nanoTimeout = deadline - now;
		}
		else if (nanoTimeout != 0)
		{
			semSignal2(sem, acquiredSoFar);
			return AE_TIME;
		};
	};
	
	return AE_OK;
};

ACPI_STATUS AcpiOsSignalSemaphore(ACPI_SEMAPHORE sem, UINT32 units)
{
	TRACE();
	//if (sem == (void*)0xFFFF810000290E80)
	//{
	//	kprintf("Semaphore %p signalled by %p\n", sem, getCurrentThread());
	//};
	
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
	TRACE();
	*read = 0;
	return AE_OK;
};

ACPI_STATUS AcpiOsSignal(UINT32 func, void *info)
{
	TRACE();
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
	TRACE();
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
	TRACE();
	memset(&pdptAcpi, 0, sizeof(PDPT));
	pdptAcpi.entries[511].present = 1;
	pdptAcpi.entries[511].rw = 1;
	pdptAcpi.entries[511].pdPhysAddr = ((uint64_t)&pdptAcpi - 0xFFFF800000000000) >> 12;
	PML4 *pml4 = getPML4();
	pml4->entries[262].present = 1;
	pml4->entries[262].rw = 1;
	pml4->entries[262].pdptPhysAddr = ((uint64_t)&pdptAcpi - 0xFFFF800000000000) >> 12;
	refreshAddrSpace();
	mutexInit(&acpiMemoryLock);
	return AE_OK;
};

ACPI_STATUS AcpiOsTerminate()
{
	TRACE();
	return AE_OK;
};

ACPI_STATUS AcpiOsPredefinedOverride(const ACPI_PREDEFINED_NAMES *PredefinedObject, ACPI_STRING *NewValue)
{
	TRACE();
	*NewValue = NULL;
	return AE_OK;
};

ACPI_STATUS AcpiOsTableOverride(ACPI_TABLE_HEADER *ExistingTable, ACPI_TABLE_HEADER **NewTable)
{
	TRACE();
	*NewTable = NULL;
	return AE_OK;
};

void AcpiOsWaitEventsComplete()
{
	TRACE();
};

ACPI_STATUS AcpiOsPhysicalTableOverride(ACPI_TABLE_HEADER *ExistingTable, ACPI_PHYSICAL_ADDRESS *NewAddress, UINT32 *NewTableLength)
{
	TRACE();
	*NewAddress = 0;
	return AE_OK;
};

ACPI_STATUS AcpiOsReadPciConfiguration(ACPI_PCI_ID *id, UINT32 reg, UINT64 *value, UINT32 width)
{
	TRACE();
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
	TRACE();
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
	TRACE();
	AcpiIntHandlers[irq](AcpiIntContexts[irq]);
};

ACPI_STATUS AcpiOsInstallInterruptHandler(UINT32 irq, ACPI_OSD_HANDLER handler, void *context)
{
	TRACE();
	AcpiIntContexts[irq] = context;
	AcpiIntHandlers[irq] = handler;
	registerIRQHandler(irq, AcpiOsGenericInt);
	return AE_OK;
};

ACPI_STATUS AcpiOsRemoveInterruptHandler(UINT32 intno, ACPI_OSD_HANDLER handler)
{
	TRACE();
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
	TRACE();
	mutexLock(&acpiMemoryLock);
	uint64_t startPhys = phaddr >> 12;
	uint64_t endPhys = (phaddr+len) >> 12;
	uint64_t outAddr = 0xFFFF830000000000 + nextFreePage * 0x1000 + (phaddr & 0xFFF);
	uint64_t frame;
	
	for (frame=startPhys; frame<=endPhys; frame++)
	{
		PTe *pte = acgetPage(nextFreePage++);
		pte->present = 1;
		pte->framePhysAddr = frame;
		pte->pcd = 1;
		pte->rw = 1;
	};
	
	refreshAddrSpace();
	mutexUnlock(&acpiMemoryLock);
	return (void*) outAddr;
};

void AcpiOsUnmapMemory(void *laddr, ACPI_SIZE len)
{
	TRACE();
	mutexLock(&acpiMemoryLock);
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
	mutexUnlock(&acpiMemoryLock);
};

void* mapPhysMemory(uint64_t phaddr, uint64_t len)
{
	return AcpiOsMapMemory(phaddr, len);
};

void unmapPhysMemory(void *laddr, uint64_t len)
{
	AcpiOsUnmapMemory(laddr, len);
};

ACPI_STATUS AcpiOsCreateMutex(ACPI_MUTEX *out)
{
	Mutex *mutex = (Mutex*) kmalloc(sizeof(Mutex));
	mutexInit(mutex);
	*out = mutex;
	return AE_OK;
};

void AcpiOsDeleteMutex(ACPI_MUTEX mutex)
{
	kfree(mutex);
};

ACPI_STATUS AcpiOsAcquireMutex(ACPI_MUTEX handle, UINT16 timeout)
{
	// TODO: take timeout into account
	mutexLock((Mutex*)handle);
	return AE_OK;
};

void AcpiOsReleaseMutex(ACPI_MUTEX handle)
{
	mutexUnlock((Mutex*)handle);
};
