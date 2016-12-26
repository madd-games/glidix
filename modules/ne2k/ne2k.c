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

#include <glidix/module.h>
#include <glidix/console.h>
#include <glidix/pci.h>
#include <glidix/string.h>
#include <glidix/netif.h>
#include <glidix/memory.h>
#include <glidix/port.h>
#include <glidix/spinlock.h>
#include <glidix/sched.h>
#include <glidix/idt.h>
#include <glidix/ethernet.h>

#define	RX_BUFFER_START			((16*1024)/256+6)
#define	RX_BUFFER_END			((32*1024)/256)

typedef struct
{
	uint16_t			vendor;
	uint16_t			device;
	const char*			name;
} NeDevice;

typedef struct
{
	uint8_t				rsr;
	uint8_t				next;
	uint16_t			totalSize;
} PACKED NePacketHeader;

static NeDevice knownDevices[] = {
	{0x10EC, 0x8029, "Realtek RTL-8029"},
	{0x1050, 0x0940, "Winbond 89C940"},
	{0x11f6, 0x1401, "Compex RL2000"},
	{0x8e2e, 0x3000, "KTI ET32P2"},
	{0x4a14, 0x5000, "NetVin NV5000SC"},
	{0x1106, 0x0926, "Via 86C926"},
	{0x10bd, 0x0e34, "SureCom NE34"},
	{0x1050, 0x5a5a, "Winbond W89C940F"},
	{0x12c3, 0x0058, "Holtek HT80232"},
	{0x12c3, 0x5598, "Holtek HT80229"},
	{0, 0, NULL}
};

typedef struct NeInterface_
{
	struct NeInterface_*		next;
	PCIDevice*			pcidev;
	NetIf*				netif;
	uint16_t			iobase;
	Spinlock			lock;
	int				running;
	Thread*				thread;
} NeInterface;

static NeInterface *interfaces = NULL;
static NeInterface *lastIf = NULL;

static uint8_t readRegister(NeInterface *nif, uint8_t page, uint8_t index)
{
	// select page
	outb(nif->iobase, page << 6);
	
	// read the register
	return inb(nif->iobase + index);
};

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

static void ne2k_read(NeInterface *nif, uint32_t addr, void *buffer, size_t size)
{
	ne2k_setremaddr(nif, addr);
	ne2k_setremcount(nif, size);
	
	uint8_t *put = (uint8_t*) buffer;
	while (size--)
	{
		*put++ = inb(nif->iobase + 0x10);
	};
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
	
	// remove CRC
	framelen -= 4;

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
	
	// go to START mode
	outb(nif->iobase, 0x02);
	
	spinlockRelease(&nif->lock);
	__sync_fetch_and_add(&netif->numRecv, 1);
	//kprintf_debug("PACKET SENT\n");
};

static void ne2k_thread(void *context)
{
	thnice(NICE_NETRECV);
	
	// buffer to store largest possible frame:
	// max payload (1500) + size of ethernet header (14) + FCS (4)
	// we do this to avoid using kmalloc() and kfree() as this would
	// fragment the heap too much.
	char recvBuffer[1518];
	NePacketHeader packetHeader;
	NeInterface *nif = (NeInterface*) context;
	
	while (nif->running)
	{
		pciWaitInt(nif->pcidev);
		
		spinlockAcquire(&nif->lock);
		uint8_t isr = readRegister(nif, 0, 0x07);
		if (isr & 1)
		{
			// received a frame!
			uint32_t addr = readRegister(nif, 0, 0x03) * 256;	// get address using boundary register
			ne2k_read(nif, addr, &packetHeader, sizeof(NePacketHeader));
			
			size_t frameSize = packetHeader.totalSize-sizeof(NePacketHeader);
			//kprintf_debug("RECEIVING FRAME OF SIZE: %d\n", (int)frameSize);
			
			// copy frame from NIC memory to buffer
			ne2k_read(nif, addr+sizeof(NePacketHeader), recvBuffer, frameSize);
			
			// release the buffer, go to next boundary
			writeRegister(nif, 0, 0x03, packetHeader.next);

			// we append a random uninitialised CRC (4 bytes) to the end, as this is required,
			// but we tell Glidix to ignore it
			onEtherFrame(nif->netif, recvBuffer, frameSize+4, ETHER_IGNORE_CRC);
			
			// acknowledge the receive
			writeRegister(nif, 0, 0x07, 0x01);	
		};
		
		spinlockRelease(&nif->lock);
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
		//outb(nif->iobase + 0x0C, 0x20);		// set to monitor
		//outb(nif->iobase + 0x0D, 0x02);		// and loopback mode.
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
		
		nif->running = 1;
		
		KernelThreadParams pars;
		memset(&pars, 0, sizeof(KernelThreadParams));
		pars.name = "NE2000 Interrupt Handler";
		pars.stackSize = DEFAULT_STACK_SIZE;
		nif->thread = CreateKernelThread(ne2k_thread, &pars, nif);
		
		writeRegister(nif, 0, 0x07, 0xFF);		// clear interrupt status
		writeRegister(nif, 0, 0x0F, 0x01);		// enable reception interrupt
		writeRegister(nif, 0, 0x0D, 0x00);		// normal operation
		
		// set up receive buffer, 6 pages away from the transmit buffer
		writeRegister(nif, 0, 0x01, RX_BUFFER_START);
		writeRegister(nif, 0, 0x02, RX_BUFFER_END);
		
		// enable receiving
		writeRegister(nif, 0, 0x03, RX_BUFFER_START);
		writeRegister(nif, 1, 0x07, RX_BUFFER_START);
		writeRegister(nif, 0, 0x0C, 0x04);
		writeRegister(nif, 0, 0x0E, 0x48);
		
		// start the card
		outb(nif->iobase, 0x02);
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
		// mark the device as not running, then interrupt the thread so that it exits,
		// then release it. after this, we are safe to release the device itself.
		nif->running = 0;
		wcUp(&nif->pcidev->wcInt);
		ReleaseKernelThread(nif->thread);
		
		pciReleaseDevice(nif->pcidev);
		DeleteNetworkInterface(nif->netif);
		
		next = nif->next;
		kfree(nif);
		nif = next;
	};
	
	kprintf("ne2k: exiting\n");
	return 0;
};
