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

#include <glidix/pci.h>
#include <glidix/port.h>
#include <glidix/devfs.h>
#include <glidix/vfs.h>
#include <glidix/spinlock.h>
#include <glidix/string.h>
#include <glidix/sched.h>
#include <glidix/errno.h>
#include <glidix/memory.h>
#include <glidix/console.h>
#include <glidix/acpi.h>
#include <glidix/apic.h>
#include <glidix/idt.h>

static Spinlock pciLock;

static PCIDevice *pciDevices = NULL;
static PCIDevice *lastDevice = NULL;
static int nextPCIID = 0;

static int pciIntInitDone = 0;

static void checkBus(uint8_t bus);
static void checkSlot(uint8_t bus, uint8_t slot)
{
	PCIDeviceConfig config;
	uint8_t funcs = 1;
	
	pciGetDeviceConfig(bus, slot, 0, &config);
	if (config.std.vendor == 0xFFFF) return;
	
	if (config.std.headerType & 0x80)
	{
		funcs = 8;
	};
	
	if ((config.std.headerType & 0x7F) == 0x01)
	{
		// PCI-to-PCI bridge, scan secondary bus
		checkBus(config.bridge.secondaryBus);
	}
	else
	{
		uint8_t func = 0;
		do
		{
			if (config.std.vendor != 0xFFFF)
			{
				PCIDevice *dev = (PCIDevice*) kmalloc(sizeof(PCIDevice));
				dev->next = NULL;
				dev->bus = bus;
				dev->slot = slot;
				dev->func = func;
				dev->id = nextPCIID++;
				dev->vendor = config.std.vendor;
				dev->device = config.std.device;
				uint16_t classcode16 = (uint16_t) config.std.classcode;
				uint16_t subclass16 = (uint16_t) config.std.subclass;
				dev->type = (classcode16 << 8) | (subclass16 & 0xFF);
				dev->intpin = config.std.intpin;
				dev->driver = NULL;
				strcpy(dev->driverName, "null");
				strcpy(dev->deviceName, "Unknown");
				memcpy(dev->bar, config.std.bar, 4*6);
				dev->intNo = 0;
				wcInit(&dev->wcInt);
				
				if (lastDevice == NULL)
				{
					pciDevices = dev;
					lastDevice = dev;
				}
				else
				{
					lastDevice->next = dev;
					lastDevice = dev;
				};
			};
			
			func++;
			if (func != 8)
			{
				pciGetDeviceConfig(bus, slot, func, &config);
			};
		} while (func < funcs);
	};
};

static void checkBus(uint8_t bus)
{
	uint8_t slot;
	for (slot=0; slot<32; slot++)
	{
		checkSlot(bus, slot);
	};
};

/**
 * Called when the local interrupt for a device was determined.
 */
static void pciMapLocalInterrupt(uint8_t bus, uint8_t slot, uint8_t intpin, int intNo)
{
	PCIDevice *dev;
	for (dev=pciDevices; dev!=NULL; dev=dev->next)
	{
		if ((dev->bus == bus) && (dev->slot == slot) && (dev->intpin == intpin))
		{
			dev->intNo = intNo;
			return;
		};
	};
};

typedef struct
{
	int			gsi;
	int			intNo;
} PCIGlobalInterruptMapping;
static PCIGlobalInterruptMapping *gsiMapCache = NULL;
static int gsiMapSize = 0;

/**
 * Called when we have determined that a given device is connected to a given Global System Interrupt (GSI).
 * If that GSI was previously mapped, we look back at our mapping cache and use the local interrupt it was
 * connected to. Otherwise we get the next free local interrupt (or reuse one) and add that to the cache.
 */
