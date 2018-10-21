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

#include <glidix/util/common.h>
#include <glidix/module/module.h>
#include <glidix/display/console.h>
#include <glidix/hw/port.h>
#include <glidix/util/string.h>
#include <glidix/util/memory.h>
#include <glidix/hw/pci.h>
#include <glidix/storage/storage.h>
#include <glidix/thread/semaphore.h>
#include <glidix/util/time.h>
#include <glidix/thread/sched.h>
#include <glidix/thread/waitcnt.h>
#include <glidix/hw/idt.h>
#include <glidix/hw/pagetab.h>

#include "sdide.h"
#include "atapi.h"
#include "ata.h"

IDEController *ctrlFirst;
IDEController *ctrlLast;
int numCtrlFound;

/**
 * Interrupt flags.
 */
volatile int ideInts[2];

static void ideIrqHandler(int irq)
{
	ideInts[irq-14] = 1;
};

static void ideInit(IDEController *ctrl)
{
	// initialize and acquire the lock
	semInit(&ctrl->lock);
	semWait(&ctrl->lock);
	
	// detect base registers
	if (ctrl->pcidev->bar[0] == 0) ctrl->channels[ATA_PRIMARY].base = 0x1F0;
	else ctrl->channels[ATA_PRIMARY].base = ctrl->pcidev->bar[0] & 0xFFFFFFFC;
	
	if (ctrl->pcidev->bar[1] == 0) ctrl->channels[ATA_PRIMARY].ctrl = 0x3F6;
	else ctrl->channels[ATA_PRIMARY].ctrl = ctrl->pcidev->bar[1] & 0xFFFFFFFC;
	
	if (ctrl->pcidev->bar[2] == 0) ctrl->channels[ATA_SECONDARY].base = 0x170;
	else ctrl->channels[ATA_SECONDARY].base = ctrl->pcidev->bar[2] & 0xFFFFFFFC;
	
	if (ctrl->pcidev->bar[3] == 0) ctrl->channels[ATA_SECONDARY].ctrl = 0x376;
	else ctrl->channels[ATA_SECONDARY].ctrl = ctrl->pcidev->bar[3] & 0xFFFFFFFC;
	
	// disable interrupts for both channels
	outb(ctrl->channels[ATA_PRIMARY].ctrl + ATA_CREG_CONTROL, ATA_CTRL_NIEN);
	outb(ctrl->channels[ATA_SECONDARY].ctrl + ATA_CREG_CONTROL, ATA_CTRL_NIEN);
	
	// detect drives
	int channel, slot;
	for (channel=0; channel<2; channel++)
	{
		for (slot=0; slot<2; slot++)
		{
			// select slot
			outb(ctrl->channels[channel].base + ATA_IOREG_HDDEVSEL, 0x0A | (slot << 4));
			sleep(1);		// wait 1ms for selection
			
			// send the IDENTIFY command
			outb(ctrl->channels[channel].base + ATA_IOREG_COMMAND, ATA_CMD_IDENTIFY);
			sleep(1);
			
			// see if any device at all is attached
			if (inb(ctrl->channels[channel].base + ATA_IOREG_STATUS) == 0)
			{
				// no device here
				continue;
			};
			
			// wait until command completes, check for error
			int error = 0;
			while (1)
			{
				uint8_t status = inb(ctrl->channels[channel].base + ATA_IOREG_STATUS);
				if (status & ATA_SR_ERR)
				{
					error = 1;
					break;
				};
				
				if (!(status & ATA_SR_BSY) && (status & ATA_SR_DRQ))
				{
					break;
				};
			};
			
			// if an error occured, this might be an ATAPI device
			int type = IDE_ATA;
			if (error)
			{
				uint8_t cl = inb(ctrl->channels[channel].base + ATA_IOREG_LBA1);
				uint8_t ch = inb(ctrl->channels[channel].base + ATA_IOREG_LBA2);
				
				if ((cl == 0x14) && (ch == 0xEB))
				{
					type = IDE_ATAPI;
				}
				else if ((cl == 0x69) && (ch == 0x96))
				{
					type = IDE_ATAPI;
				}
				else
				{
					continue;	// unknown type, possibly nothing connected
				};
				
				outb(ctrl->channels[channel].base + ATA_IOREG_COMMAND, ATA_CMD_IDENTIFY_PACKET);
				sleep(1);
			};
			
			// read the identification data
			unsigned char identBuf[512];
			int count = 128;
			uint32_t *put = (uint32_t*) identBuf;
			while (count--)
			{
				*put++ = ind(ctrl->channels[channel].base + ATA_IOREG_DATA);
			};

			int k;
			char model[41];
			for (k=0; k<40; k+=2)
			{
				model[k] = identBuf[ATA_IDENT_MODEL + k + 1];
				model[k + 1] = identBuf[ATA_IDENT_MODEL + k];
			};
			
			model[40] = 0;

			char *check = &model[39];
			while (*check == ' ')
			{
				if (check == model) break;
				*check-- = 0;
			};

			if (type == IDE_ATA)
			{
				SDParams sdpars;
				sdpars.flags = 0;
				sdpars.blockSize = 512;

				uint32_t commandSets = *((uint32_t*)(identBuf + ATA_IDENT_COMMANDSETS));
				
				if (commandSets & ATA_CMDSET_LBA_EXT)
				{
					uint32_t blocks = *((uint32_t*)(identBuf + ATA_IDENT_MAX_LBA_EXT));
					sdpars.totalSize = (uint64_t)blocks << 9;
				}
				else
				{
					uint32_t blocks = *((uint32_t*)(identBuf + ATA_IDENT_MAX_LBA));
					sdpars.totalSize = (uint64_t)blocks << 9;
				};

				int index = ctrl->numDevs++;
				ctrl->devs[index].type = IDE_ATA;
				ctrl->devs[index].channel = channel;
				ctrl->devs[index].slot = slot;
				ctrl->devs[index].ctrl = ctrl;
				ctrl->devs[index].sd = sdCreate(&sdpars, model, &ataOps, &ctrl->devs[index]);
			}
			else if (type == IDE_ATAPI)
			{
				// report it to the kernel
				SDParams sdpars;
				sdpars.flags = SD_READONLY;
				sdpars.blockSize = 2048;
				sdpars.totalSize = 0;
				
				int index = ctrl->numDevs++;
				ctrl->devs[index].type = IDE_ATAPI;
				ctrl->devs[index].channel = channel;
				ctrl->devs[index].slot = slot;
				ctrl->devs[index].ctrl = ctrl;
				ctrl->devs[index].sd = sdCreate(&sdpars, model, &atapiOps, &ctrl->devs[index]);
			};
		};
	};
	
	// release lock
	semSignal(&ctrl->lock);
};

