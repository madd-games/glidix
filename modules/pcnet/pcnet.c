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


#include <glidix/module/module.h>
#include <glidix/display/console.h>
#include <glidix/hw/pci.h>
#include <glidix/util/string.h>
#include <glidix/net/netif.h>
#include <glidix/util/memory.h>
#include <glidix/hw/port.h>
#include <glidix/thread/semaphore.h>
#include <glidix/thread/sched.h>
#include <glidix/hw/dma.h>
#include <glidix/thread/waitcnt.h>

#define NUM_TX_DESC_LOG2 3
#define NUM_RX_DESC_LOG2 3

#define PCNET_RDESC_BAM 	(1 << 4)
#define PCNET_RDESC_LAFM	(1 << 5)
#define PCNET_RDESC_PAM 	(1 << 6)
#define PCNET_RDESC_BPE 	(1 << 7)
#define PCNET_RDESC_ENP 	(1 << 8)
#define PCNET_RDESC_STP 	(1 << 9)
#define PCNET_RDESC_BUFF	(1 << 10)
#define PCNET_RDESC_CRC 	(1 << 11)
#define PCNET_RDESC_OFLO	(1 << 11)
#define PCNET_RDESC_FRAM	(1 << 13)
#define PCNET_RDESC_ERR 	(1 << 14)
#define PCNET_RDESC_OWN 	(1 << 15)

#define PCNET_TDESC_BPE 	(1 << 7)
#define PCNET_TDESC_ENP		(1 << 8)
#define PCNET_TDESC_STP 	(1 << 9)
#define PCNET_TDESC_DEF 	(1 << 10)
#define PCNET_TDESC_ONE 	(1 << 11)
#define PCNET_TDESC_MORELTINT 	(1 << 12)
#define PCNET_TDESC_ADDNOFCS	(1 << 13)
#define PCNET_TDESC_ERR 	(1 << 14)
#define PCNET_TDESC_OWN 	(1 << 15)

#define CSR3_RX_MASK 		(1 << 10)
#define CSR3_TX_MASK 		(1 << 9)
#define CSR3_DONE_MASK 		(1 << 8)

#define CSR0_INIT 		(1 << 0)
#define CSR0_INT_ENABLE 	(1 << 6)
#define CSR0_STRT	 	(1 << 1)
#define CSR0_STOP	 	(1 << 2)
#define	CSR0_INIT_DONE 		(1 << 8)
#define	CSR0_INTR 		(1 << 7)
#define	CSR0_RINT 		(1 << 10)

#define MODE_PROM (1 << 15)

typedef struct
{
	uint16_t			vendor;
	uint16_t			device;
	const char*			name;
} PCnDevice;

typedef struct PCnetInterface_
{
	struct PCnetInterface_*		next;
	PCIDevice*			pcidev;
	const char*			name;
	NetIf*				netif;
	Semaphore			lock;
	DMABuffer			dmaSharedArea;
	Thread*				thread;
	uint16_t			io_base;
	volatile int			running;
	int				nextTX;
	WaitCounter			wcInts;
} PCnetInterface;

typedef struct
{
	uint32_t 			rbandr;
	uint16_t			bcnt;		// ~byteCount
	uint16_t			flags;
	uint16_t			mcnt;
	uint8_t				rpc;
	uint8_t				rcc;
	uint32_t			reserved;
} PCnetRXDesc;

typedef struct
{
	uint32_t 			tbandr;
	uint16_t			bcnt;		// ~byteCount
	uint16_t			flags;
	uint64_t			reserved;
} PCnetTXDesc;

typedef struct
{
	char 				data[1518];
} PCnetBuffer;

typedef struct
{
	uint16_t			mode;
	uint8_t				tlen;
	uint8_t				rlen;
	uint8_t 			mac[6];
	uint16_t			reserved;
	uint8_t				ladr[8];
	uint32_t			rxPhys;
	uint32_t			txPhys;
} InitPacket;

typedef struct
{
	InitPacket 				initPacket;
	volatile PCnetTXDesc			txdesc[1 << NUM_TX_DESC_LOG2];
	volatile PCnetRXDesc			rxdesc[1 << NUM_RX_DESC_LOG2];
	
	volatile PCnetBuffer			txbufs[1 << NUM_TX_DESC_LOG2];
	volatile PCnetBuffer			rxbufs[1 << NUM_RX_DESC_LOG2];
} PCnetSharedArea;

static PCnDevice knownDevices[] = {
	{0x1022, 0x2000, "AMD PCnet-PCI II (Am79C970A)"},
	{0x1022, 0x2001, "PCnet-PCI III"},
	{0, 0, NULL}
};

static PCnetInterface *interfaces = NULL;
static PCnetInterface *lastIf = NULL;

