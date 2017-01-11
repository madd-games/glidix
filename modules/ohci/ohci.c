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

#include <glidix/common.h>
#include <glidix/pci.h>
#include <glidix/module.h>
#include <glidix/usb.h>
#include <glidix/console.h>
#include <glidix/string.h>
#include <glidix/dma.h>
#include "ohci.h"

static OHCIRegs*      ohciRegs;
static DMABuffer      dmaComm;
static DMABuffer      dmaDescriptors;
static PCIDevice*     ohciPCI;
static uint32_t       ohciNumPorts;
static OHCIPort       ohciPorts[15];
static Thread*        ohciThread;
static int            stopping;
static OHCICommPage*  commPage;

static int ohciEnumerator(PCIDevice *pcidev, void *param)
{
	if ((pcidev->type == 0x0C03) && (pcidev->progif == 0x10))
	{		
		uint64_t regsPhysAddr = (uint64_t)pcidev->bar[0] & ~0xF;
		ohciRegs = (OHCIRegs*) mapPhysMemory(regsPhysAddr, sizeof(OHCIRegs));

		if (dmaCreateBuffer(&dmaComm, sizeof(OHCICommPage), DMA_32BIT) != 0)
		{
			kprintf("ohci: failed to allocate communications page; aborting\n");
			unmapPhysMemory(ohciRegs, sizeof(OHCIRegs));
			return 0;
		};
		
		if (dmaCreateBuffer(&dmaDescriptors, 128*16*32, DMA_32BIT) != 0)
		{
			kprintf("ohci: failed to allocate DMA buffer for descriptors\n");
			unmapPhysMemory(ohciRegs, sizeof(OHCIRegs));
			dmaReleaseBuffer(&dmaComm);
			return 0;
		};

		strcpy(pcidev->deviceName, "USB OHCI Controller");
		ohciPCI = pcidev;
		
		return 1;
	};
	
	return 0;
};

static void scanPorts()
{
	uint32_t i;
	for (i=0; i<ohciNumPorts; i++)
	{
		uint32_t connected = ohciRegs->hcRhPortStatus[i] & HC_PORT_CCS;
		OHCIPort *port = &ohciPorts[i];
		
		if ((connected) && (port->usbdev == NULL))
		{
			// something connected to the port
			ohciRegs->hcRhPortStatus[i] = HC_PORT_PRS;
			__sync_synchronize();
			while (ohciRegs->hcRhPortStatus[i] & HC_PORT_PRS) __sync_synchronize();
			
			port->usbdev = usbCreateDevice();
			if (port->usbdev == NULL)
			{
				kprintf("ohci: WARNING: usbCreateDevice() failed!\n");
				continue;
			};
			
			port->usbdev->data = port;
			
			commPage->initReq.wValue = port->usbdev->faddr;
			commPage->initTD.tdFlags = HC_TD_DP_SETUP;
			commPage->initTD.tdBuffer = (uint32_t) (dmaGetPhys(&dmaComm) + offsetof(OHCICommPage, initReq));
			commPage->initTD.tdNext = 0;
			commPage->initTD.tdBufferEnd = commPage->initTD.tdBuffer + 7;	/* last byte, not size! */
			uint32_t tdPhys = (uint32_t) (dmaGetPhys(&dmaComm) + offsetof(OHCICommPage, initTD));
			commPage->initED.edHeadPCH = tdPhys;
			
			//kprintf("waiting for SET_ADDRESS to finish\n");
			//while ((ohciRegs->hcInterruptStatus & HC_INT_WDH) == 0)
			//{
			//	//kprintf("STATUS: 0x%08x\n", ohciRegs->hcInterruptStatus);
			//	__sync_synchronize();
			//};
			//if (commPage->hcca.hccaDoneHead != tdPhys)
			//{
			//	panic("hccaDoneHead is wrong!");
			//};
			//panic("hccaDoneHead is right!");
			ohciRegs->hcInterruptStatus = HC_INT_WDH;
			__sync_synchronize();
			
			usbPostDevice(port->usbdev);
		}
		else if ((!connected) && (port->usbdev != NULL))
		{
			// something disconnected from the port
			usbDisconnect(port->usbdev);
			port->usbdev = NULL;
		};
	};
};

static void ohciThreadFunc(void *data)
{
	(void)data;
	
	while (1)
	{
		pciWaitInt(ohciPCI);
		if (stopping) return;
		
		if (ohciRegs->hcInterruptStatus & HC_INT_RHSC)
		{
			scanPorts();
			ohciRegs->hcInterruptStatus = HC_INT_RHSC;
		};
	};
};