static int ideEnumerator(PCIDevice *dev, void *param)
{
	if (dev->type == 0x101)
	{
		strcpy(dev->deviceName, "IDE Controller");
		
		IDEController *ctrl = NEW(IDEController);
		memset(ctrl, 0, sizeof(IDEController));
		ctrl->pcidev = dev;
		
		if (ctrlLast == NULL)
		{
			ctrlFirst = ctrlLast = ctrl;
		}
		else
		{
			ctrlLast->next = ctrl;
			ctrlLast = ctrl;
		};
		
		numCtrlFound++;
		return 1;
	};
	
	return 0;
};

MODULE_INIT(const char *opt)
{
	registerIRQHandler(14, ideIrqHandler);
	registerIRQHandler(15, ideIrqHandler);
	
	ctrlFirst = ctrlLast = NULL;
	kprintf("sdide: detecting IDE controllers\n");
	pciEnumDevices(THIS_MODULE, ideEnumerator, NULL);
	kprintf("sdide: found %d controllers\n", numCtrlFound);
	if (ctrlFirst == NULL) return MODINIT_CANCEL;
	
	IDEController *ctrl;
	for (ctrl=ctrlFirst; ctrl!=NULL; ctrl=ctrl->next)
	{
		ideInit(ctrl);
	};
	
	return MODINIT_OK;
};

MODULE_FINI()
{
	kprintf("sdide: releasing IDE controllers\n");
	
	IDEController *ctrl = ctrlFirst;
	while (ctrl != NULL)
	{
		IDEController *next = ctrl->next;
		
		int i;
		for (i=0; i<ctrl->numDevs; i++)
		{
			sdHangup(ctrl->devs[i].sd);
		};

		pciReleaseDevice(ctrl->pcidev);
		kfree(ctrl);
		
		ctrl = next;
	};

	return 0;
};