static int pcnet_enumerator(PCIDevice *dev, void *ignore)
{
	(void)ignore;
	
	PCnDevice *nedev;
	for (nedev=knownDevices; nedev->name!=NULL; nedev++)
	{
		if ((nedev->vendor == dev->vendor) && (nedev->device == dev->device))
		{
			strcpy(dev->deviceName, nedev->name);
			PCnetInterface *intf = (PCnetInterface*) kmalloc(sizeof(PCnetInterface));
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

void writeRAP32(uint32_t val, uint16_t io_base)
{
    outd(io_base + 0x14, val);
}
 
void writeRAP16(uint16_t val, uint16_t io_base)
{
    outw(io_base + 0x12, val);
}
 
uint32_t readCSR32(uint32_t csr_no, uint16_t io_base)
{
    writeRAP32(csr_no, io_base);
    return ind(io_base + 0x10);
}
 
uint16_t readCSR16(uint16_t csr_no, uint16_t io_base)
{
    writeRAP32(csr_no, io_base);
    return inw(io_base + 0x10);
}
 
void writeCSR32(uint32_t csr_no, uint32_t val, uint16_t io_base)
{
    writeRAP32(csr_no, io_base);
    outd(io_base + 0x10, val);
}
 
void writeCSR16(uint16_t csr_no, uint16_t val, uint16_t io_base)
{
    writeRAP16(csr_no, io_base);
    outw(io_base + 0x10, val);
}

uint16_t readBCR32(uint32_t csr_no, uint16_t io_base)
{
    writeRAP32(csr_no, io_base);
    return inw(io_base + 0x1c);
}
 
void writeBCR32(uint32_t csr_no, uint32_t val, uint16_t io_base)
{
    writeRAP32(csr_no, io_base);
    outd(io_base + 0x1c, val);
}

static void pcnet_send(NetIf *netif, const void *frame, size_t framelen)
{	
	framelen -= 4;
	PCnetInterface *nif = (PCnetInterface*) netif->drvdata;
	PCnetSharedArea *shared = (PCnetSharedArea*) dmaGetPtr(&nif->dmaSharedArea);
	PCnetSharedArea *areaPhys = (PCnetSharedArea*) dmaGetPhys(&nif->dmaSharedArea);
	semWait(&nif->lock);
	
	while(shared->txdesc[nif->nextTX].flags & PCNET_TDESC_OWN)
	{
	};
	memcpy((void*)shared->txbufs[nif->nextTX].data, frame, framelen);
	shared->txdesc[nif->nextTX].tbandr = (uint32_t) (uint64_t) &areaPhys->txbufs[nif->nextTX];
	shared->txdesc[nif->nextTX].bcnt = 0xF000 | ((-framelen) & 0xFFF);
	shared->txdesc[nif->nextTX].flags |= PCNET_TDESC_OWN | PCNET_TDESC_STP | PCNET_TDESC_ENP;
	nif->nextTX = (nif->nextTX + 1) % (1 << NUM_TX_DESC_LOG2);
	semSignal(&nif->lock);
};

static int pcnet_int(void *context)
{
	PCnetInterface *nif = (PCnetInterface*) context;
	uint32_t csr0 = readCSR32(0, nif->io_base);
	
	if ((csr0 & CSR0_RINT) == 0)
	{
		return -1;
	}
	else
	{
		if (csr0 & CSR0_RINT)
		{
			// packet received
			writeCSR32(0, readCSR32(0, nif->io_base) | CSR0_RINT, nif->io_base);
			wcUp(&nif->wcInts);
		};
		
		return 0;
	};
};

static void pcnet_thread(void *context)
{
	thnice(NICE_NETRECV);
	PCnetInterface *nif = (PCnetInterface*) context;

	while (nif->running)
	{
		wcDown(&nif->wcInts);
		
		int i;
		PCnetSharedArea *shared = (PCnetSharedArea*) dmaGetPtr(&nif->dmaSharedArea);
		for (i=0; i<(1 << NUM_RX_DESC_LOG2); i++)
		{
			if(!(shared->rxdesc[i].flags & PCNET_RDESC_OWN))
			{
				if(!(shared->rxdesc[i].flags & PCNET_TDESC_ERR))
				{
					onEtherFrame(nif->netif, (void*) &shared->rxbufs[i], shared->rxdesc[i].mcnt+4, ETHER_IGNORE_CRC);
				}
				shared->rxdesc[i].flags |= PCNET_TDESC_OWN;
			}
		}
	}
}

MODULE_INIT()
{
	kprintf("PCnet: driver loaded\n");
	pciEnumDevices(THIS_MODULE, pcnet_enumerator, NULL);
	
	PCnetInterface *nif;
	for (nif=interfaces; nif!=NULL; nif=nif->next)
	{
		int i;
		nif->io_base = nif->pcidev->bar[0] & ~3;
		
		ind(nif->io_base + 0x18);
		inw(nif->io_base + 0x14);
		sleep(1);
		
		pciSetIrqHandler(nif->pcidev, pcnet_int, nif);
		
		outd(nif->io_base + 0x10, 0);
		
		pciSetBusMastering(nif->pcidev, 1);

		uint32_t csr58 = readCSR32(58, nif->io_base);
		csr58 &= 0xfff0;
		csr58 |= 2;
		writeCSR32(58, csr58, nif->io_base);
		
		uint32_t bcr2 = readBCR32(2, nif->io_base);
		bcr2 |= 0x2;
		writeBCR32(2, bcr2, nif->io_base);
		
		uint32_t macBits[2];
		macBits[0] = ind(nif->io_base);
		macBits[1] = ind(nif->io_base + 0x04);
		
		NetIfConfig ifconfig;
		memset(&ifconfig, 0, sizeof(NetIfConfig));
		ifconfig.ethernet.type = IF_ETHERNET;
		ifconfig.ethernet.send = pcnet_send;
		
		memcpy(&ifconfig.ethernet.mac, macBits, 6);
		
		wcInit(&nif->wcInts);
		nif->running = 1;
		nif->nextTX = 0;
		
		if (dmaCreateBuffer(&nif->dmaSharedArea, sizeof(PCnetSharedArea), DMA_32BIT) != 0)
		{
			panic("failed to allocate shared area for PCnet");
		};
		
		PCnetSharedArea *shared = (PCnetSharedArea*) dmaGetPtr(&nif->dmaSharedArea);
		memset(shared, 0, sizeof(PCnetSharedArea));
		
		PCnetSharedArea *areaPhys = (PCnetSharedArea*) dmaGetPhys(&nif->dmaSharedArea);
		shared->initPacket.rxPhys = (uint32_t) (uint64_t) areaPhys->rxdesc;
		shared->initPacket.txPhys = (uint32_t) (uint64_t) areaPhys->txdesc;
		shared->initPacket.mode |= MODE_PROM;
		shared->initPacket.tlen = NUM_TX_DESC_LOG2 << 4;
		shared->initPacket.rlen = NUM_RX_DESC_LOG2 << 4;
		memcpy(shared->initPacket.mac, macBits, 6);
		for(i=0; i<6; i++)
		{
			shared->initPacket.ladr[i] = 0;
		};
		
		writeCSR32(3, readCSR32(3, nif->io_base) | CSR3_RX_MASK | CSR3_TX_MASK | CSR3_DONE_MASK, nif->io_base);
		
		uint32_t addr = (uint32_t) (uint64_t) areaPhys;
		uint16_t high = (uint16_t) (addr >> 16);
		uint16_t low = (uint16_t) (addr);
		writeCSR32(1, low, nif->io_base);
		writeCSR32(2, high, nif->io_base);
		
		writeCSR32(0, readCSR32(0, nif->io_base) | CSR0_INIT | CSR0_INT_ENABLE, nif->io_base);
		
		for (i=0; i<(1 << NUM_RX_DESC_LOG2); i++)
		{
			shared->rxdesc[i].rbandr = (uint32_t) (uint64_t) &areaPhys->rxbufs[i];
			shared->rxdesc[i].bcnt = (uint16_t)(-1518);
			shared->rxdesc[i].flags |= PCNET_TDESC_OWN;
		}
		
		KernelThreadParams pars;
		memset(&pars, 0, sizeof(KernelThreadParams));
		pars.name = "PCnet Interrupt Handler";
		pars.stackSize = DEFAULT_STACK_SIZE;
		nif->thread = CreateKernelThread(pcnet_thread, &pars, nif);
		
		while((readCSR32(0, nif->io_base) & CSR0_INIT_DONE) == 0)
		{
		};
		
		writeCSR32(0, ((readCSR32(0, nif->io_base) & ~(CSR0_INIT)) & ~(CSR0_STOP)) | CSR0_STRT, nif->io_base);
		
		nif->netif = CreateNetworkInterface(nif, &ifconfig);
		if (nif->netif == NULL)
		{
			kprintf("pcnet: CreateNetworkInterface() failed\n");
		}
		else
		{
			kprintf("pcnet: created interface '%s' for '%s'\n", nif->netif->name, nif->name);
		};
	};
	
	return MODINIT_OK;
};

MODULE_FINI()
{
	kprintf("PCnet: deleting interfaces\n");
	PCnetInterface *nif = interfaces;
	while (nif != NULL)
	{
		nif->running = 0;
		wcUp(&nif->wcInts);
		ReleaseKernelThread(nif->thread);
		DeleteNetworkInterface(nif->netif);
		
		pciSetBusMastering(nif->pcidev, 0);
		pciReleaseDevice(nif->pcidev);
		dmaReleaseBuffer(&nif->dmaSharedArea);
		
		PCnetInterface *next = nif->next;
		kfree(nif);
		nif = next;
	};
	
	kprintf("PCnet: exiting\n");
	return 0;
};