MODULE_INIT()
{
	ohciRegs = NULL;
	kprintf("ohci: scanning for USB OHCI controller\n");
	pciEnumDevices(THIS_MODULE, ohciEnumerator, NULL);
	
	if (ohciRegs != NULL)
	{
		kprintf("ohci: initializing controller\n");
		
		commPage = (OHCICommPage*) dmaGetPtr(&dmaComm);
		memset(commPage, 0, sizeof(OHCICommPage));
		pciSetBusMastering(ohciPCI, 1);
				
		// if the SMM driver is active, disable it
		if (ohciRegs->hcControl & HC_CTRL_IR)
		{
			ohciRegs->hcCommandStatus = HC_CMD_OCR;
			while (ohciRegs->hcControl & HC_CTRL_IR) __sync_synchronize();
		};
		
		// if the BIOS is active, disable it
		if (HC_CTRL_HCFS(ohciRegs->hcControl) != HC_USB_RESET)
		{
			if (HC_CTRL_HCFS(ohciRegs->hcControl) != HC_USB_OPERATIONAL)
			{
				ohciRegs->hcControl = (ohciRegs->hcControl & 0xFFFFFF3F) | (HC_USB_RESUME << 6);
				/* TODO: "wait minimum time for assertion of resume/reset" ??? */
			};
		};
		__sync_synchronize();
		
		commPage->initED.edFlags = HC_ED_MPS(8);
		commPage->initED.edTailP = 0;
		commPage->initED.edHeadPCH = 0;
		commPage->initED.edNext = 0;
		
		commPage->initTD.tdFlags = HC_TD_DP_SETUP;
		commPage->initTD.tdBuffer = (uint32_t) (dmaGetPhys(&dmaComm) + offsetof(OHCICommPage, initReq));
		commPage->initTD.tdNext = 0;
		commPage->initTD.tdBufferEnd = commPage->initTD.tdBuffer + 7;	/* last byte, not size! */
		
		commPage->initReq.bmRequestType = 0;
		commPage->initReq.bRequest = USB_SET_ADDRESS;
		commPage->initReq.wValue = 0;
		commPage->initReq.wIndex = 0;
		commPage->initReq.wLength = 0;
		
		uint32_t interval = ohciRegs->hcFmInterval;
		ohciRegs->hcCommandStatus = HC_CMD_HCR;
		__sync_synchronize();
		ohciRegs->hcFmInterval = interval;
		__sync_synchronize();
		ohciRegs->hcHCCA = (uint32_t) (dmaGetPhys(&dmaComm) + offsetof(OHCICommPage, hcca));
		__sync_synchronize();
		ohciRegs->hcInterruptStatus = ohciRegs->hcInterruptStatus;
		__sync_synchronize();
		ohciRegs->hcInterruptEnable = HC_INT_WDH | HC_INT_RHSC | HC_INT_MIE;
		__sync_synchronize();
		
		ohciRegs->hcControlHeadED = (uint32_t) (dmaGetPhys(&dmaComm) + offsetof(OHCICommPage, initED));
		__sync_synchronize();
		ohciRegs->hcControl = (ohciRegs->hcControl & 0xFFFFFF3F) | (HC_USB_OPERATIONAL << 6) | HC_CTRL_CLE;
		__sync_synchronize();
		ohciRegs->hcPeriodicStart = interval * 100 / 90;
		__sync_synchronize();
		
		ohciNumPorts = ohciRegs->hcRhDescriptorA & 0xFF;
		if ((ohciNumPorts < 1) || (ohciNumPorts > 15))
		{
			kprintf("ohci: invalid number of ports (%u)! assuming 15\n", ohciNumPorts);
			ohciNumPorts = 15;
		};
		
		// initialize our port information
		uint32_t i;
		for (i=0; i<ohciNumPorts; i++)
		{
			ohciPorts[i].index = i;
			ohciPorts[i].usbdev = NULL;
		};
		
		scanPorts();
		stopping = 0;
		
		// start the interrupt-handling thread
		KernelThreadParams pars;
		memset(&pars, 0, sizeof(KernelThreadParams));
		pars.stackSize = DEFAULT_STACK_SIZE;
		pars.name = "OHCI Interrupt Handler";
		ohciThread = CreateKernelThread(ohciThreadFunc, &pars, NULL);
		
		return MODINIT_OK;
	};
	
	return MODINIT_CANCEL;
};

MODULE_FINI()
{
	if (ohciRegs != NULL)
	{
		stopping = 1;
		wcUp(&ohciPCI->wcInt);
		ReleaseKernelThread(ohciThread);
		
		uint32_t i;
		for (i=0; i<ohciNumPorts; i++)
		{
			if (ohciPorts[i].usbdev != NULL)
			{
				usbDisconnect(ohciPorts[i].usbdev);
			};
		};
		
		pciSetBusMastering(ohciPCI, 0);
		unmapPhysMemory(ohciRegs, sizeof(OHCIRegs));
		dmaReleaseBuffer(&dmaComm);
		dmaReleaseBuffer(&dmaDescriptors);
	};
	
	return 0;
};
