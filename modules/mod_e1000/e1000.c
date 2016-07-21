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

/**
 * Glidix driver for the Intel 8254x (Gigabit Ethernet) series network cards.
 */

#include <glidix/module.h>
#include <glidix/console.h>
#include <glidix/pci.h>
#include <glidix/string.h>
#include <glidix/netif.h>
#include <glidix/memory.h>
#include <glidix/port.h>
#include <glidix/semaphore.h>
#include <glidix/sched.h>
#include <glidix/dma.h>

#define	E1000_MMIO_SIZE			0x10000		// 16KB

// those must always be powers of 2
#define	NUM_TX_DESC			32
#define	NUM_RX_DESC			32

typedef struct
{
	uint16_t			vendor;
	uint16_t			device;
	const char*			name;
} EDevice;

typedef struct
{
	volatile uint64_t		phaddr;
	volatile uint16_t		len;
	volatile uint8_t		cso;
	volatile uint8_t		cmd;
	volatile uint8_t		sta;
	volatile uint8_t		css;
	volatile uint16_t		special;
} ETXDesc;

typedef struct
{
	volatile uint64_t		phaddr;
	volatile uint16_t		len;
	volatile uint16_t		checksum;
	volatile uint8_t		status;
	volatile uint8_t		errors;
	volatile uint16_t		special;
} ERXDesc;

typedef struct
{
	// must be 2KB to accomodate receiving
	volatile char			data[2048];
} EFrameBuffer;

typedef struct
{
	// those two must be 16-byte-aligned
	volatile ETXDesc		txdesc[NUM_TX_DESC];
	volatile ERXDesc		rxdesc[NUM_RX_DESC];
	
	volatile EFrameBuffer		txbufs[NUM_TX_DESC];
	volatile EFrameBuffer		rxbufs[NUM_RX_DESC];
} ESharedArea;

typedef struct EInterface_
{
	struct EInterface_*		next;
	PCIDevice*			pcidev;
	NetIf*				netif;
	Semaphore			lock;
	int				running;
	Thread*				thread;
	const char*			name;
	uint64_t			mmioAddr;
	DMABuffer			dmaSharedArea;
	Semaphore			semTXCount;
	int				nextTX;
	int				nextWaitingTX;
	int				nextRX;
} EInterface;

static EInterface *interfaces = NULL;
static EInterface *lastIf = NULL;

static EDevice knownDevices[] = {
	{0x8086, 0x100E, "Intel PRO/1000 MT Desktop (82540EM) NIC"},
	{0, 0, NULL}
};

