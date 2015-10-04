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

#include <glidix/module.h>
#include <glidix/console.h>
#include <glidix/pci.h>
#include <glidix/string.h>
#include <glidix/netif.h>
#include <glidix/memory.h>
#include <glidix/port.h>
#include <glidix/spinlock.h>
#include <glidix/sched.h>

typedef struct
{
	uint16_t			vendor;
	uint16_t			device;
	const char*			name;
} NeDevice;

static NeDevice knownDevices[] = {
	{0x10EC, 0x8029, "Realtek RTL8191SE Wireless LAN 802.11n PCI-E NIC"},
	{0, 0, NULL}
};

typedef struct NeInterface_
{
	struct NeInterface_*		next;
	PCIDevice*			pcidev;
	NetIf*				netif;
	uint16_t			iobase;
	Spinlock			lock;
	Thread*				thread;
} NeInterface;

static NeInterface *interfaces;
static NeInterface *lastIf;

#if 0
static uint8_t readRegister(NeInterface *nif, uint8_t page, uint8_t index)
{
	// select page
	outb(nif->iobase, page << 6);
	
	// read the register
	return inb(nif->iobase + index);
};
#endif

static void writeRegister(NeInterface *nif, uint8_t page, uint8_t index, uint8_t value)
{
	outb(nif->iobase, page << 6);
	outb(nif->iobase + index, value);
};

static void ne2k_setremaddr(NeInterface *nif, uint32_t val)
{
	outb(nif->iobase + 0x08, (uint8_t) (val & 0xFF));
	outb(nif->iobase + 0x09, (uint8_t) ((val >> 8) & 0xFF));
};

static void ne2k_setremcount(NeInterface *nif, size_t val)
{
	outb(nif->iobase + 0x0A, (uint8_t) (val & 0xFF));
	outb(nif->iobase + 0x0B, (uint8_t) ((val >> 8) & 0xFF));
};

static void ne2k_write(NeInterface *nif, uint32_t addr, const void *buffer, size_t size)
{
	ne2k_setremaddr(nif, addr);
	ne2k_setremcount(nif, size);

	outb(nif->iobase + 0x0E, 0x48);		// set byte-wide access
	const uint8_t *scan = (const uint8_t*) buffer;

	outb(nif->iobase, 0x12);		// write DMA + start
	while (size--)
	{
		outb(nif->iobase + 0x10, *scan++);
	};
	
	while ((inb(nif->iobase + 0x07) & 0x40) == 0);
};

static void ne2k_settxcount(NeInterface *nif, size_t count)
{
	outb(nif->iobase + 0x05, (uint8_t) (count & 0xFF));
	outb(nif->iobase + 0x06, (uint8_t) ((count >> 8) & 0xFF));
};

static void ne2k_send(NetIf *netif, const void *frame, size_t framelen)
{
	(void)netif;
	(void)frame;
	(void)framelen;

	kprintf_debug("ne2k_send called (size=%d):\n", (int) framelen);
	EthernetHeader *head = (EthernetHeader*) frame;
	kprintf_debug("\tDST: %x:%x:%x:%x:%x:%x\n", head->dest.addr[0], head->dest.addr[1], head->dest.addr[2],
						head->dest.addr[3], head->dest.addr[4], head->dest.addr[5]);
	kprintf_debug("\tSRC: %x:%x:%x:%x:%x:%x\n", head->src.addr[0], head->src.addr[1], head->src.addr[2],
						head->src.addr[3], head->src.addr[4], head->src.addr[5]);
	kprintf_debug("\tTYP: 0x%x\n", __builtin_bswap16(head->type));
	kprintf_debug("\tCRC: 0x%x\n", ether_checksum(frame, framelen));
	kprintf_debug("\tBuffer address: %a\n", frame);
	
	if (framelen >= 64)
	{
		size_t line, offset;
		for (line=0; line<4; line++)
		{
			for (offset=0; offset<16; offset++)
			{
				uint8_t byte = (*((uint8_t*)frame + (line*16+offset)));
				if (byte < 0x10)
				{
					kprintf_debug("0%x ", (int) byte);
				}
				else
				{
					kprintf_debug("%x ", (int) byte);
				};
			};
			kprintf_debug("\n");
		};
	};
	
	NeInterface *nif = (NeInterface*) netif->drvdata;
	spinlockAcquire(&nif->lock);
	
	// write the frame to the transmit buffer
	ne2k_write(nif, 16*1024, frame, framelen);
	
	// specify the location of transmission buffer in NIC memory
	outb(nif->iobase + 0x04, (16*1024)/256);
	
	// specify the size of the frame
	ne2k_settxcount(nif, framelen);

	// transmit
	outb(nif->iobase, 0x26);
	
	// wait for transmit to complete
	while ((inb(nif->iobase) & 0x04) == 0);
	
	spinlockRelease(&nif->lock);
	kprintf_debug("PACKET SENT\n");
};