static void pciMapInterruptFromGSI(uint8_t bus, uint8_t slot, uint8_t intpin, int gsi)
{
	kprintf_debug("PCI %d/%d INT%c# -> GSI %d\n", (int) bus, (int) slot, 'A'+intpin, gsi);
	static int nextLocalInt = 0;
	
	int i;
	for (i=0; i<gsiMapSize; i++)
	{
		if (gsiMapCache[i].gsi == gsi)
		{
			pciMapLocalInterrupt(bus, slot, intpin, gsiMapCache[i].intNo);
			return;
		};
	};
	
	int intNo = I_PCI0 + nextLocalInt;
	nextLocalInt = (nextLocalInt + 1) % 16;
	
	PCIGlobalInterruptMapping *newCache = (PCIGlobalInterruptMapping*) kmalloc(sizeof(PCIGlobalInterruptMapping)*(gsiMapSize+1));
	memcpy(newCache, gsiMapCache, sizeof(PCIGlobalInterruptMapping)*gsiMapSize);
	newCache[gsiMapSize].gsi = gsi;
	newCache[gsiMapSize].intNo = intNo;
	if (gsiMapCache != NULL) kfree(gsiMapCache);
	gsiMapCache = newCache;
	
	// remap the interrupt
	uint32_t volatile* regsel = (uint32_t volatile*) 0xFFFF808000002000;
	uint32_t volatile* iowin = (uint32_t volatile*) 0xFFFF808000002010;
	*regsel = (0x10+2*gsi);
	__sync_synchronize();
	uint64_t entry = (uint64_t)(intNo) | ((uint64_t)(apic->id) << 56);
	*iowin = (uint32_t) entry;
	__sync_synchronize();
	*regsel = (0x10+2*gsi+1);
	__sync_synchronize();
	*iowin = (uint32_t)(entry>>32);
	__sync_synchronize();
	
	// and register with the device
	pciMapLocalInterrupt(bus, slot, intpin, intNo);
};

/**
 * Called on an ISA IRQ for PCI.
 */
static void pciOnIRQ(int irq)
{
	pciInterrupt(IRQ0+irq);
};

/**
 * Called when we have determined that a PCI device receives its interrupt from an ISA IRQ.
 */
static void pciMapInterruptFromIRQ(uint8_t bus, uint8_t slot, uint8_t intpin, int irq)
{
	pciMapLocalInterrupt(bus, slot, intpin, IRQ0+irq);
	registerIRQHandler(irq, pciOnIRQ);
};

static int foundRootBridge = 0;
static ACPI_STATUS pciWalkCallback(ACPI_HANDLE object, UINT32 nestingLevel, void *context, void **returnvalue)
{
	ACPI_DEVICE_INFO *info;
	ACPI_STATUS status = AcpiGetObjectInfo(object, &info);
	
	if (status != AE_OK)
	{
		panic("AcpiGetObjectInfo failed");
	};
	
	if (info->Flags & ACPI_PCI_ROOT_BRIDGE)
	{
		foundRootBridge = 1;
		
		ACPI_BUFFER prtbuf;
		prtbuf.Length = ACPI_ALLOCATE_BUFFER;
		prtbuf.Pointer = NULL;
		
		status = AcpiGetIrqRoutingTable(object, &prtbuf);
		if (status != AE_OK)
		{
			kprintf("WARNING: AcpiGetIrqRoutingTable failed for a root bridge!\n");
			ACPI_FREE(info);
			return AE_OK;
		};
		
		char *scan = (char*) prtbuf.Pointer;
		while (1)
		{
			ACPI_PCI_ROUTING_TABLE *table = (ACPI_PCI_ROUTING_TABLE*) scan;
			if (table->Length == 0)
			{
				break;
			};
			
			uint8_t slot = (uint8_t) (table->Address >> 16);
			if (table->Source == 0)
			{
				// static assignment
				pciMapInterruptFromGSI(0, slot, table->Pin+1, table->SourceIndex);
			}
			else
			{
				// get the PCI Link Object
				ACPI_HANDLE linkObject;
				status = AcpiGetHandle(object, table->Source, &linkObject);
				if (status != AE_OK)
				{
					panic("AcpiGetHandle failed for '%s'", table->Source);
				};
				
				// get the IRQ it is using
				ACPI_BUFFER resbuf;
				resbuf.Length = ACPI_ALLOCATE_BUFFER;
				resbuf.Pointer = NULL;
				
				status = AcpiGetCurrentResources(linkObject, &resbuf);
				if (status != AE_OK)
				{
					panic("AcpiGetCurrentResources failed for '%s'", table->Source);
				};
				
				char *rscan = (char*) resbuf.Pointer;
				int devIRQ = -1;
				while (1)
				{
					ACPI_RESOURCE *res = (ACPI_RESOURCE*) rscan;
					if (res->Type == ACPI_RESOURCE_TYPE_END_TAG)
					{
						break;
					};
					
					if (res->Type == ACPI_RESOURCE_TYPE_IRQ)
					{
						devIRQ = res->Data.Irq.Interrupts[table->SourceIndex];
						break;
					};
					
					if (res->Type == ACPI_RESOURCE_TYPE_EXTENDED_IRQ)
					{
						devIRQ = res->Data.ExtendedIrq.Interrupts[table->SourceIndex];
						break;
					};
					
					rscan += res->Length;
				};
				
				kfree(resbuf.Pointer);
				
				if (devIRQ == -1)
				{
					panic("failed to derive IRQ for device %d from '%s'", (int)slot, table->Source);
				};
				
				//kprintf("DEVICE %d intpin INT%c# -> IRQ %d (from '%s')\n", (int) slot, 'A'+table->Pin, devIRQ, table->Source);
				pciMapInterruptFromIRQ(0, slot, table->Pin+1, devIRQ);
			};
			
			scan += table->Length;
		};
		
		kfree(prtbuf.Pointer);
		//panic("end of tables");
	};
	
	ACPI_FREE(info);
	return AE_OK;
};

