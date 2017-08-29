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

#include <glidix/usb.h>
#include <glidix/pci.h>
#include <glidix/sched.h>
#include <glidix/module.h>
#include <glidix/common.h>
#include <glidix/console.h>
#include <glidix/string.h>
#include <glidix/dma.h>
#include <glidix/waitcnt.h>
#include <glidix/memory.h>

#include "ehci.h"

typedef struct EhciCtrl_
{
	struct EhciCtrl_ *next;
	PCIDevice *pcidev;
	EhciCaps *caps;
	EhciRegs *regs;
	DMABuffer dmaPeriodicList;
	WaitCounter wcConnCh;
	Thread *thConnCh;
	int running;
	uint32_t numPorts;
	USBDevice* portDevices[16];
	EhciQueue* qAsync;		/* head of asynchronous schedule; the "device 0 endpoint 0" queue */
	DMABuffer dmaSetAddr;		/* the "set address" buffer */
	EhciSetAddr* reqSetAddr;
	Semaphore lock;
	EhciRequestBuffer *rqCurrent;
	EhciRequestBuffer *rqLast;
	EhciRequestBuffer *rqFirst;
	Thread* thQueue;		/* queue thread */
	WaitCounter wcQueue;
} EhciCtrl;

static EhciCtrl *ehciFirst;
static EhciCtrl *ehciLast;

static EhciQueue* ehciCreateQueue(uint8_t devaddr, uint8_t endpoint, int speed, size_t maxPacketLen, uint32_t flags)
{
	EhciQueue *queue = NEW(EhciQueue);
	queue->next = NULL;
	
	if (dmaCreateBuffer(&queue->dmabuf, sizeof(EhciQH), DMA_32BIT) != 0)
	{
		kfree(queue);
		return NULL;
	};
	
	queue->qh = (EhciQH*) dmaGetPtr(&queue->dmabuf);
	queue->physQH = (uint32_t) dmaGetPhys(&queue->dmabuf);
	
	// zero out the queue head
	memset(queue->qh, 0, sizeof(EhciQH));
	
	// initialize "horptr" to point to nothing, just in case
	queue->qh->horptr = EHCI_HORPTR_TERM;
	queue->qh->endpointInfo = devaddr		/* low 7 bits = device address */
							/* bit 7 = invalidate bit (initialize to 0) */
		| (endpoint << 8)			/* bits 11:8 = endpoint number */
		| (speed << 12)				/* bits 13:12 = endpoint speed */
		| (maxPacketLen << 16)			/* bits 26:16 = maximum packet length */
		| flags;				/* any extra flags */
	queue->qh->endpointCaps = (1 << 30);		// TODO: hubs and stuff
	
	// no transfer in progress
	queue->qh->overlay.horptr = EHCI_HORPTR_TERM;
	queue->qh->overlay.altHorptr = EHCI_HORPTR_TERM;
	
	return queue;
};

static void ehciAddQueue(EhciCtrl *ehci, EhciQueue *current, EhciQueue *newq)
{
	semWait(&ehci->lock);
	newq->prev = current;
	newq->next = current->next;
	current->next = newq;
	
	newq->qh->horptr = current->qh->horptr;
	current->qh->horptr = newq->physQH | EHCI_HORPTR_QH;
	
	semSignal(&ehci->lock);
};

static void* ehciCreateControlPipe(USBDevice *dev, usb_epno_t epno, size_t maxPacketLen)
{
	EhciCtrl *ehci = (EhciCtrl*) dev->data;
	EhciQueue *queue = ehciCreateQueue(dev->addr, epno, EHCI_SPEED_HIGH, maxPacketLen, EHCI_QH_DTC);
	if (queue == NULL) return NULL;
	
	ehciAddQueue(ehci, ehci->qAsync, queue);
	return queue;
};

static int ehciSubmit(USBRequest *urb)
{
	EhciCtrl *ehci = (EhciCtrl*) urb->header.dev->data;
	EhciRequestBuffer *rqbuf = NEW(EhciRequestBuffer);
	rqbuf->next = NULL;
	rqbuf->urb = urb;
	
	switch (urb->header.type)
	{
	case USB_TRANS_CONTROL:
		rqbuf->hwq = (EhciQueue*) urb->control.pipe->data;
		break;
	default:
		kfree(rqbuf);
		return EINVAL;
	};
	
	semWait(&ehci->lock);
	if (ehci->rqFirst == NULL)
	{
		ehci->rqLast = rqbuf;
		ehci->rqFirst = rqbuf;
	}
	else
	{
		ehci->rqLast->next = rqbuf;
		ehci->rqLast = rqbuf;
	};
	semSignal(&ehci->lock);
	
	wcUp(&ehci->wcQueue);
	return 0;
};

