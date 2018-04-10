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

#include <glidix/hw/acpi.h>
#include <glidix/display/console.h>
#include <glidix/util/string.h>
#include <glidix/util/isp.h>
#include <glidix/util/memory.h>
#include <glidix/hw/apic.h>
#include <glidix/hw/pagetab.h>
#include <glidix/hw/idt.h>

static uint32_t acpiNumTables;
static uint32_t *acpiTables;
static MADT_IOAPIC madtIOAPIC;

/**
 * I/O APIC memory-mapped registers (REGSELF and IOWIN).
 */
typedef struct
{
	volatile uint32_t			regsel;		// 0x00
	volatile uint32_t			pad0;		// 0x04
	volatile uint32_t			pad1;		// 0x08
	volatile uint32_t			pad2;		// 0x0C
	volatile uint32_t			iowin;		// 0x10
} IoApic;

/**
 * Information about a detected I/O APIC.
 */
typedef struct
{
	/**
	 * Virtual location into which the I/O APIC register space was mapped.
	 */
	IoApic*					regs;
	
	/**
	 * Interrupt base (first handled interrupt number accoridng to MADT).
	 */
	int					intbase;
	
	/**
	 * Number of interrupts handled by this I/O APIC.
	 */
	int					intcnt;
} IoApicInfo;

/**
 * Array of APIC information structures.
 */
static IoApicInfo ioapics[16];

/**
 * Number of detected I/O APICs.
 */
static int numIoApics = 0;

static void acpiReadTable(int index, ACPI_SDTHeader *head, uint32_t *payloadPhysAddr)
{
	uint32_t physloc = acpiTables[index];
	pmem_read(head, physloc, sizeof(ACPI_SDTHeader));
	*payloadPhysAddr = physloc + sizeof(ACPI_SDTHeader);
};

static ACPI_RSDPDescriptor *findRSDPInRange(uint64_t start, uint64_t end)
{
	uint64_t try;
	for (try=start; try<end; try+=0x10)
	{
		if (memcmp((void*)try, "RSD PTR ", 8) == 0)
		{
			uint8_t *test = (uint8_t*) try;
			uint8_t sum = 0;
			int i;

			for (i=0; i<20; i++)
			{
				sum += test[i];
			};

			if (sum == 0)
			{
				return (ACPI_RSDPDescriptor*) try;
			};
		};
	};

	return NULL;
};

static ACPI_RSDPDescriptor *findRSDP()
{
	ACPI_RSDPDescriptor *rsdp = findRSDPInRange(0xFFFF8000000E0000, 0xFFFF800000100000);
	if (rsdp != NULL) return rsdp;

	uint16_t ebdaSegment = *((uint16_t*)0xFFFF80000000040E);
	uint64_t start = 0xFFFF800000000000+(((uint64_t)ebdaSegment) << 4);
	return findRSDPInRange(start, start+0x400);
};

// maps an IRQ to a system interrupt
static int irqMap[16];
static uint16_t irqFlags[16];

void ioapicDetected(uint64_t base, uint32_t intbase)
{
	int index = numIoApics++;
	
	// the page table is already created, by isp.c.
	uint64_t addr = 0xFFFF808000004000 + ((uint64_t)index << 12);
	uint64_t ptaddr = 0xFFFFFF8000000000 | (addr >> 9);
	PTe *pte = (PTe*) ptaddr;
	
	pte->present = 1;
	pte->framePhysAddr = (base >> 12);
	pte->pcd = 1;
	pte->rw = 1;
	refreshAddrSpace();
	
	IoApic *regs = (IoApic*) addr;
	ioapics[index].regs = regs;
	ioapics[index].intbase = (int) intbase;
	
	// get the number of interrupts
	regs->regsel = 1;
	uint32_t maxintr = (regs->iowin >> 16) & 0xFF;
	ioapics[index].intcnt = (int) (maxintr+1);
	
	// mask all interrupts until programmed
	int intno;
	for (intno=0; intno<ioapics[index].intcnt; intno++)
	{
		regs->regsel = (0x10+2*intno);
		__sync_synchronize();
		regs->iowin = (1 << 16);		// mask interrupt
		__sync_synchronize();
		regs->regsel = (0x10+2*intno+1);
		__sync_synchronize();
		regs->iowin = 0;
		__sync_synchronize();
	};
	
	kprintf("I/O APIC: intbase=%d, intcnt=%d, phys=0x%016lX\n",
		ioapics[index].intbase, ioapics[index].intcnt, base);
};

