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

#include <glidix/module.h>
#include <glidix/console.h>
#include <glidix/pci.h>
#include <glidix/string.h>
#include <glidix/netif.h>
#include <glidix/memory.h>
#include <glidix/port.h>
#include <glidix/spinlock.h>
#include <glidix/sched.h>
#include <glidix/dma.h>

#define VIONET_DESC_F_NEXT       	1
#define VIONET_DESC_F_WRITE      	2

#define	VIONET_REG_QADDR		0x08
#define	VIONET_REG_QSIZE		0x0C
#define	VIONET_REG_QSEL			0x0E
#define	VIONET_REG_DEVST		0x12
#define	VIONET_REG_QNOTF		0x10

#define VIONET_ALIGN(x) (((x) + 4095) & ~4095)

#define	VIONET_DEVST_ACK		(1 << 0)
#define	VIONET_DEVST_DRV		(1 << 1)
#define	VIONET_DEVST_OK			(1 << 2)
#define	VIONET_DEVST_FOK		(1 << 3)
#define	VIONET_DEVST_FAIL		(1 << 7)

#define	VIONET_QUEUE_RX			0
#define	VIONET_QUEUE_TX			1

typedef struct
{
	uint64_t			phaddr;
	uint32_t			len;
	uint16_t			flags;
	uint16_t			next;
} VionetBufferDesc;

typedef struct
{
	uint32_t			index;
	uint32_t			len;
} VionetUsedElem;

typedef struct
{
	uint16_t			flags;
	uint16_t			index;
	uint16_t			ring[];
	/* after the ring: uint16_t intIndex */
} VionetAvail;

typedef struct
{
	uint16_t			flags;
	uint16_t			index;
	VionetUsedElem			ring[];
	/* after the ring: uint16_t intIndex */
} VionetUsed;

typedef struct
{
	uint16_t			count;
	VionetBufferDesc*		bufDesc;
	VionetAvail*			avail;
	VionetUsed*			used;
	uint16_t*			availIntIndex;
	uint16_t*			usedIntIndex;
} VionetQueue;

typedef struct
{
	uint8_t				flags;
	uint8_t				seg;
	uint16_t			headlen;
	uint16_t			seglen;
	uint16_t			cstart;
	uint16_t			coff;
	uint16_t			bufcount;
} VionetPacketHeader;

typedef struct VionetInterface_
{
	struct VionetInterface_*	next;
	PCIDevice*			pcidev;
	NetIf*				netif;
	uint16_t			iobase;
	Thread*				thread;
	int				running;
	Spinlock			lock;
	
	/**
	 * The DMA buffers for the RX and TX queue descriptors,
	 * and the RX/TX buffer area.
	 */
	DMABuffer			dmaRX;
	DMABuffer			dmaTX;
	DMABuffer			dmaIO;
	
	/**
	 * Low 4 bits specify which of the 4 TX buffers are in use.
	 */
	uint8_t				txBitmap;
	
	/**
	 * Queue handles. All queue memory is in DMA memory.
	 */
	VionetQueue			rxq;
	VionetQueue			txq;
} VionetInterface;

static void vionet_qinit(VionetQueue *q, uint16_t count, void *p)
{
	uint64_t addr = (uint64_t) p;
	q->count = count;
	q->bufDesc = (VionetBufferDesc*) addr;
	q->avail = (VionetAvail*) (addr + sizeof(VionetBufferDesc) * count);
	q->used = (VionetUsed*) VIONET_ALIGN(((uint64_t) &q->avail->ring[count]));
	//q->availIntIndex = &q->avail->ring[count];
	//q->usedIntIndex = (uint16_t*) &q->used->ring[count];
};

static VionetInterface *interfaces = NULL;
static VionetInterface *lastIf = NULL;