static USBCtrl ehciCtrl = {
	.createControlPipe = ehciCreateControlPipe,
	// TODO: deleteControlPipe
	.submit = ehciSubmit,
};

static int ehciEnumerator(PCIDevice *dev, void *ignore)
{
	if ((dev->type == 0x0C03) && (dev->progif == 0x20))
	{
		strcpy(dev->deviceName, "EHCI Controller (USB 2.0)");
		
		EhciCtrl *ehci = NEW(EhciCtrl);
		memset(ehci, 0, sizeof(EhciCtrl));
		ehci->pcidev = dev;
		kprintf("ehci: found EHCI (USB 2.0) controller (%04hX:%04hX)\n", dev->vendor, dev->device);
		
		if (ehciLast == NULL)
		{
			ehciFirst = ehciLast = ehci;
		}
		else
		{
			ehciLast->next = ehci;
			ehciLast = ehci;
		};
		
		return 1;
	};
	
	return 0;
};

static int ehciInt(void *context)
{
	EhciCtrl *ehci = (EhciCtrl*) context;
	uint32_t status = ehci->regs->usbsts;
	ehci->regs->usbsts = status;
	int result = -1;
	
	if (status & EHCI_USBSTS_PORTCH)
	{
		wcUp(&ehci->wcConnCh);
		result = 0;
	};
	
	if (status & EHCI_USBSTS_USBINT)
	{
		wcUp(&ehci->wcQueue);
		result = 0;
	};
	
	return result;
};

static void ehci_connch(void *context)
{
	EhciCtrl *ehci = (EhciCtrl*) context;
	int firstGo = 1;
	while (ehci->running)
	{
		wcDown(&ehci->wcConnCh);
		
		uint32_t i;
		for (i=0; i<ehci->numPorts; i++)
		{
			uint32_t port = ehci->regs->ports[i];
			ehci->regs->ports[i] = port;
			
			if (port & EHCI_PORT_CONNCH || firstGo)
			{
				// NOTE: Always make sure to clear the CONNCH bit, as writing a 1
				// to it will clear it, when we don't want that here
				if (ehci->portDevices[i] != NULL)
				{
					usbHangup(ehci->portDevices[i]);
					ehci->portDevices[i] = NULL;
				};
				
				if (port & EHCI_PORT_CONNECTED)
				{	
					// check power
					if (ehci->caps->hcsparams & EHCI_HCS_PPC)
					{
						// turn on power
						ehci->regs->ports[i] = (ehci->regs->ports[i] | EHCI_PORT_POWER)
							& ~EHCI_PORT_CONNCH;
					};
					
					sleep(100);
					ehci->regs->ports[i] =
						(ehci->regs->ports[i] | EHCI_PORT_RESET) & 
						~(EHCI_PORT_ENABLED | EHCI_PORT_CONNCH);
					sleep(100);
					ehci->regs->ports[i] &= ~(EHCI_PORT_RESET | EHCI_PORT_CONNCH);
					sleep(100);
					
					if ((ehci->regs->ports[i] & EHCI_PORT_ENABLED) == 0)
					{
						uint32_t numcc = (ehci->caps->hcsparams >> 12) & 0xF;
						if (numcc == 0)
						{
							// there are no companion controllers;
							// cannot handle non-high-speed devices
							continue;
						};
						
						// delegate to companion controller
						ehci->regs->ports[i] = (ehci->regs->ports[i] | EHCI_PORT_OWNER)
							& ~EHCI_PORT_CONNCH;
						continue;
					};
					
					// allocate an address for the new device
					usb_addr_t addr = usbAllocAddr();
					if (addr == 0)
					{
						kprintf_debug("ehci: error: out of USB addresses\n");
						continue;
					};
					
					// set up the transaction
					uint32_t transPhys = dmaGetPhys(&ehci->dmaSetAddr);
					memset(ehci->reqSetAddr, 0, sizeof(EhciSetAddr));
					ehci->reqSetAddr->tdSetup.horptr = transPhys + offsetof(EhciSetAddr, tdStatus);
					ehci->reqSetAddr->tdSetup.altHorptr = EHCI_HORPTR_TERM;
					ehci->reqSetAddr->tdSetup.token = 0
						| (8 << 16)			/* 8 bytes to transfer */
						| (EHCI_PID_SETUP << 8)		/* SETUP packet */
						| EHCI_TD_ACTIVE
						| (3 << 10);			/* CERR = 3 */
					ehci->reqSetAddr->tdSetup.bufs[0] = transPhys + offsetof(EhciSetAddr, setupData);
					
					ehci->reqSetAddr->tdStatus.horptr = EHCI_HORPTR_TERM;
					ehci->reqSetAddr->tdStatus.altHorptr = EHCI_HORPTR_TERM;
					ehci->reqSetAddr->tdStatus.token = 0
						| (EHCI_PID_IN << 8)		/* IN packet */
						| EHCI_TD_ACTIVE
						| EHCI_TD_DT
						| (3 << 10);			/* CERR = 3 */
					
					ehci->reqSetAddr->setupData.bmRequestType = 0;
					ehci->reqSetAddr->setupData.bRequest = USB_REQ_SET_ADDRESS;
					ehci->reqSetAddr->setupData.wValue = addr;
					ehci->reqSetAddr->setupData.wIndex = 0;
					ehci->reqSetAddr->setupData.wLength = 0;
					
					// send the request
					ehci->qAsync->qh->overlay.horptr = transPhys;

					// wait for trasmission of the SETUP packet
					uint32_t status;
					while (ehci->reqSetAddr->tdSetup.token & EHCI_TD_ACTIVE);
					
					status = ehci->reqSetAddr->tdSetup.token & 0xFF;
					if (status != 0)
					{
						kprintf("ehci: failed to send SET_ADDRESS command\n");
						
						// disable the port and wait for it to disable
						ehci->regs->ports[i] &= ~(EHCI_PORT_ENABLED | EHCI_PORT_CONNCH);
						while (ehci->regs->ports[i] & EHCI_PORT_ENABLED);
						
						// release the address and give up
						usbReleaseAddr(addr);
						continue;
					};
					
					// wait for ACK
					while (ehci->reqSetAddr->tdStatus.token & EHCI_TD_ACTIVE);
					
					status = ehci->reqSetAddr->tdStatus.token & 0xFF;
					if (status != 0)
					{
						kprintf("ehci: SET_ADDRESS not ACKed\n");
						
						// disable the port and wait for it to disable
						ehci->regs->ports[i] &= ~(EHCI_PORT_ENABLED | EHCI_PORT_CONNCH);
						while (ehci->regs->ports[i] & EHCI_PORT_ENABLED);
						
						// release the address and give up
						usbReleaseAddr(addr);
						continue;
					};
					
					// wait for the device to set address
					sleep(2);
					
					// report detection
					ehci->portDevices[i] = usbCreateDevice(addr, &ehciCtrl, 0, ehci, 2);
				};
			};
		};
		
		firstGo = 0;
	};
};

