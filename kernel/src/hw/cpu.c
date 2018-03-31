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

#include <glidix/util/common.h>
#include <glidix/hw/pagetab.h>
#include <glidix/hw/cpu.h>
#include <glidix/hw/physmem.h>
#include <glidix/util/string.h>
#include <glidix/hw/acpi.h>
#include <glidix/hw/apic.h>
#include <glidix/util/time.h>
#include <glidix/display/console.h>
#include <glidix/util/memory.h>
#include <glidix/util/isp.h>
#include <glidix/thread/sched.h>
#include <glidix/hw/idt.h>
#include <glidix/hw/fpu.h>
#include <glidix/hw/msr.h>

/**
 * Uncomment this to enable SMP.
 * Currently there is a bug where the page fault handler causes a page fault
 * while attempting to get the procmem spinlock.
 */
#define	ENABLE_SMP

static PER_CPU CPU *currentCPU;
PER_CPU char localGDT[64];
PER_CPU char localTSS[192];
PER_CPU TSS *localTSSPtr;
PER_CPU char* localGDTPtr;

extern char _per_cpu_start;
extern char _per_cpu_end;
void _syscall_entry();

static uint16_t cpuReadyBitmap;

void initPerCPU()
{
	PML4 *pml4 = getPML4();
	pml4->entries[261].present = 1;
	pml4->entries[261].pdptPhysAddr = phmAllocZeroFrame();
	pml4->entries[261].rw = 1;
	refreshAddrSpace();
	
	uint64_t firstPage = (uint64_t) &_per_cpu_start / 0x1000;
	uint64_t lastPage = (uint64_t) &_per_cpu_start / 0x1000;
	
	uint64_t page;
	for (page=firstPage; page<=lastPage; page++)
	{
		// we already know the PDPT is 261.
		uint64_t pageIndex = page & 0x1FF;
		uint64_t ptIndex = (page >> 9) & 0x1FF;
		uint64_t pdIndex = (page >> 18) & 0x1FF;
		
		PDPT *pdpt = (PDPT*) 0xFFFFFFFFFFF05000;
		if (!pdpt->entries[pdIndex].present)
		{
			pdpt->entries[pdIndex].present = 1;
			pdpt->entries[pdIndex].pdPhysAddr = phmAllocZeroFrame();
			pdpt->entries[pdIndex].rw = 1;
			refreshAddrSpace();
		};
		
		PD *pd = (PD*) (0xFFFFFFFFE0A00000 + 0x1000 * pdIndex);
		if (!pd->entries[ptIndex].present)
		{
			pd->entries[ptIndex].present = 1;
			pd->entries[ptIndex].ptPhysAddr = phmAllocZeroFrame();
			pd->entries[ptIndex].rw = 1;
			refreshAddrSpace();
		};
		
		PT *pt = (PT*) (0xFFFFFFC140000000 + 0x200000 * pdIndex + 0x1000 * ptIndex);
		if (!pt->entries[pageIndex].present)
		{
			pt->entries[pageIndex].present = 1;
			pt->entries[pageIndex].framePhysAddr = phmAllocZeroFrame();
			pt->entries[pageIndex].rw = 1;
			refreshAddrSpace();
		};
	};
	
	// zero out all per-CPU variables
	uint64_t per_cpu_size = (uint64_t) &_per_cpu_end - (uint64_t) &_per_cpu_start;
	memset(&_per_cpu_start, 0, per_cpu_size);

	initPerCPU2();
};

void initPerCPU2()
{
	msrWrite(MSR_STAR, ((uint64_t)8 << 32) | ((uint64_t)0x1b << 48));
	msrWrite(MSR_LSTAR, (uint64_t)(&_syscall_entry));
	msrWrite(MSR_CSTAR, (uint64_t)(&_syscall_entry));		// we don't actually use compat mode
	msrWrite(MSR_SFMASK, (1 << 9) | (1 << 10));			// disable interrupts on syscall and set DF=0
	msrWrite(MSR_EFER, msrRead(MSR_EFER) | EFER_SCE | EFER_NXE);
};

extern char trampoline_start;
extern char trampoline_end;
extern char GDT64;
extern char GDTPointer;
extern char idtPtr;
extern void loadLocalGDT();	// trampoline.asm