static inline size_t vionet_qdsize(uint16_t qsz)
{ 
	return VIONET_ALIGN(sizeof(VionetBufferDesc)*qsz + sizeof(uint16_t)*(3 + qsz))
		+ VIONET_ALIGN(sizeof(uint16_t)*3 + sizeof(VionetUsed)*qsz);
}

static int vionet_findAvailTXBuffer(VionetInterface *nif)
{
	int i;
	for (i=0; i<4; i++)
	{
		uint8_t mask = (uint8_t) (1 << i);
		if ((nif->txBitmap & mask) == 0)
		{
			break;
		};
	};
	
	return i;
};

// TODO: vionet_send should update the "packet errors" field if necessary,
// and the interrupt handler should update the "packets sent" field.
static void vionet_send(NetIf *netif, const void *frame, size_t framelen)
{
	VionetInterface *nif = (VionetInterface*) netif->drvdata;
	
	spinlockAcquire(&nif->lock);
	int txbuf;
	if ((txbuf = vionet_findAvailTXBuffer(nif)) == 4)
	{
		// there are no available buffers, so 
		spinlockRelease(&nif->lock);
		return;
	};
	
	nif->txBitmap |= (1 << txbuf);
	
	uint64_t spaceBase = dmaGetPhys(&nif->dmaIO);
	
	// load the frame into the buffer
	uint64_t fbufAddr = (uint64_t) dmaGetPtr(&nif->dmaIO) + 8*sizeof(VionetPacketHeader) + (txbuf+4)*1514;
	memset((void*)fbufAddr, 0, 1514);		// never send uninitialized data!
	memcpy((void*)fbufAddr, frame, framelen-4);	// discard CRC field.
	
	// fill in the descriptors
	nif->txq.bufDesc[2*txbuf].phaddr = spaceBase + (txbuf+4)*sizeof(VionetPacketHeader);
	nif->txq.bufDesc[2*txbuf].len = sizeof(VionetPacketHeader);
	nif->txq.bufDesc[2*txbuf].flags = VIONET_DESC_F_NEXT;
	nif->txq.bufDesc[2*txbuf].next = 2*txbuf+1;
	
	// frame buffer
	nif->txq.bufDesc[2*txbuf+1].phaddr = spaceBase + 8*sizeof(VionetPacketHeader) + (txbuf+4)*1514;
	nif->txq.bufDesc[2*txbuf+1].len = framelen - 4;		// discard CRC field
	nif->txq.bufDesc[2*txbuf+1].flags = 0;
	nif->txq.bufDesc[2*txbuf+1].next = 0;
	
	// flush memory
	__sync_synchronize();
	
	// post the buffer to the available ring
	nif->txq.avail->ring[nif->txq.avail->index] = 2*txbuf;
	__sync_synchronize();
	nif->txq.avail->index++;
	__sync_synchronize();
	
	// notify the device that we have updated the TX queue.
	outw(nif->iobase + VIONET_REG_QNOTF, VIONET_QUEUE_TX);
	
	spinlockRelease(&nif->lock);

#if 0
	uint16_t *volatile idx = &nif->txq.used->index;
	while (1)
	{
		kprintf("Index: %d\n", (int) (*idx));
	};
#endif
};