static void ehci_qthread(void *context)
{
	EhciCtrl *ehci = (EhciCtrl*) context;
	
	while (ehci->running)
	{
		wcDown(&ehci->wcQueue);
	
		// check the status of the current transaction if any
		if (ehci->rqCurrent != NULL)
		{
			EhciTD *tdList = (EhciTD*) &ehci->rqCurrent->hwq->qh[1];
			
			if (ehci->rqCurrent->tdNext != ehci->rqCurrent->tdCount)
			{
				if ((tdList[ehci->rqCurrent->tdNext].token & EHCI_TD_ACTIVE) == 0)
				{
					// no longer active, this one is done
					if (tdList[ehci->rqCurrent->tdNext].token & 0xFF)
					{
						// an error occured
						ehci->rqCurrent->urb->header.status = -1;
					};
					
					ehci->rqCurrent->tdNext++;
				};
			};
			
			// report completion if all transfer descriptors done
			if (ehci->rqCurrent->tdNext == ehci->rqCurrent->tdCount)
			{
				usbPostComplete(ehci->rqCurrent->urb);
				
				// type-specific cleanup
				if (ehci->rqCurrent->urb->header.type == USB_TRANS_CONTROL)
				{
					size_t totalSize = 0;
					USBTransferInfo *info;
					for (info=ehci->rqCurrent->urb->control.packets; info->type!=USB_END; info++)
					{
						totalSize += info->size;
					};
					
					memcpy(ehci->rqCurrent->urb->control.buffer, dmaGetPtr(&ehci->rqCurrent->dmabuf),
						totalSize);
				};
				
				kfree(ehci->rqCurrent);
				ehci->rqCurrent = NULL;
			};
		};
		
		// set up the next request if one is waiting
		if (ehci->rqCurrent == NULL)
		{
			semWait(&ehci->lock);
			EhciRequestBuffer *rq = ehci->rqFirst;
			if (rq != NULL) ehci->rqFirst = rq->next;
			semSignal(&ehci->lock);
			
			// skip if there is no request waiting
			if (rq == NULL) continue;
			
			// type-specific setup
			if (rq->urb->header.type == USB_TRANS_CONTROL)
			{
				size_t totalSize = 0;
				int tdCount = 0;
				USBTransferInfo *info;
				for (info=rq->urb->control.packets; info->type!=USB_END; info++)
				{
					totalSize += info->size;
					tdCount++;
				};
			
				if (dmaCreateBuffer(&rq->dmabuf, totalSize, DMA_32BIT) != 0)
				{
					rq->urb->header.status = -1;
					kfree(rq);
					continue;
				};
			
				// copy buffer contents
				memcpy(dmaGetPtr(&rq->dmabuf), rq->urb->control.buffer, totalSize);
				
				// get the queue
				rq->hwq = (EhciQueue*) rq->urb->control.pipe->data;
				
				// transfer descriptor info
				rq->tdNext = 0;
				rq->tdCount = tdCount;
				
				// set up the transfer descriptors
				EhciTD *tdCurrent = (EhciTD*) &rq->hwq->qh[1];
				uint32_t currentPhys = rq->hwq->physQH + sizeof(EhciQH);
				uint32_t bufCurrent = dmaGetPhys(&rq->dmabuf);
				
				for (info=rq->urb->control.packets; info->type!=USB_END; info++)
				{
					memset(tdCurrent, 0, sizeof(EhciTD));
					if (info[1].type == USB_END)
					{
						tdCurrent->horptr = EHCI_HORPTR_TERM;
					}
					else
					{
						tdCurrent->horptr = currentPhys + sizeof(EhciTD);
					};
					
					currentPhys += sizeof(EhciTD);
					tdCurrent->altHorptr = EHCI_HORPTR_TERM;
					tdCurrent->token = 0
						| (info->size << 16)		/* bytes to transfer */
						| EHCI_TD_ACTIVE		/* active */
						| (3 << 10)			/* CERR = 3 */
						| EHCI_TD_IOC;			/* interrupt on complete */
					
					// type
					switch (info->type)
					{
					case USB_SETUP:
						tdCurrent->token |= (EHCI_PID_SETUP << 8);
						break;
					case USB_IN:
						tdCurrent->token |= (EHCI_PID_IN << 8);
						break;
					case USB_OUT:
						tdCurrent->token |= (EHCI_PID_OUT << 8);
						break;
					};
					
					// data toggle
					if (info->dt)
					{
						tdCurrent->token |= EHCI_TD_DT;
					};
					
					// buffer
					tdCurrent->bufs[0] = bufCurrent;
					bufCurrent += info->size;
				};
				
				// execute operation
				rq->hwq->qh->overlay.horptr = rq->hwq->physQH + sizeof(EhciQH);
				
			};
			
			// mark it as current
			ehci->rqCurrent = rq;
		};
	};
};