void pciInit()
{
	spinlockRelease(&pciLock);
	checkBus(0);
};

void pciInitACPI()
{
	void *retval;
	ACPI_STATUS status = AcpiGetDevices(NULL, pciWalkCallback, NULL, &retval);
	if (status != AE_OK)
	{
		panic("AcpiGetDevices failed");
	};
	
	if (!foundRootBridge)
	{
		panic("failed to find PCI root bridge");
	};
	
	pciIntInitDone = 1;
};

void pciGetDeviceConfig(uint8_t bus, uint8_t slot, uint8_t func, PCIDeviceConfig *config)
{
	spinlockAcquire(&pciLock);
	uint32_t addr = 0;
	uint32_t lbus = (uint32_t) bus;
	uint32_t lslot = (uint32_t) slot;
	uint32_t lfunc = (uint32_t) func;
	addr = (lbus << 16) | (lslot << 11) | (lfunc << 8) | (1 << 31);

	uint32_t *put = (uint32_t*) config;
	int count = 16;

	while (count--)
	{
		outd(PCI_CONFIG_ADDR, addr);
		*put++ = ind(PCI_CONFIG_DATA);
		addr += 4;
	};
	spinlockRelease(&pciLock);
};

ssize_t sys_pcistat(int id, PCIDevice *buffer, size_t bufsize)
{
	spinlockAcquire(&pciLock);
	PCIDevice *dev;
	
	for (dev=pciDevices; dev!=NULL; dev=dev->next)
	{
		if (dev->id == id)
		{
			if (bufsize > sizeof(PCIDevice))
			{
				bufsize = sizeof(PCIDevice);
			};
			
			memcpy(buffer, dev, bufsize);
			spinlockRelease(&pciLock);
			return (ssize_t) bufsize;
		};
	};
	
	spinlockRelease(&pciLock);
	ERRNO = ENOENT;
	return -1;
};

void pciEnumDevices(Module *module, int (*enumerator)(PCIDevice *dev, void *param), void *param)
{
	spinlockAcquire(&pciLock);
	PCIDevice *dev;
	
	for (dev=pciDevices; dev!=NULL; dev=dev->next)
	{
		if (dev->driver == NULL)
		{
			if (enumerator(dev, param))
			{
				dev->driver = module;
				strcpy(dev->driverName, module->name);
			};
		};
	};
	
	spinlockRelease(&pciLock);
};

void pciReleaseDevice(PCIDevice *dev)
{
	spinlockAcquire(&pciLock);
	dev->driver = NULL;
	strcpy(dev->driverName, "null");
	strcpy(dev->deviceName, "Unknown");
	spinlockRelease(&pciLock);
};

void pciInterrupt(int intNo)
{
	if (!pciIntInitDone) return;
	
	PCIDevice *dev;
	for (dev=pciDevices; dev!=NULL; dev=dev->next)
	{
		if (dev->intNo == intNo)
		{
			wcUp(&dev->wcInt);
		};
	};
};

void pciWaitInt(PCIDevice *dev)
{
	wcDown(&dev->wcInt);
};

void pciSetBusMastering(PCIDevice *dev, int enable)
{
	spinlockAcquire(&pciLock);
	uint32_t addr = 0;
	uint32_t lbus = (uint32_t) dev->bus;
	uint32_t lslot = (uint32_t) dev->slot;
	uint32_t lfunc = (uint32_t) dev->func;
	addr = (lbus << 16) | (lslot << 11) | (lfunc << 8) | (1 << 31) | 0x4;
	
	outd(PCI_CONFIG_ADDR, addr);
	uint32_t statcmd = ind(PCI_CONFIG_DATA);
	
	if (enable)
	{
		statcmd |= (1 << 2);
	}
	else
	{
		statcmd &= ~(1 << 2);
	};
	
	outd(PCI_CONFIG_ADDR, addr);
	outd(PCI_CONFIG_DATA, statcmd);
	spinlockRelease(&pciLock);
};
