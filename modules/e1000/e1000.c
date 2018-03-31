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

/**
 * Glidix driver for the Intel 8254x (Gigabit Ethernet) series network cards.
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

typedef struct EPacket_
{
	struct EPacket_*		next;
	size_t				size;
	uint8_t				data[];
} EPacket;

typedef struct EInterface_
{
	struct EInterface_*		next;
	PCIDevice*			pcidev;
	NetIf*				netif;
	Semaphore			lock;
	volatile int			running;
	Thread*				thread;
	Thread*				qthread;
	const char*			name;
	uint64_t			mmioAddr;
	DMABuffer			dmaSharedArea;
	Semaphore			semTXCount;
	int				nextTX;
	int				nextWaitingTX;
	int				nextRX;
	Semaphore			semQueue;
	Semaphore			semQueueCount;
	EPacket*			qfirst;
	EPacket*			qlast;
	uint64_t			numIntTX;
	uint64_t			numIntRX;
	WaitCounter			wcInts;
} EInterface;

static EInterface *interfaces = NULL;
static EInterface *lastIf = NULL;

static EDevice knownDevices[] = {
	{0x8086, 0x1004, "Intel PRO/1000 T Server (82543GC) NIC"},
	{0x8086, 0x100E, "Intel PRO/1000 MT Desktop (82540EM) NIC"},
	{0x8086, 0x100F, "Intel PRO/1000 MT Server (82545EM) NIC"},
	{0, 0, NULL}
};

static int e1000_int(void *context)
{
	EInterface *nif = (EInterface*) context;
	uint32_t *volatile regICR = (uint32_t*volatile) (nif->mmioAddr + 0x00C0);
	uint32_t icr = *regICR;
	
	if (icr == 0)
	{
		return -1;
	}
	else
	{
		if (icr & (1 << 0))
		{
			// transmit descriptor written back
			__sync_fetch_and_add(&nif->numIntTX, 1);
			wcUp(&nif->wcInts);
		};
		
		if ((icr & (1 << 16)) || (icr & (1 << 7)) || (icr & (1 << 6)) || (icr & (1 << 4)))
		{
			// packet received
			__sync_fetch_and_add(&nif->numIntRX, 1);
			wcUp(&nif->wcInts);
		};
		
		return 0;
	};
};

static void e1000_qthread(void *context)
{
	thnice(NICE_NETRECV);
	
	EInterface *nif = (EInterface*) context;
	while (nif->running)
	{
		semWait(&nif->semQueueCount);
		if (!nif->running) break;
		
		semWait(&nif->semQueue);
		EPacket *packet = nif->qfirst;
		nif->qfirst = packet->next;
		semSignal(&nif->semQueue);

		onEtherFrame(nif->netif, packet->data, packet->size+4, ETHER_IGNORE_CRC);
		kfree(packet);
	};
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
	index &= (NUM_TX_DESC-1);
	
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
	uint32_t *volatile regTail = (uint32_t*volatile) (nif->mmioAddr + 0x3818);
	*regTail = (uint32_t) nif->nextTX;
	
	semSignal(&nif->lock);
};

static uint16_t e1000_eeprom_read(EInterface *nif, uint8_t addr)
{
	uint16_t data = 0;
	uint32_t tmp = 0;
	*((uint32_t*volatile)(nif->mmioAddr + 0x0014)) = (1) | ((uint32_t)(addr) << 8);
	
	while (!((tmp = *((uint32_t*)(nif->mmioAddr + 0x0014))) & (1 << 4)));
		
	data = (uint16_t)((tmp >> 16) & 0xFFFF);
	return data;
};

static void e1000_thread(void *context)
{
	thnice(NICE_NETRECV);
	EInterface *nif = (EInterface*) context;
	
	// NOTE: we do not need to hold the lock, since we only receive; something that no other thread does,
	// and we do not work on data outside of that. also locking causes onEtherFrame() to fail if we receive
	// an ARP request, since it immediately calls e1000_send().
	while (nif->running)
	{
		wcDown(&nif->wcInts);
		
		if (nif->numIntTX > 0)
		{
			// transmit descriptor written back
			__sync_fetch_and_add(&nif->numIntTX, -1);
			ESharedArea *sha = (ESharedArea*) dmaGetPtr(&nif->dmaSharedArea);
			
			uint32_t *volatile regTXHead = (uint32_t*volatile) (nif->mmioAddr + 0x3810);
			while ((sha->txdesc[nif->nextWaitingTX].sta & 1) && ((uint32_t)nif->nextWaitingTX != (*regTXHead)))
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
		
		if (nif->numIntRX > 0)
		{
			// packet received
			__sync_fetch_and_add(&nif->numIntRX, -1);
			ESharedArea *sha = (ESharedArea*) dmaGetPtr(&nif->dmaSharedArea);
			int index = nif->nextRX & (NUM_RX_DESC-1);

			uint32_t *volatile regRXHead = (uint32_t*volatile) (nif->mmioAddr + 0x2810);

			while ((sha->rxdesc[index].status & 1) && ((uint32_t)index != (*regRXHead)))
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
	
					__sync_synchronize();
					EPacket *pkt = (EPacket*) kmalloc(sizeof(EPacket) + len + 4);
					pkt->next = NULL;
					pkt->size = len;
					memcpy(pkt->data, (const void*)sha->rxbufs[index].data, len);
					semWait(&nif->semQueue);
					if (nif->qfirst == NULL)
					{
						nif->qfirst = nif->qlast = pkt;
					}
					else
					{
						nif->qlast->next = pkt;
						nif->qlast = pkt;
					};
					semSignal(&nif->semQueue);
					semSignal(&nif->semQueueCount);
				};
				
				// return the buffer to the NIC
				sha->rxdesc[index].status = 0;
				nif->nextRX = (++index) & (NUM_RX_DESC-1);
				uint32_t *volatile regRXTail = (uint32_t*volatile) (nif->mmioAddr + 0x2818);
				*regRXTail = nif->nextRX;
				__sync_synchronize();
				
				index &= (NUM_RX_DESC-1);
			};
		};
	};
};

static uint64_t e1000_make_filter(MacAddress *mac)
{
	uint64_t filter = 0x8000000000000000;
	filter |= (uint64_t) mac->addr[0];
	filter |= ((uint64_t) mac->addr[1] << 8);
	filter |= ((uint64_t) mac->addr[2] << 16);
	filter |= ((uint64_t) mac->addr[3] << 24);
	filter |= ((uint64_t) mac->addr[4] << 32);
	filter |= ((uint64_t) mac->addr[5] << 40);
	return filter;
};

MODULE_INIT(const char *opt)
{
	kprintf("e1000: enumerating Intel Gigabit Ethernet-compatible PCI devices\n");
	pciEnumDevices(THIS_MODULE, e1000_enumerator, NULL);

	kprintf("e1000: creating network interfaces\n");
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
		uint32_t *volatile regCtrl = (uint32_t*volatile) nif->mmioAddr;
		do
		{
			(*regCtrl) |= (1 << 26);					// reset
			__sync_synchronize();
			sleep(1);
		} while ((*regCtrl) & (1 << 26));
		(*regCtrl) |= (1 << 6);							// UP
		__sync_synchronize();
		
		// zero out the MTA
		uint32_t *volatile regMTA = (uint32_t*volatile) (nif->mmioAddr + 0x5200);
		for (i=0; i<128; i++)
		{
			regMTA[i] = 0;
		};
		
		// set up MAC filtering
		uint64_t addrFilters[16];
		memset(&addrFilters, 0, 8*16);
		addrFilters[0] = e1000_make_filter(&ifconfig.ethernet.mac);
		
		uint32_t *volatile regRA = (uint32_t*volatile) (nif->mmioAddr + 0x5400);
		uint32_t *filtScan = (uint32_t*) addrFilters;
		for (i=0; i<32; i++)
		{
			regRA[i] = filtScan[i];
		};
		__sync_synchronize();

		// allocate the shared area
		if (dmaCreateBuffer(&nif->dmaSharedArea, sizeof(ESharedArea), 0) != 0)
		{
			panic("failed to allocate shared area for e1000");
		};
		
		// initialize transmit descriptors
		ESharedArea *sha = (ESharedArea*) dmaGetPtr(&nif->dmaSharedArea);
		memset(sha, 0, sizeof(ESharedArea));
		
		ESharedArea *physShared = (ESharedArea*) dmaGetPhys(&nif->dmaSharedArea);
		uint64_t txBase = (uint64_t) (physShared->txdesc);
		uint32_t *volatile regTDB = (uint32_t*volatile) (nif->mmioAddr + 0x3800);
		regTDB[0] = (uint32_t) txBase;
		regTDB[1] = (uint32_t) (txBase >> 32);
		regTDB[2] = 16 * NUM_TX_DESC;			// length
		
		uint32_t *volatile regTXHead = (uint32_t*volatile) (nif->mmioAddr + 0x3810);
		*regTXHead = 0;
		uint32_t *volatile regTXTail = (uint32_t*volatile) (nif->mmioAddr + 0x3818);
		*regTXTail = NUM_TX_DESC;
		
		uint32_t *volatile regTCTL = (uint32_t*volatile) (nif->mmioAddr + 0x0400);
		*regTCTL = (1 << 1) | (1 << 3);			// enable, pad short packets
		
		// initialize the counter for number of free TX buffers
		semInit2(&nif->semTXCount, NUM_TX_DESC);
		
		// initialize receive queue
		semInit(&nif->semQueue);
		semInit2(&nif->semQueueCount, 0);
		nif->qfirst = nif->qlast = NULL;
		
		// initialize interrupt counters
		nif->numIntTX = 0;
		nif->numIntRX = 0;
		wcInit(&nif->wcInts);
		
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

		// next RX descriptor is zero
		nif->nextRX = 0;
		pciSetIrqHandler(nif->pcidev, e1000_int, nif);

		// enable interrupts
		uint32_t *volatile regIMS = (uint32_t*volatile) (nif->mmioAddr + 0x00D0);
		*regIMS = 0x1FFFF;			// all interrupts

		// set up receive base and size
		uint32_t *volatile regRDB = (uint32_t*volatile) (nif->mmioAddr + 0x2800);
		uint64_t rxBase = (uint64_t) physShared->rxdesc;
		regRDB[0] = (uint32_t) rxBase;
		regRDB[1] = (uint32_t) (rxBase >> 32);
		regRDB[2] = 16 * NUM_RX_DESC;			// length
		
		// receive head and tail
		uint32_t *volatile regRXHead = (uint32_t*volatile) (nif->mmioAddr + 0x2810);
		*regRXHead = 0;
		uint32_t *volatile regRXTail = (uint32_t*volatile) (nif->mmioAddr + 0x2818);
		*regRXTail = NUM_RX_DESC;
		
		// enable bus mastering before receiving
		pciSetBusMastering(nif->pcidev, 1);
		
		// TODO: let's not be promiscuous
		// a value of 0 for BSEX and BSIZE means 2KB buffers, just like we want
		uint32_t *volatile regRCTL = (uint32_t*volatile) (nif->mmioAddr + 0x0100);
		*regRCTL = 0
			| (1 << 1)				// enable receiver
			| (1 << 4)				// multicast promiscous
			| (1 << 15)				// accept broadcasts (ff:ff:ff:ff:ff:ff)
			| (1 << 26)				// discard CRC
		;
		
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
		
		memset(&pars, 0, sizeof(KernelThreadParams));
		pars.name = "E1000 Queue Thread";
		pars.stackSize = DEFAULT_STACK_SIZE;
		nif->qthread = CreateKernelThread(e1000_qthread, &pars, nif);
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
	kprintf("e1000: deleting interfaces\n");
	EInterface *nif = interfaces;
	while (nif != NULL)
	{
		// TODO: we should really disable the device properly
		nif->running = 0;
		wcUp(&nif->wcInts);
		semSignal(&nif->semQueueCount);
		ReleaseKernelThread(nif->thread);
		ReleaseKernelThread(nif->qthread);
		DeleteNetworkInterface(nif->netif);
		
		pciSetBusMastering(nif->pcidev, 0);
		pciReleaseDevice(nif->pcidev);
		unmapPhysMemory((void*)nif->mmioAddr, E1000_MMIO_SIZE);
		dmaReleaseBuffer(&nif->dmaSharedArea);
		
		EInterface *next = nif->next;
		kfree(nif);
		nif = next;
	};
	
	kprintf("e1000: exiting\n");
	return 0;
};
