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

// TODO: ensure 64-bit addressing is supported

#include <glidix/module/module.h>
#include <glidix/display/console.h>
#include <glidix/thread/sched.h>
#include <glidix/storage/storage.h>
#include <glidix/util/string.h>
#include <glidix/hw/pci.h>
#include <glidix/util/memory.h>
#include <glidix/hw/port.h>
#include <glidix/util/time.h>
#include <glidix/thread/semaphore.h>
#include <glidix/hw/idt.h>
#include <glidix/hw/dma.h>
#include <glidix/util/time.h>

#include "sdahci.h"
#include "ata.h"
#include "atapi.h"

static AHCIController *firstCtrl;
static AHCIController *lastCtrl;
static int numCtrlFound;

void ahciStopCmd(volatile AHCIPort *port)
{
	port->cmd &= ~CMD_ST;
	while (port->cmd & CMD_CR);
	port->cmd &= ~CMD_FRE;
	while (port->cmd & CMD_FR);
};

void ahciStartCmd(volatile AHCIPort *port)
{
	while (port->cmd & CMD_CR);
	port->cmd |= CMD_FRE;
	port->cmd |= CMD_ST;
};

int ahciIssueCmd(volatile AHCIPort *port)
{
	uint64_t startTime = getNanotime();
	
	port->is = port->is;
	port->ci = 1;
	
	while (1)
	{
		if (getNanotime()-startTime > 8*NANO_PER_SEC)
		{
			// taking longer than 8 seconds
			kprintf("sdahci: timeout; aborting command. IS=0x%08X, SERR=0x%08X, TFD=0x%08X\n", port->is, port->serr, port->tfd);
			ahciStopCmd(port);
			ahciStartCmd(port);
			port->serr = port->serr;
			return EIO;
		};
		
		if (port->is & IS_ERR_FATAL)
		{
			// a fatal error occured
			kprintf("sdahci: fatal error. IS=0x%08X, SERR=0x%08X\n", port->is, port->serr);
			
			ahciStopCmd(port);
			ahciStartCmd(port);
			port->serr = port->serr;
			return EIO;
		};
		
		if ((port->ci & 1) == 0)
		{
			break;
		};
	};
	
	int busy = STS_BSY | STS_DRQ;
	while (port->tfd & busy)
	{
		if (getNanotime()-startTime > 8*NANO_PER_SEC)
		{
			kprintf("sdahci: timeout; aborting command. IS=0x%08X, SERR=0x%08X, TFD=0x%08X\n", port->is, port->serr, port->tfd);
			ahciStopCmd(port);
			ahciStartCmd(port);
			port->serr = port->serr;
			return EIO;
		};
	};
	
	if (port->tfd & STS_ERR)
	{
		return EIO;
	};
	
	return 0;
};

static void ahciInit(AHCIController *ctrl)
{
	// map MMIO regs
	ctrl->regs = mapPhysMemory((uint64_t) (ctrl->pcidev->bar[5] & ~0xF), sizeof(AHCIMemoryRegs));
	
	// take ownership of the device from firmware
	ctrl->regs->bohc |= BOHC_OOS;
	while (ctrl->regs->bohc & BOHC_BOS);
	
	// Make sure bus mastering is enabled.
	pciSetBusMastering(ctrl->pcidev, 1);
	ctrl->numAtaDevices = 0;

	// Perform a HBA reset and enable AHCI mode.
	ctrl->regs->ghc |= GHC_AE | GHC_HR;
	while (ctrl->regs->ghc & GHC_HR);
	
	// Initialize the ports.
	int i;
	for (i=0; i<32; i++)
	{
		if (ctrl->regs->pi & (1 << i))
		{
			ahciStopCmd(&ctrl->regs->ports[i]);
			uint32_t ssts = ctrl->regs->ports[i].ssts;
			
			uint8_t ipm = (ssts >> 8) & 0x0F;
			uint8_t det = ssts & 0x0F;
			
			if (det != SSTS_DET_OK)
			{
				continue;
			};
			
			if (ipm != SSTS_IPM_ACTIVE)
			{
				continue;
			};
			
			uint32_t sig = ctrl->regs->ports[i].sig;
			if (sig == AHCI_SIG_ATA)
			{
				kprintf("sdahci: detected ATA drive on port %d\n", i);
				ataInit(ctrl, i);
			}
			else if (sig == AHCI_SIG_ATAPI)
			{
				kprintf("sdahci: detected ATAPI drive on port %d\n", i);
				atapiInit(ctrl, i);
			}
			else
			{
				kprintf("sdahci: unknown device: signature 0x%08X (port %d)\n", sig, i);
			};
		};
	};
};

static int ahciEnumerator(PCIDevice *dev, void *ignore)
{
	if (dev->type == 0x0106)
	{
		strcpy(dev->deviceName, "AHCI Controller");
		
		AHCIController *ctrl = NEW(AHCIController);
		ctrl->next = NULL;
		ctrl->pcidev = dev;
		
		if (lastCtrl == NULL)
		{
			firstCtrl = lastCtrl = ctrl;
		}
		else
		{
			lastCtrl->next = ctrl;
			lastCtrl = ctrl;
		};
		
		numCtrlFound++;
		return 1;
	};
	
	return 0;
};

MODULE_INIT()
{
	pciEnumDevices(THIS_MODULE, ahciEnumerator, NULL);
	
	kprintf("sdahci: found %d controllers, initializing\n", numCtrlFound);
	AHCIController *ctrl;
	for (ctrl=firstCtrl; ctrl!=NULL; ctrl=ctrl->next)
	{
		ahciInit(ctrl);
	};
	
	return MODINIT_OK;
};

MODULE_FINI()
{
	kprintf("sdahci: removing controllers\n");
	
	AHCIController *ctrl;
	while (firstCtrl != NULL)
	{
		ctrl = firstCtrl;
		firstCtrl = ctrl->next;
		
		int i;
		for (i=0; i<ctrl->numAtaDevices; i++)
		{
			if (ctrl->ataDevices[i]->sd != NULL) sdHangup(ctrl->ataDevices[i]->sd);
			ahciStopCmd(ctrl->ataDevices[i]->port);
			dmaReleaseBuffer(&ctrl->ataDevices[i]->dmabuf);
		};
		
		unmapPhysMemory(ctrl->regs, sizeof(AHCIMemoryRegs));
		pciSetBusMastering(ctrl->pcidev, 0);
		pciReleaseDevice(ctrl->pcidev);
		kfree(ctrl);
	};
	
	return 0;
};
