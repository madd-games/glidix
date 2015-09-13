/*
	Glidix kernel

	Copyright (c) 2014-2015, Madd Games.
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

static Spinlock pciLock;

static PCIDevice *pciDevices = NULL;
static PCIDevice *lastDevice = NULL;
static int nextPCIID = 0;

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

void pciInit()
{
	spinlockRelease(&pciLock);
	checkBus(0);
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
