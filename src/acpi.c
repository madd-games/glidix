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

#include <glidix/acpi.h>
#include <glidix/console.h>
#include <glidix/string.h>
#include <glidix/isp.h>
#include <glidix/memory.h>
#include <glidix/apic.h>

static uint32_t acpiNumTables;
static uint32_t *acpiTables;
static MADT_IOAPIC madtIOAPIC;

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
	kprintf("START: %a\n", start);
	return findRSDPInRange(start, start+0x400);
};

// maps an IRQ to a system interrupt
static int irqMap[16];

static void ioapicInit(uint64_t ioapicbasephys)
{
	uint32_t volatile* regsel = (uint32_t volatile*) /*(ioapicbasephys+0xFFFF800000000000)*/ 0xFFFF808000002000;
	uint32_t volatile* iowin = (uint32_t volatile*) /*(ioapicbasephys+0xFFFF800000000010)*/ 0xFFFF808000002010;
	*regsel = 1;
	__sync_synchronize();
	uint32_t maxintr = ((*iowin) >> 16) & 0xFF;
	__sync_synchronize();

	kprintf("APIC ID: %d, NUM INTERRUPTS: %d\n", apic->id, maxintr);
	uint8_t *checkInts = (uint8_t*) kmalloc(maxintr+1);
	memset(checkInts, 0, maxintr+1);
	
	int i;
	for (i=0; i<16; i++)
	{
		kprintf("SYSINT %d -> LOCAL INT %d\n", irqMap[i], i+32);
		*regsel = (0x10+2*irqMap[i]);
		__sync_synchronize();
		uint64_t entry = (uint64_t)(i+32) | ((uint64_t)(apic->id) << 56);
		*iowin = (uint32_t)(entry);
		__sync_synchronize();
		*regsel = (0x10+2*irqMap[i]+1);
		__sync_synchronize();
		*iowin = (uint32_t)(entry >> 32);
		__sync_synchronize();
		
		checkInts[irqMap[i]] = 1;
	};

	for (i=0; i<=maxintr; i++)
	{
		if (!checkInts[i])
		{
			kprintf("SYSINT %d -> LOCAL INT 7\n", i);
			*regsel = (0x10+2*i);
			__sync_synchronize();
			uint64_t entry = (uint64_t)(39) | ((uint64_t)(apic->id) << 56);
			*iowin = (uint32_t)(entry);
			__sync_synchronize();
			*regsel = (0x10+2*i+1);
			__sync_synchronize();
			*iowin = (uint32_t)(entry >> 32);
			__sync_synchronize();
		};
	};
	
	kfree(checkInts);
	kprintf("Done.\n");

	//panic("Stop");
};

void acpiInit()
{
	ACPI_RSDPDescriptor *rsdp = findRSDP();

	if (rsdp == NULL)
	{
		FAILED();
		panic("cannot find RSDP");
	};

	DONE();

#if 0
	kprintf("ACPI RSDP addr: %a\n", rsdp);
	kprintf("Revision: %d\n", rsdp->rev);
	kprintf("Therefore RSDT is physically at %a\n", rsdp->rsdtAddr);
#endif

	ACPI_SDTHeader rsdtHeader;
	pmem_read(&rsdtHeader, rsdp->rsdtAddr, sizeof(ACPI_SDTHeader));

	acpiNumTables = (rsdtHeader.len - sizeof(rsdtHeader)) / 4;
	kprintf("Number of ACPI tables: %d\n", acpiNumTables);
	acpiTables = (uint32_t*) kmalloc(acpiNumTables*4);
	pmem_read(acpiTables, rsdp->rsdtAddr+sizeof(ACPI_SDTHeader), acpiNumTables*4);

	int i;
	for (i=0; i<16; i++)
	{
		irqMap[i] = i;
	};
	
	uint64_t ioapicphys = 0;
	
	ACPI_SDTHeader head;
	uint32_t payloadPhysAddr;
	for (i=0; i<acpiNumTables; i++)
	{
		acpiReadTable(i, &head, &payloadPhysAddr);
		char buf[5];
		buf[4] = 0;
		memcpy(buf, head.sig, 4);
		kprintf("Table %d: %s\n", i, buf);
		if (memcmp(head.sig, "APIC", 4) == 0)
		{
			uint32_t searching = payloadPhysAddr + 8;
			while (searching < payloadPhysAddr+head.len)
			{
				MADTRecordHeader rhead;
				pmem_read(&rhead, searching, sizeof(MADTRecordHeader));

				//kprintf("TYPE=%d, LEN=%d\n", rhead.type, rhead.len);
				if (rhead.type == 1)
				{
					pmem_read(&madtIOAPIC, searching+sizeof(MADTRecordHeader), sizeof(MADT_IOAPIC));
					kprintf("BASE: %a\n", madtIOAPIC.ioapicbase);
					kprintf("INTBASE: %a\n", madtIOAPIC.intbase);
					//ioapicInit(madtIOAPIC.ioapicbase);
					ioapicphys = madtIOAPIC.ioapicbase;
				}
				else if (rhead.type == 2)
				{
					MADT_IntOvr intovr;
					pmem_read(&intovr, searching, sizeof(MADT_IntOvr));
					kprintf("INTOVR: BUS=%d, IRQ=%d, SYSINT=%d, FLAGS=%a\n", intovr.bus, intovr.irq, intovr.sysint, intovr.flags);
					if (intovr.bus == 0)
					{
						if (intovr.irq != intovr.sysint)
						{
							irqMap[intovr.irq] = intovr.sysint;
							if (intovr.sysint < 16)
							{
								irqMap[intovr.sysint] = 7;
							};
						};
					};
				};
				
				if (rhead.len == 0) break;
				searching += rhead.len;
			};
		};
	};
	
	if (ioapicphys == 0)
	{
		panic("failed to find the IOAPIC ACPI table");
	};
	
	ioapicInit(ioapicphys);
	//panic("ye");
};