int mapInterrupt(int sysint, uint64_t entry)
{
	static Spinlock apicLock;
	uint64_t flags = getFlagsRegister();
	cli();
	spinlockAcquire(&apicLock);
	
	int i;
	for (i=0; i<numIoApics; i++)
	{
		IoApicInfo *info = &ioapics[i];
		if ((info->intbase <= sysint) && ((info->intbase+info->intcnt) > sysint))
		{
			sysint -= info->intbase;
			info->regs->regsel = (0x10+2*sysint);
			__sync_synchronize();
			info->regs->iowin = (uint32_t)(entry);
			__sync_synchronize();
			info->regs->regsel = (0x10+2*sysint+1);
			__sync_synchronize();
			info->regs->iowin = (uint32_t)(entry >> 32);
			__sync_synchronize();
			break;
		};
	};
	
	spinlockRelease(&apicLock);
	setFlagsRegister(flags);
	return i;
};

static void configInts()
{
	kprintf("LOCAL APIC ID: 0x%08X\n", apic->id);
	uint8_t checkInts[16];
	memset(checkInts, 0, 16);

	int i;
	for (i=0; i<16; i++)
	{
		if (irqMap[i] != -1)
		{
			uint64_t entry = (uint64_t)(i+32) | ((uint64_t)(apic->id >> 24) << 56);
			if ((irqFlags[i] & 3) == 3)
			{
				entry |= (1 << 13);		// active low
			};
			if ((irqFlags[i] & 0x0C) == 0x0C)
			{
				entry |= (1 << 15);
			};
			
			kprintf("SYSINT %d -> LOCAL INT %d (flags=%d, ioapic=%d)\n",
				irqMap[i], i+32, irqFlags[i], mapInterrupt(irqMap[i], entry));
			if (irqMap[i] < 16) checkInts[irqMap[i]] = 1;
		};
	};

	kprintf("Done.\n");
};

uint8_t apicList[16];
int apicCount = 0;

void acpiInit()
{
	ACPI_RSDPDescriptor *rsdp = findRSDP();

	if (rsdp == NULL)
	{
		FAILED();
		panic("cannot find RSDP");
	};

	DONE();

	ACPI_SDTHeader rsdtHeader;
	pmem_read(&rsdtHeader, rsdp->rsdtAddr, sizeof(ACPI_SDTHeader));

	acpiNumTables = (rsdtHeader.len - sizeof(rsdtHeader)) / 4;
	acpiTables = (uint32_t*) kmalloc(acpiNumTables*4);
	pmem_read(acpiTables, rsdp->rsdtAddr+sizeof(ACPI_SDTHeader), acpiNumTables*4);

	int i;
	for (i=0; i<16; i++)
	{
		irqMap[i] = i;
	};
	
	//uint64_t ioapicphys = 0;
	
	ACPI_SDTHeader head;
	uint32_t payloadPhysAddr;
	for (i=0; i<acpiNumTables; i++)
	{
		acpiReadTable(i, &head, &payloadPhysAddr);
		char buf[5];
		buf[4] = 0;
		memcpy(buf, head.sig, 4);
		//kprintf("Table %d: %s\n", i, buf);
		if (memcmp(head.sig, "APIC", 4) == 0)
		{
			uint32_t searching = payloadPhysAddr + 8;
			while (searching < payloadPhysAddr+head.len)
			{
				MADTRecordHeader rhead;
				pmem_read(&rhead, searching, sizeof(MADTRecordHeader));

				//kprintf("TYPE=%d, LEN=%d\n", rhead.type, rhead.len);
				if (rhead.type == 0)
				{
					MADT_APIC apicInfo;
					pmem_read(&apicInfo, searching+sizeof(MADTRecordHeader), sizeof(MADT_APIC));
					
					if (apicInfo.flags & 1)
					{
						//kprintf("Detected APIC: %d\n", (int) apicInfo.apicID);
						if (apicCount < 16)
						{
							apicList[apicCount++] = apicInfo.apicID;
						};
					};
				}
				else if (rhead.type == 1)
				{
					pmem_read(&madtIOAPIC, searching+sizeof(MADTRecordHeader), sizeof(MADT_IOAPIC));
					ioapicDetected(madtIOAPIC.ioapicbase, madtIOAPIC.intbase);
				}
				else if (rhead.type == 2)
				{
					MADT_IntOvr intovr;
					pmem_read(&intovr, searching, sizeof(MADT_IntOvr));
					kprintf("INTOVR: BUS=%d, IRQ=%d, SYSINT=%d, FLAGS=%d\n", intovr.bus, intovr.irq, intovr.sysint, intovr.flags);
					if (intovr.bus == 0)
					{
						irqFlags[intovr.irq] = intovr.flags;
						irqMap[intovr.irq] = intovr.sysint;
						if (intovr.sysint < 16 && intovr.irq != intovr.sysint)
						{
							irqMap[intovr.sysint] = -1;
						};
					};
				};
				
				if (rhead.len == 0) break;
				searching += rhead.len;
			};
		};
	};
	
	configInts();
};