static int vionet_enumerator(PCIDevice *dev, void *ignore)
{
	(void)ignore;
	
	if ((dev->vendor == 0x1AF4) && (dev->device >= 0x1000) && (dev->device <= 0x103F) && (dev->type == 0x0200))
	{
		// found VirtIO network controller
		strcpy(dev->deviceName, "VirtIO Network Controller");
		
		VionetInterface *intf = NEW(VionetInterface);
		intf->next = NULL;
		intf->pcidev = dev;
		intf->netif = NULL;
		intf->iobase = dev->bar[0] & ~3;
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
	
	// not VirtIO-net
	return 0;
};

static void vionet_thread(void *context)
{
	VionetInterface *nif = (VionetInterface*) context;
	
	while (nif->running)
	{
		pciWaitInt(nif->pcidev);
		kprintf("VIONET INTERRUPT!!!\n");
	};
};

MODULE_INIT(const char *opt)
{
	kprintf("vionet: enumerating virtio-net devices\n");
	pciEnumDevices(THIS_MODULE, vionet_enumerator, NULL);
	
	kprintf("vionet: creating network interfaces\n");
	VionetInterface *nif;
	for (nif=interfaces; nif!=NULL; nif=nif->next)
	{
		NetIfConfig ifconfig;
		memset(&ifconfig, 0, sizeof(NetIfConfig));
		ifconfig.ethernet.type = IF_ETHERNET;
		ifconfig.ethernet.send = vionet_send;
		
		int i;
		for (i=0; i<6; i++)
		{
			ifconfig.ethernet.mac.addr[i] = inb(nif->iobase + 0x14 + i);
		};
		
		pciSetBusMastering(nif->pcidev, 1);
		
		outw(nif->iobase + VIONET_REG_DEVST, 0);
		outw(nif->iobase + VIONET_REG_DEVST, VIONET_DEVST_ACK);
		outw(nif->iobase + VIONET_REG_DEVST, VIONET_DEVST_ACK | VIONET_DEVST_DRV);
		
		// find out the size of RX and TX queues.
		outw(nif->iobase + VIONET_REG_QSEL, VIONET_QUEUE_RX);
		uint16_t rxqsize = inw(nif->iobase + VIONET_REG_QSIZE);
		outw(nif->iobase + VIONET_REG_QSEL, VIONET_QUEUE_TX);
		uint16_t txqsize = inw(nif->iobase + VIONET_REG_QSIZE);
		
		if ((rxqsize < 4) || (txqsize < 4))
		{
			panic("vionet: at least 4 RX and 4 TX buffers must be available!");
		};
		
		size_t rxqdsize = vionet_qdsize(rxqsize);
		size_t txqdsize = vionet_qdsize(txqsize);
		
		// allocate the queues as DMA buffers
		int errnum;
		if ((errnum = dmaCreateBuffer(&nif->dmaRX, rxqdsize, DMA_32BIT)) != 0)
		{
			panic("vionet: failed to allocate RX queue! (%d)", errnum);
		};
		
		if ((errnum = dmaCreateBuffer(&nif->dmaTX, txqdsize, DMA_32BIT)) != 0)
		{
			panic("vionet: failed to allocate TX queue! (%d)", errnum);
		};
		
		// zero out the queue descriptors
		memset(dmaGetPtr(&nif->dmaRX), 0, rxqdsize);
		memset(dmaGetPtr(&nif->dmaTX), 0, txqdsize);
		
		// load the queue handles
		vionet_qinit(&nif->rxq, rxqsize, dmaGetPtr(&nif->dmaRX));
		vionet_qinit(&nif->txq, txqsize, dmaGetPtr(&nif->dmaTX));
		
		// allocate the RX/TX space. It starts with 8 packet headers
		// (4 for RX, then 4 for TX), followed by 1514-byte frame
		// buffers (4 for RX and another 4 for TX). The packet headers
		// shall be zeroed out and left like that.
		if ((errnum = dmaCreateBuffer(&nif->dmaIO, 8*sizeof(VionetPacketHeader) + 8*1514, 0)) != 0)
		{
			panic("vionet: failed to allocate RX/TX space! (%d)", errnum);
		};
		
		//panic("\nADDR RX: %a\nADDR TX:%a\nADDR IO: %a\n", dmaGetPhys(&nif->dmaRX), dmaGetPhys(&nif->dmaTX), dmaGetPhys(&nif->dmaIO));
		
		// zero out the RX/TX space then post the 4 RX buffers to the queue.
		memset(dmaGetPtr(&nif->dmaIO), 0, 8*sizeof(VionetPacketHeader) + 8*1514);
		
		uint64_t spaceBase = dmaGetPhys(&nif->dmaIO);
		for (i=0; i<4; i++)
		{
			// packet header buffer
			nif->rxq.bufDesc[2*i].phaddr = spaceBase + i * sizeof(VionetPacketHeader);
			nif->rxq.bufDesc[2*i].len = sizeof(VionetPacketHeader);
			nif->rxq.bufDesc[2*i].flags = VIONET_DESC_F_NEXT;
			nif->rxq.bufDesc[2*i].next = 2*i+1;
			
			// frame buffer
			nif->rxq.bufDesc[2*i+1].phaddr = spaceBase + 8*sizeof(VionetPacketHeader) + i*1514;
			nif->rxq.bufDesc[2*i+1].len = 1514;
			nif->rxq.bufDesc[2*i+1].flags = VIONET_DESC_F_WRITE;
			nif->rxq.bufDesc[2*i+1].next = 0;
			
			// post as available
			nif->rxq.avail->ring[i] = 2*i;
		};
		
		// update the RX ring index
		nif->rxq.avail->index = 4;
		
		// make sure all memory is flushed!
		__sync_synchronize();
		
		// program in the queue addresses
		uint32_t rxphys = (uint32_t) (dmaGetPhys(&nif->dmaRX) >> 12);
		uint32_t txphys = (uint32_t) (dmaGetPhys(&nif->dmaTX) >> 12);
		outw(nif->iobase + VIONET_REG_QSEL, VIONET_QUEUE_RX);
		outd(nif->iobase + VIONET_REG_QADDR, rxphys);
		outw(nif->iobase + VIONET_REG_QSEL, VIONET_QUEUE_TX);
		outd(nif->iobase + VIONET_REG_QADDR, txphys);
		
		nif->netif = CreateNetworkInterface(nif, &ifconfig);
		if (nif->netif == NULL)
		{
			kprintf("vionet: CreateNetworkInterface() failed\n");
		}
		else
		{
			kprintf("vionet: created interface: %s\n", nif->netif->name);
		};

		nif->running = 1;
		spinlockRelease(&nif->lock);
		nif->txBitmap = 0;
		
		// tell the host that we are ready to drive the device, and that we support MAC.
		outw(nif->iobase + 0x04, (1 << 5));
		outw(nif->iobase + VIONET_REG_DEVST, VIONET_DEVST_ACK | VIONET_DEVST_DRV | VIONET_DEVST_OK);
		outw(nif->iobase + VIONET_REG_DEVST, VIONET_DEVST_ACK | VIONET_DEVST_DRV | VIONET_DEVST_OK | VIONET_DEVST_FOK);
		
		// do we have to nitify the device of the queues?
		outw(nif->iobase + VIONET_REG_QNOTF, VIONET_QUEUE_RX);
		outw(nif->iobase + VIONET_REG_QNOTF, VIONET_QUEUE_TX);
		
		KernelThreadParams pars;
		memset(&pars, 0, sizeof(KernelThreadParams));
		pars.name = "VirtIO-net Interrupt Handler";
		pars.stackSize = DEFAULT_STACK_SIZE;
		nif->thread = CreateKernelThread(vionet_thread, &pars, nif);
	};
	
	if (interfaces == NULL)
	{
		return MODINIT_CANCEL;
	};
	
	return MODINIT_OK;
};

MODULE_FINI()
{
	kprintf("vionet: releasing devices\n");
	VionetInterface *nif = interfaces;
	VionetInterface *next = NULL;
	
	while (nif != NULL)
	{
		nif->running = 0;
		wcUp(&nif->pcidev->wcInt);
		ReleaseKernelThread(nif->thread);
		
		pciReleaseDevice(nif->pcidev);
		DeleteNetworkInterface(nif->netif);
		next = nif->next;
		kfree(nif);
		nif = next;
	};
	
	kprintf("vionet: exiting\n");
	return 0;
};