static int e1000_enumerator(PCIDevice *dev, void *ignore)
{
	(void)ignore;
	
	EDevice *nedev;
	for (nedev=knownDevices; nedev->name!=NULL; nedev++)
	{
		if ((nedev->vendor == dev->vendor) && (nedev->device == dev->device))
		{
			strcpy(dev->deviceName, nedev->name);
			EInterface *intf = (EInterface*) kmalloc(sizeof(EInterface));
			intf->next = NULL;
			intf->pcidev = dev;
			intf->netif = NULL;
			intf->name = nedev->name;
			semInit(&intf->lock);
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

static void e1000_send(NetIf *netif, const void *frame, size_t framelen)
{
	EInterface *nif = (EInterface*) netif->drvdata;
	
	// wait for transmit descriptors to be available
	semWait(&nif->semTXCount);
	
	semWait(&nif->lock);
	
	// get buffer index
	int index = nif->nextTX++;
	nif->nextTX &= (NUM_TX_DESC-1);
	
	// fill the buffer
	ESharedArea *sha = (ESharedArea*) dmaGetPtr(&nif->dmaSharedArea);
	memcpy((void*)sha->txbufs[index].data, frame, framelen);
	
	// fill the descriptor
	ESharedArea *shaPhys = (ESharedArea*) dmaGetPhys(&nif->dmaSharedArea);
	sha->txdesc[index].phaddr = (uint64_t) (shaPhys->txbufs[index].data);
	sha->txdesc[index].len = framelen - 4;			// remove CRC
	sha->txdesc[index].cso = 0;
	sha->txdesc[index].cmd = 
		(1 << 3)					// report status
		| (1 << 1)					// insert CRC
		| (1 << 0);					// end of packet
	sha->txdesc[index].sta = 0;
	sha->txdesc[index].css = 0;
	sha->txdesc[index].special = 0;
	
	// write new tail
	volatile uint32_t *regTail = (volatile uint32_t*) (nif->mmioAddr + 0x3818);
	*regTail = (uint32_t) nif->nextTX;
	
	semSignal(&nif->lock);
};

static uint16_t e1000_eeprom_read(EInterface *nif, uint8_t addr)
{
	uint16_t data = 0;
	uint32_t tmp = 0;
	*((volatile uint32_t*)(nif->mmioAddr + 0x0014)) = (1) | ((uint32_t)(addr) << 8);
	
	while (!((tmp = *((uint32_t*)(nif->mmioAddr + 0x0014))) & (1 << 4)));
		
	data = (uint16_t)((tmp >> 16) & 0xFFFF);
	return data;
};

static void e1000_thread(void *context)
{
	EInterface *nif = (EInterface*) context;
	
	// NOTE: we do not need to hold the lock, since we only receive; something that no other thread does,
	// and we do not work on data outside of that. also locking causes onEtherFrame() to fail if we receive
	// an ARP request, since it immediately calls e1000_send().
	while (nif->running)
	{
		pciWaitInt(nif->pcidev);
		
		volatile uint32_t *regICR = (volatile uint32_t*) (nif->mmioAddr + 0x00C0);
		uint32_t icr = *regICR;
		
		if (icr & (1 << 0))
		{
			// transmit descriptor written back
			ESharedArea *sha = (ESharedArea*) dmaGetPtr(&nif->dmaSharedArea);
			if (sha->txdesc[nif->nextWaitingTX].sta & 1)
			{
				// it's done with this descriptor
				if (sha->txdesc[nif->nextWaitingTX].sta & 2)
				{
					// error occured (excessive collisions)
					__sync_fetch_and_add(&nif->netif->numErrors, 1);
				}
				else
				{
					// successful transmission
					__sync_fetch_and_add(&nif->netif->numTrans, 1);
				};
				
				nif->nextWaitingTX = (nif->nextWaitingTX + 1) & (NUM_TX_DESC-1);
				semSignal(&nif->semTXCount);
			};
		};
		
		if ((icr & (1 << 7)) || (icr & (1 << 6)) || (icr & (1 << 4)))
		{
			// packet received
			ESharedArea *sha = (ESharedArea*) dmaGetPtr(&nif->dmaSharedArea);
			int index = nif->nextRX & (NUM_RX_DESC-1);
			
			if (sha->rxdesc[index].status & 1)
			{
				// actually received
				int drop = 0;
				
				size_t len = (size_t) sha->rxdesc[index].len;				
				if ((sha->rxdesc[index].status & (1 << 1)) == 0)
				{
					// not full packet in buffer???
					drop = 1;
				};
				
				if (sha->rxdesc[index].errors != 0)
				{
					drop = 1;
				};
				
				if (drop)
				{
					__sync_fetch_and_add(&nif->netif->numDropped, 1);
				}
				else
				{
					__sync_fetch_and_add(&nif->netif->numRecv, 1);
					onEtherFrame(nif->netif, (const void*)sha->rxbufs[index].data, len+4, ETHER_IGNORE_CRC);
				};
				
				// return the buffer to the NIC
				sha->rxdesc[index].status = 0;
				nif->nextRX = (index + 1) & (NUM_RX_DESC-1);
				volatile uint32_t *regRXTail = (volatile uint32_t*) (nif->mmioAddr + 0x2818);
				*regRXTail = nif->nextRX;
			}
			else
			{
				kprintf("NICE MEME WARNING\n");
			};
		};
	};
};

MODULE_INIT(const char *opt)
{
	kprintf_debug("e1000: enumating Intel Gigabit Ethernet-compatible PCI devices\n");
	pciEnumDevices(THIS_MODULE, e1000_enumerator, NULL);

	kprintf_debug("e1000: creating network interfaces\n");
	EInterface *nif;
	for (nif=interfaces; nif!=NULL; nif=nif->next)
	{
		nif->mmioAddr = (uint64_t) mapPhysMemory((uint64_t) nif->pcidev->bar[0] & ~0xF, E1000_MMIO_SIZE);
		
		NetIfConfig ifconfig;
		memset(&ifconfig, 0, sizeof(NetIfConfig));
		ifconfig.ethernet.type = IF_ETHERNET;
		ifconfig.ethernet.send = e1000_send;

		uint16_t macbits[3];
		int i;
		for (i=0; i<3; i++)
		{
			macbits[i] = e1000_eeprom_read(nif, (uint8_t)i);
		};
		memcpy(ifconfig.ethernet.mac.addr, macbits, 6);

		// set the link UP
		volatile uint32_t *regCtrl = (volatile uint32_t*) nif->mmioAddr;
		(*regCtrl) |= (1 << 6);							// UP

		// zero out the MTA
		volatile uint32_t *regMTA = (volatile uint32_t*) (nif->mmioAddr + 0x5200);
		for (i=0; i<128; i++)
		{
			regMTA[i] = 0;
		};
		
		// enable interrupts, clear existing ones
		volatile uint32_t *regICR = (volatile uint32_t*) (nif->mmioAddr + 0x00C0);
		(void)(*regICR);
		volatile uint32_t *regIMS = (volatile uint32_t*) (nif->mmioAddr + 0x00D0);
		//*regIMS = 0
		//	| (1 << 0)			// TX Descriptor Written Back
		//	| (1 << 7)			// RX interrupt
		//;
		*regIMS = 0x1F6DC;			// all interrupts
		
		// allocate the shared area
		if (dmaCreateBuffer(&nif->dmaSharedArea, sizeof(ESharedArea), 0) != 0)
		{
			panic("failed to allocate shared area for e1000");
		};
		
		// initialize transmit descriptors
		ESharedArea *sha = (ESharedArea*) dmaGetPtr(&nif->dmaSharedArea);
		for (i=0; i<NUM_TX_DESC; i++)
		{
			sha->txdesc[i].phaddr = 0;
			sha->txdesc[i].cmd = 0;
		};
		
		ESharedArea *physShared = (ESharedArea*) dmaGetPhys(&nif->dmaSharedArea);
		uint64_t txBase = (uint64_t) (physShared->txdesc);
		volatile uint32_t *regTDB = (volatile uint32_t*) (nif->mmioAddr + 0x3800);
		regTDB[0] = (uint32_t) txBase;
		regTDB[1] = (uint32_t) (txBase >> 32);
		regTDB[2] = 16 * NUM_TX_DESC;			// length
		
		volatile uint32_t *regTXHead = (volatile uint32_t*) (nif->mmioAddr + 0x3810);
		*regTXHead = 0;
		volatile uint32_t *regTXTail = (volatile uint32_t*) (nif->mmioAddr + 0x3818);
		*regTXTail = NUM_TX_DESC;
		
		volatile uint32_t *regTCTL = (volatile uint32_t*) (nif->mmioAddr + 0x0400);
		*regTCTL = (1 << 1) | (1 << 3);			// enable, pad short packets
		
		// initialize the counter for number of free TX buffers
		semInit2(&nif->semTXCount, NUM_TX_DESC);
		
		// next TX descriptor is zero (the first one)
		nif->nextTX = 0;
		
		// waiting for the NIC to finish with descriptor zero
		nif->nextWaitingTX = 0;
		
		// initialize receive descriptors
		for (i=0; i<NUM_RX_DESC; i++)
		{
			sha->rxdesc[i].phaddr = (uint64_t) physShared->rxbufs[i].data;
			sha->rxdesc[i].status = 0;
		};
		
		// set up receive base and size
		volatile uint32_t *regRDB = (volatile uint32_t*) (nif->mmioAddr + 0x2800);
		uint64_t rxBase = (uint64_t) physShared->rxdesc;
		regRDB[0] = (uint32_t) rxBase;
		regRDB[1] = (uint32_t) (rxBase >> 32);
		regRDB[2] = 16 * NUM_RX_DESC;			// length
		
		// receive head and tail
		volatile uint32_t *regRXHead = (volatile uint32_t*) (nif->mmioAddr + 0x2810);
		*regRXHead = 0;
		volatile uint32_t *regRXTail = (volatile uint32_t*) (nif->mmioAddr + 0x2818);
		*regRXTail = NUM_RX_DESC;
		
		// TODO: let's not be promiscuous
		// a value of 0 for BSEX and BSIZE means 2KB buffers, just like we want
		volatile uint32_t *regRCTL = (volatile uint32_t*) (nif->mmioAddr + 0x0100);
		*regRCTL = 0
			| (1 << 1)				// enable receiver
			| (1 << 3)				// unicast promiscous
			| (1 << 4)				// multicast promiscous
			| (1 << 15)				// accept broadcasts (ff:ff:ff:ff:ff:ff)
			| (1 << 26)				// discard CRC
		;
		
		// next RX descriptor is zero
		nif->nextRX = 0;
		
		// create the network interface for glidix
		nif->netif = CreateNetworkInterface(nif, &ifconfig);
		if (nif->netif == NULL)
		{
			kprintf("e1000: CreateNetworkInterface() failed\n");
		}
		else
		{
			kprintf("e1000: created interface '%s' for '%s'\n", nif->netif->name, nif->name);
		};

		nif->running = 1;
		
		KernelThreadParams pars;
		memset(&pars, 0, sizeof(KernelThreadParams));
		pars.name = "E1000 Interrupt Handler";
		pars.stackSize = DEFAULT_STACK_SIZE;
		nif->thread = CreateKernelThread(e1000_thread, &pars, nif);
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
	kprintf_debug("e1000: deleting interfaces\n");
	EInterface *nif = interfaces;
	while (nif != NULL)
	{
		nif->running = 0;
		wcUp(&nif->pcidev->wcInt);
		ReleaseKernelThread(nif->thread);
		DeleteNetworkInterface(nif->netif);
		
		pciReleaseDevice(nif->pcidev);
		unmapPhysMemory((void*)nif->mmioAddr, E1000_MMIO_SIZE);
		dmaReleaseBuffer(&nif->dmaSharedArea);
		
		EInterface *next = nif->next;
		kfree(nif);
		nif = next;
	};
	
	kprintf_debug("e1000: exiting\n");
	return 0;
};