MODULE_INIT(const char *opt)
{
	pciEnumDevices(THIS_MODULE, ehciEnumerator, NULL);
	
	kprintf("ehci: initializing controllers\n");
	EhciCtrl *ehci;
	for (ehci=ehciFirst; ehci!=NULL; ehci=ehci->next)
	{
		uint64_t regaddr = ehci->pcidev->bar[0] & ~0xF;
		
		ehci->caps = (EhciCaps*) mapPhysMemory(regaddr, sizeof(EhciCaps));
		if (ehci->caps == NULL)
		{
			panic("ehci: failed to map capabilities into memory");
		};
		
		ehci->regs = (EhciRegs*) mapPhysMemory(regaddr + ehci->caps->caplen, sizeof(ehci->regs));
		if (ehci->regs == NULL)
		{
			panic("ehci: failed to map registers into memory");
		};

		// perform the OS-BIOS hand-off if necessary
		uint32_t capoff = (ehci->caps->hccparams >> 8) & 0xFF;
		while (capoff != 0)
		{
			uint32_t cap = pciRead(ehci->pcidev, capoff);
			uint32_t captype = cap & 0xFF;
			
			kprintf("ehci: [%hhu/%hhu/%hhu] extended capability 0x%08X\n",
				ehci->pcidev->bus, ehci->pcidev->slot, ehci->pcidev->func, cap);
			
			if (captype == 0x01)
			{
				// OS-BIOS hand-off necessary; set the OS-owned bit
				pciWrite(ehci->pcidev, capoff, cap | EHCI_LEGSUP_OWN_OS);
				
				// wait until BIOS hands over control
				while (pciRead(ehci->pcidev, capoff) & EHCI_LEGSUP_OWN_BIOS);
			};

			capoff = (cap >> 8) & 0xFF;
		};
		
		// initialize driver data
		semInit(&ehci->lock);
		wcInit(&ehci->wcConnCh);
		wcUp(&ehci->wcConnCh);
		wcInit(&ehci->wcQueue);
		ehci->numPorts = ehci->caps->hcsparams & 0xF;

		// reset the controller
		ehci->regs->usbcmd = EHCI_USBCMD_HCRESET;
		while (ehci->regs->usbcmd & EHCI_USBCMD_HCRESET);
		
		// enable interrupts
		pciSetIrqHandler(ehci->pcidev, ehciInt, ehci);
		ehci->regs->usbintr = EHCI_USBINTR_PORTCH | EHCI_USBINTR_USBINT;
		
		// periodic list
		if (dmaCreateBuffer(&ehci->dmaPeriodicList, 4096, DMA_32BIT) != 0)
		{
			panic("ehci: failed to allocate periodic list DMA buffer");
		};
		
		uint32_t *periodicList = (uint32_t*) dmaGetPtr(&ehci->dmaPeriodicList);
		int i;
		for (i=0; i<1024; i++)
		{
			periodicList[i] = 1;
		};
		
		ehci->regs->periodicBase = (uint32_t) dmaGetPhys(&ehci->dmaPeriodicList);

		// enable bus mastering
		pciSetBusMastering(ehci->pcidev, 1);

		// run
		ehci->regs->usbcmd |= EHCI_USBCMD_RUN;
		
		// route all ports to EHCI
		ehci->regs->configFlag = 1;

		// set up the asynchronous queue
		ehci->qAsync = ehciCreateQueue(0, 0, EHCI_SPEED_HIGH, 64, EHCI_QH_HEAD | EHCI_QH_DTC);
		if (ehci->qAsync == NULL)
		{
			panic("ehci: failed to allocate queue heads");
		};
		
		// initial asynchronous queue head must point to itself
		ehci->qAsync->qh->horptr = ehci->qAsync->physQH | EHCI_HORPTR_QH;
		ehci->qAsync->prev = NULL;
		ehci->qAsync->next = NULL;
		
		// enable the asynchronous schedule
		ehci->regs->asyncListAddr = ehci->qAsync->physQH;
		ehci->regs->usbcmd |= EHCI_USBCMD_ASYNC_ENABLE;
		while ((ehci->regs->usbsts & EHCI_USBSTS_ASYNC) == 0) __sync_synchronize();	// wait until enabled
		
		// create the "set address" request
		if (dmaCreateBuffer(&ehci->dmaSetAddr, sizeof(EhciSetAddr), DMA_32BIT) != 0)
		{
			panic("ehci: failed to allocate the SET_ADDRESS request area");
		};
		
		ehci->reqSetAddr = (EhciSetAddr*) dmaGetPtr(&ehci->dmaSetAddr);
		
		// interrupt handler thread
		ehci->running = 1;
		KernelThreadParams pars;
		memset(&pars, 0, sizeof(KernelThreadParams));
		pars.name = "EHCI Interrupt Thread";
		pars.stackSize = DEFAULT_STACK_SIZE;
		ehci->thConnCh = CreateKernelThread(ehci_connch, &pars, ehci);
		
		// queue thread
		memset(&pars, 0, sizeof(KernelThreadParams));
		pars.name = "EHCI Queue Thread";
		pars.stackSize = DEFAULT_STACK_SIZE;
		ehci->thQueue = CreateKernelThread(ehci_qthread, &pars, ehci);
	};
	return MODINIT_OK;
};

MODULE_FINI()
{
	kprintf("ehci: releasing resources\n");
	EhciCtrl *ehci;
	for (ehci=ehciFirst; ehci!=NULL; ehci=ehci->next)
	{
		// TODO: do a proper reset to disable interrupts etc
		ehci->running = 0;
		wcUp(&ehci->wcConnCh);
		ReleaseKernelThread(ehci->thConnCh);
		wcUp(&ehci->wcQueue);
		ReleaseKernelThread(ehci->thQueue);
		pciSetBusMastering(ehci->pcidev, 0);
		pciReleaseDevice(ehci->pcidev);
		unmapPhysMemory(ehci->caps, sizeof(EhciCaps));
		unmapPhysMemory(ehci->regs, sizeof(ehci->regs));
		dmaReleaseBuffer(&ehci->dmaPeriodicList);
		dmaReleaseBuffer(&ehci->dmaSetAddr);
		
		// TODO: delete all the queues
	};
	return 0;
};