CPU cpuList[16];
int numCPU = 1;
void initMultiProc()
{
	cpuReadyBitmap = 0;
	memset(cpuList, 0, sizeof(CPU)*16);
	
	cpuList[0].id = 0;
	cpuList[0].apicID = (apic->id >> 24);
	currentCPU = &cpuList[0];

#ifdef ENABLE_SMP
	int cpuno = 1;
	int i;
	for (i=0; i<apicCount; i++)
	{
		if (apicList[i] != (apic->id >> 24))
		{
			cpuList[cpuno].id = cpuno;
			cpuList[cpuno].apicID = apicList[i];
			cpuno++;
		};
	};
	
	numCPU = cpuno;
	kprintf("Found %d CPUs\n", numCPU);
	
	uint64_t trampolineSize = (uint64_t) &trampoline_end - (uint64_t) &trampoline_start;
	memcpy((void*)0xFFFF80000000A000, &trampoline_start, trampolineSize);
	
	TrampolineData *tramData = (TrampolineData*) 0xFFFF80000000B000;
	
	for (cpuno=1; cpuno<numCPU; cpuno++)
	{
		// the GDT must be in low memory at first
		kprintf("Starting up CPU %d (APIC ID 0x%02X)... ", cpuno, cpuList[cpuno].apicID);
		uint16_t *limitPtr = (uint16_t*) &tramData->gdtPtr[0];
		uint64_t *ptrPtr = (uint64_t*) &tramData->gdtPtr[2];
		
		uint16_t gdtLimit = (uint64_t) &GDTPointer - (uint64_t) &GDT64 - 1;
		*limitPtr = gdtLimit;
		*ptrPtr = (uint64_t) &GDT64 - 0xFFFF800000000000;
		
		// we pass the real IDT
		memcpy(&tramData->idtPtr, &idtPtr, 10);
		
		// set up the new CPU's PML4 in the temporary area, then copy it to a new frame
		PML4 *pmlAP = (PML4*) 0xFFFF80000000C000;
		PML4 *pmlBSP = getPML4();
		memcpy(pmlAP, pmlBSP, 0x1000);
		
		// map the bottom of memory to the static kernel image, so that the AP
		// can boot in low memory
		pmlAP->entries[0] = pmlAP->entries[256];
		
		// allocate a permanent frame for the PML4 and hence recursive-map the PML4
		uint64_t pmlFrame = phmAllocFrame();
		pmlAP->entries[511].present = 1;
		pmlAP->entries[511].pdptPhysAddr = pmlFrame;
		
		// zero out the per-CPU entry just in case the AP tries destroying the BSP's
		// per-CPU variables for some reason
		memset(&pmlAP->entries[261], 0, 8);
		
		// now also copy the PML4 over to the real frame
		ispLock();
		ispSetFrame(pmlFrame);
		memcpy(ispGetPointer(), pmlAP, 0x1000);
		ispUnlock();
		
		// OK, pass the PML4 to the AP
		tramData->pageMapPhys = pmlFrame * 0x1000;
		
		// allocate a stack for the AP (64KB)
		tramData->initStack = (uint64_t) kmalloc(0x10000) + 0x10000;
		
		// initialize the exitLock as locked
		spinlockRelease(&tramData->exitLock);
		spinlockAcquire(&tramData->exitLock);
		
		// initialize the flags as locked
		spinlockRelease(&tramData->flagAP2BSP);
		spinlockAcquire(&tramData->flagAP2BSP);
		spinlockRelease(&tramData->flagBSP2AP);
		spinlockAcquire(&tramData->flagBSP2AP);
		
		__sync_synchronize();
		
		// initialize the CPU and wait 10ms
		apic->icrHigh = (uint32_t) cpuList[cpuno].apicID << 24;
		__sync_synchronize();
		apic->icrLow = 0x00004500;
		__sync_synchronize();
		sleep(10);
		
		// try to launch the trampoline
		apic->icrHigh = (uint32_t) cpuList[cpuno].apicID << 24;
		__sync_synchronize();
		apic->icrLow = 0x0000460A;
		__sync_synchronize();
		
		// wait for response
		int startTicks = getTicks();
		int ok = 0;
		while ((getTicks()-startTicks) < 5)
		{
			__sync_synchronize();
			if (spinlockTry(&tramData->flagAP2BSP) == 0)
			{
				ok = 1;
				break;
			};
		};
		
		// if that didn't work, try one more time
		if (!ok)
		{
			apic->icrHigh = (uint32_t) cpuList[cpuno].apicID << 24;
			__sync_synchronize();
			apic->icrLow = 0x0000460A;
			__sync_synchronize();
			
			while ((getTicks()-startTicks) < 1000)
			{
				__sync_synchronize();
				if (spinlockTry(&tramData->flagAP2BSP) == 0)
				{
					ok = 1;
					break;
				};
			};
			
			if (!ok)
			{
				FAILED();
				panic("CPU with APIC ID %d refuses to start up\n", (int) cpuList[cpuno].apicID);
			};
		};
		
		// tell the AP we've seen it and it may continue
		spinlockRelease(&tramData->flagBSP2AP);
		__sync_synchronize();
		
		// wait to be able to continue
		DONE();
		kprintf("Waiting for release... ");
		spinlockAcquire(&tramData->exitLock);
		
		DONE();
	};
#endif
};