static void ne2k_thread(void *context)
{
	NeInterface *nif = (NeInterface*) context;
	
	while (1)
	{
		pciWaitInt(nif->pcidev);
		kprintf_debug("NE2000 interrupt\n");
	};
};

static int ne2k_enumerator(PCIDevice *dev, void *ignore)
{
	(void)ignore;
	
	NeDevice *nedev;
	for (nedev=knownDevices; nedev->name!=NULL; nedev++)
	{
		if ((nedev->vendor == dev->vendor) && (nedev->device == dev->device))
		{
			strcpy(dev->deviceName, nedev->name);
			NeInterface *intf = (NeInterface*) kmalloc(sizeof(NeInterface));
			intf->next = NULL;
			intf->pcidev = dev;
			intf->netif = NULL;
			spinlockRelease(&intf->lock);
			if (lastIf == NULL)
			{
				interfaces = intf;
				lastIf = intf;
			}
			else
			{
				lastIf->next = intf;
				lastIf = intf;
			};
			return 1;
		};
	};
	
	return 0;
};

MODULE_INIT(const char *opt)
{
	kprintf("ne2k: enumerating ne2000-compatible PCI devices\n");
	pciEnumDevices(THIS_MODULE, ne2k_enumerator, NULL);
	
	kprintf("ne2k: creating network interfaces\n");
	NeInterface *nif;
	for (nif=interfaces; nif!=NULL; nif=nif->next)
	{
		nif->iobase = nif->pcidev->bar[0] & ~0x3;
		
		outb(nif->iobase + 0x1F, inb(nif->iobase + 0x1F));
		while ((inb(nif->iobase + 0x07) & 0x80) == 0);
		outb(nif->iobase + 0x07, 0xFF);

		// not sure what is even going on here but linux does this
		// (ne2k-pci) so let's see if it works.
		uint8_t prom[32];
		outb(nif->iobase, (1 << 5) | 1);	// page 0, no DMA, stop
		outb(nif->iobase + 0x0E, 0x49);		// set word-wide access
		outb(nif->iobase + 0x0A, 0);		// clear the count regs
		outb(nif->iobase + 0x0B, 0);
		outb(nif->iobase + 0x0F, 0);		// mask completion IRQ
		outb(nif->iobase + 0x07, 0xFF);
		outb(nif->iobase + 0x0C, 0x20);		// set to monitor
		outb(nif->iobase + 0x0D, 0x02);		// and loopback mode.
		outb(nif->iobase + 0x0A, 32);		// reading 32 bytes
		outb(nif->iobase + 0x0B, 0);		// count high
		outb(nif->iobase + 0x08, 0);		// start DMA at 0
		outb(nif->iobase + 0x09, 0);		// start DMA high
		outb(nif->iobase, 0x0A);		// start the read
		
		int i;
		for (i=0; i<32; i++)
		{
			prom[i] = inb(nif->iobase + 0x10);
		};
		
		for (i=0; i<6; i++)
		{
			writeRegister(nif, 1, 0x01+i, prom[i]);
		};
		
		NetIfConfig ifconfig;
		memset(&ifconfig, 0, sizeof(NetIfConfig));
		ifconfig.ethernet.type = IF_ETHERNET;
		ifconfig.ethernet.send = ne2k_send;
		
		for (i=0; i<6; i++)
		{
			ifconfig.ethernet.mac.addr[i] = prom[i];
		};
		
		nif->netif = CreateNetworkInterface(nif, &ifconfig);
		if (nif->netif == NULL)
		{
			kprintf("ne2k: CreateNetworkInterface() failed\n");
		}
		else
		{
			kprintf("ne2k: created interface: %s\n", nif->netif->name);
		};
		
		KernelThreadParams pars;
		memset(&pars, 0, sizeof(KernelThreadParams));
		pars.name = "NE2000 Interrupt Handler";
		pars.stackSize = DEFAULT_STACK_SIZE;
		
		nif->thread = CreateKernelThread(ne2k_thread, &pars, nif);
		
		writeRegister(nif, 0, 0x07, 0xFF);		// mask interrupts except 1
		writeRegister(nif, 0, 0x0F, 0x01);		// [bit 0 = interrupt on reception of packets with no errors]
		writeRegister(nif, 0, 0x0D, 0);			// normal operation
	};
	
	if (interfaces == NULL)
	{
		// no interfaces available
		return MODINIT_CANCEL;
	};
	
	return MODINIT_OK;
};

MODULE_FINI()
{
	kprintf("ne2k: releasing devices\n");
	NeInterface *nif = interfaces;
	NeInterface *next;
	while (nif != NULL)
	{
		pciReleaseDevice(nif->pcidev);
		next = nif->next;
		kfree(nif);
		nif = next;
	};
	
	return 0;
};
