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
#include "port.h"

static AHCIController *firstCtrl;
static AHCIController *lastCtrl;
static int numCtrlFound;

static void ahciInit(AHCIController *ctrl)
{
	// map MMIO regs
	ctrl->regs = mapPhysMemory((uint64_t) (ctrl->pcidev->bar[5] & ~0xF), sizeof(AHCIMemoryRegs));
	
	// Don't use the controller if it does not support 64-bit addressing.
	if ((ctrl->regs->cap & CAP_S64A) == 0)
	{
		kprintf("sdahci: AHCI controller does not support 64-bit addressing, skipping.");
		unmapPhysMemory(ctrl->regs, sizeof(AHCIMemoryRegs));
		return;
	};

	// take ownership of the device from firmware
	ctrl->regs->bohc |= BOHC_OOS;
	while (ctrl->regs->bohc & BOHC_BOS);
	
	// Make sure bus mastering is enabled.
	pciSetBusMastering(ctrl->pcidev, 1);
	ctrl->numPorts = 0;

	// Perform a HBA reset and enable AHCI mode.
	ctrl->regs->ghc = GHC_HR;
	while (ctrl->regs->ghc & GHC_HR);
	ctrl->regs->ghc = GHC_AE;
	
	// Initialize the ports.
	int i;
	for (i=0; i<32; i++)
	{
		if (ctrl->regs->pi & (1 << i))
		{
			portInit(ctrl, i);
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
		for (i=0; i<ctrl->numPorts; i++)
		{
			portRelease(ctrl->ports[i]);
		};
		
		unmapPhysMemory(ctrl->regs, sizeof(AHCIMemoryRegs));
		pciSetBusMastering(ctrl->pcidev, 0);
		pciReleaseDevice(ctrl->pcidev);
		kfree(ctrl);
	};
	
	return 0;
};