void haltAllCPU()
{
	if (numCPU == 1) return;
	
	uint64_t retflags = getFlagsRegister();
	cli();
	
	int i;
	for (i=0; i<numCPU; i++)
	{
		if (cpuList[i].apicID != (apic->id >> 24))
		{
			apic->icrHigh = cpuList[i].apicID << 24;
			__sync_synchronize();
			apic->icrLow = 0x00004400;	// NMI
			__sync_synchronize();
			
			while (apic->icrLow & (1 << 12))
			{
				__sync_synchronize();
			};
		};
	};
	
	setFlagsRegister(retflags);
};

CPU *getCurrentCPU()
{
	return currentCPU;
};

void sendHintToCPU(int cpuID)
{
	uint64_t retflags = getFlagsRegister();
	cli();
	
	apic->icrHigh = cpuList[cpuID].apicID << 24;
	__sync_synchronize();
	apic->icrLow = 0x00004071;	// I_IPI_SCHED_HINT
	__sync_synchronize();

	while (apic->icrLow & (1 << 12))
	{
		__sync_synchronize();
	};
	
	setFlagsRegister(retflags);
};

void sendHintToEveryCPU()
{
	if (numCPU == 1) return;
	
	int i;
	for (i=0; i<numCPU; i++)
	{
		if (i != getCurrentCPU()->id)
		{
			sendHintToCPU(i);
		};
	};
};

typedef struct
{
	uint16_t		limitLow;
	uint16_t		baseLow;
	uint8_t			baseMiddle;
	uint8_t			access;
	uint8_t			limitHigh;
	uint8_t			baseMiddleHigh;
	uint32_t		baseHigh;
} PACKED GDT_TSS_Segment;

/**
 * This function is called when the AP trampoline has jumped to long mode and we are
 * ready to start up the CPU.
 */
void apEntry()
{
	PML4 *pml4 = getPML4();
	
	// unmap low memory now
	pml4->entries[0].present = 0;
	pml4->entries[0].pdptPhysAddr = 0;

	// set up the APIC
	msrWrite(0x1B, 0xFEE00000 | (1 << 11) /*| (1 << 8)*/ );
	apic->sivr = 0x1FF;
	apic->timerDivide = 3;
	apic->lvtTimer = I_APIC_TIMER;
	
	// initialise per-CPU variables and other per-CPU stuff
	initPerCPU();
	
	// make a copy of the GDT and TSS so that we can have a separate kernel stack
	memcpy(localGDT, &GDT64, 64);
	GDT_TSS_Segment *segTSS = (GDT_TSS_Segment*) &localGDT[0x30];
	segTSS->limitLow = 191;
	segTSS->limitHigh = 0;
	
	uint64_t tssAddr = (uint64_t) localTSS;
	segTSS->baseLow = tssAddr & 0xFFFF;
	segTSS->baseMiddle = (tssAddr >> 16) & 0xFF;
	segTSS->baseMiddleHigh = (tssAddr >> 24) & 0xFF;
	segTSS->baseHigh = (tssAddr >> 32) & 0xFFFFFFFF;
	segTSS->access = 0xE9;
	loadLocalGDT();
	
	localTSSPtr = (TSS*) localTSS;
	localGDTPtr = localGDT;
	
	// find the current CPU and set currentCPU to it
	uint8_t apicID = apic->id >> 24;
	int i;
	for (i=0; i<numCPU; i++)
	{
		if (cpuList[i].apicID == apicID)
		{
			currentCPU = &cpuList[i];
			break;
		};
	};
	
	// initialize the FPU
	fpuInit();
	
	// go to the scheduler
	initSchedAP();
};

void cpuReady()
{
	if (getCurrentCPU() == NULL) return;
	__sync_fetch_and_or(&cpuReadyBitmap, (1 << getCurrentCPU()->id));
};

void cpuBusy()
{
	if (getCurrentCPU() == NULL) return;
	__sync_fetch_and_and(&cpuReadyBitmap, ~(1 << getCurrentCPU()->id));
};

void cpuDispatch()
{
	if (getCurrentCPU() == NULL) return;
	if (numCPU == 1) return;

	int i;
	for (i=0; i<numCPU; i++)
	{
		uint16_t mask = 1 << i;
		if (__sync_fetch_and_and(&cpuReadyBitmap, ~mask) & mask)
		{
			if (i != getCurrentCPU()->id) sendHintToCPU(i);
			return;
		};
	};
};

int cpuSleeping()
{
	if (getCurrentCPU() == NULL) return 1;
	return cpuReadyBitmap & (1 << getCurrentCPU()->id);
};
