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

/**
 * Describes a channel.
 */
typedef struct
{
	uint16_t				base;	// I/O base
	uint16_t				ctrl;	// control base
} IDEChannel;

struct IDEController_;

/**
 * Describes an attached device.
 */
typedef struct
{
	/**
	 * Device type (IDE_ATA or IDE_ATAPI).
	 */
	int					type;
	
	/**
	 * Channel (primary or secondary) and slot (master or slave).
	 */
	int					channel, slot;
	
	/**
	 * The controller.
	 */
	struct IDEController_*			ctrl;
	
	/**
	 * The storage device object.
	 */
	StorageDevice*				sd;
	
	/**
	 * The thread which handles this device.
	 */
	Thread*					thread;
} IDEDevice;

/**
 * Describes an IDE controller.
 */
typedef struct IDEController_
{
	/**
	 * Next controller.
	 */
	struct IDEController_*			next;
	
	/**
	 * The PCI device description associated with the controller.
	 */
	PCIDevice*				pcidev;
	
	/**
	 * Primary and secondary channels.
	 */
	IDEChannel				channels[2];
	
	/**
	 * Lock for accessing devices on this controller.
	 */
	Semaphore				lock;
	
	/**
	 * Devices, and the number of them.
	 */
	IDEDevice				devs[4];
	int					numDevs;
} IDEController;

IDEController *ctrlFirst;
IDEController *ctrlLast;

/**
 * Interrupt flags.
 */
volatile int ideInts[2];

static void ideIrqHandler(int irq)
{
	ideInts[irq-14] = 1;
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
		
		return 1;
	};
	
	return 0;
};

static void ideAtaThread(void *param)
{
	IDEDevice *dev = (IDEDevice*) param;
	
	char zeroes[0x1000];
	memset(zeroes, 0, 0x1000);
	
	while (1)
	{
		SDCommand *cmd = sdPop(dev->sd);
		if (cmd->type == SD_CMD_SIGNAL)
		{
			sdPostComplete(cmd);
			break;
		};
		
		
		if (cmd->type == SD_CMD_READ_TRACK)
		{
			uint64_t i;
			for (i=0; i<8; i++)
			{
				frameWrite(((uint64_t)cmd->block >> 9) + i, zeroes);
			};
		};
		
		sdPostComplete(cmd);
	};
	
	sdHangup(dev->sd);
};

static void ideAtapiThread(void *param)
{
	IDEDevice *dev = (IDEDevice*) param;
	IDEController *ctrl = dev->ctrl;
	int channel = dev->channel;
	
	while (1)
	{
		SDCommand *cmd = sdPop(dev->sd);
		if (cmd->type == SD_CMD_SIGNAL)
		{
			sdPostComplete(cmd);
			break;
		}
		else if (cmd->type == SD_CMD_GET_SIZE)
		{
			panic("ATAPI: SD_CMD_GET_SIZE");
		}
		else if (cmd->type == SD_CMD_EJECT)
		{
			kprintf_debug("eject: not implemented\n");
			sdPostComplete(cmd);
		}
		else if (cmd->type == SD_CMD_READ_TRACK)
		{
			semWait(&dev->ctrl->lock);
			
			// enable interrupts
			outb(dev->ctrl->channels[dev->channel].ctrl + ATA_CREG_CONTROL, 0);
			ideInts[dev->channel] = 0;
			
			// ATAPI packet to send
			uint8_t atapiPacket[12];
			uint64_t index = cmd->pos >> 11;
			atapiPacket[0] = ATAPI_CMD_READ;
			atapiPacket[1] = 0;
			atapiPacket[2] = (index >> 24) & 0xFF;
			atapiPacket[3] = (index >> 16) & 0xFF;
			atapiPacket[4] = (index >> 8) & 0xFF;
			atapiPacket[5] = index & 0xFF;
			atapiPacket[6] = 0;
			atapiPacket[7] = 0;
			atapiPacket[8] = 0;
			atapiPacket[9] = 16;		// 2KB sectors * 16 = 32 KB track
			atapiPacket[10] = 0;
			atapiPacket[11] = 0;
			
			// select drive
			outb(ctrl->channels[channel].base + ATA_IOREG_HDDEVSEL, dev->slot << 4);
			int i;
			for (i=0; i<4; i++)
			{
				inb(ctrl->channels[channel].ctrl + ATA_CREG_CONTROL);
			};
			
			// use PIO mode
			outb(ctrl->channels[channel].base + ATA_IOREG_FEATURES, 0);
			
			// specify the size in bytes (32 KB = 0x8000, so low byte = 0x00, high byte = 0x80)
			outb(ctrl->channels[channel].base + ATA_IOREG_LBA1, 0x00);		// low byte
			outb(ctrl->channels[channel].base + ATA_IOREG_LBA2, 0x80);		// high byte
			
			// send the packet command
			outb(ctrl->channels[channel].base + ATA_IOREG_COMMAND, ATA_CMD_PACKET);
			for (i=0; i<4; i++)
			{
				inb(ctrl->channels[channel].ctrl + ATA_CREG_CONTROL);
			};
			
			// wait for it to stop being busy
			while (inb(ctrl->channels[channel].base + ATA_IOREG_STATUS) & ATA_SR_BSY);
			
			// check for errors
			uint8_t status = inb(ctrl->channels[channel].base + ATA_IOREG_STATUS);
			int error = 0;
			
			if (status & ATA_SR_ERR)
			{
				error = 1;
			};
			
			if (status & ATA_SR_DF)
			{
				error = 1;
			};
			
			if ((status & ATA_SR_DRQ) == 0)
			{
				error = 1;
			};
						
			if (!error)
			{
				// send the packet over
				outsw(ctrl->channels[channel].base + ATA_IOREG_DATA, atapiPacket, 6);

				int k;
				for (k=0; k<4; k++)
				{
					inb(ctrl->channels[channel].ctrl + ATA_CREG_CONTROL);
				};

				// wait for it to stop being busy
				while (inb(ctrl->channels[channel].base + ATA_IOREG_STATUS) & ATA_SR_BSY);

				// wait for interrupt
				while (!ideInts[dev->channel]);
				ideInts[dev->channel] = 0;

				for (k=0; k<4; k++)
				{
					inb(ctrl->channels[channel].ctrl + ATA_CREG_CONTROL);
				};

				// wait for it to stop being busy
				while (inb(ctrl->channels[channel].base + ATA_IOREG_STATUS) & ATA_SR_BSY);
				
				// receive the data (8 pages)
				for (i=0; i<8; i++)
				{
					// 2 sectors per page
					int j;
					uint8_t pagebuf[0x1000];
					
					for (j=0; j<2; j++)
					{
						insw(ctrl->channels[channel].base + ATA_IOREG_DATA, &pagebuf[0x800*j], 0x400);
					};
					
					frameWrite(((uint64_t)cmd->block >> 12) + i, pagebuf);
				};
				
				// wait for it to stop being busy
				while (!ideInts[dev->channel]);
				ideInts[dev->channel] = 0;
				while (inb(ctrl->channels[channel].base + ATA_IOREG_STATUS) & (ATA_SR_BSY | ATA_SR_DRQ));
			};
			
			sdPostComplete(cmd);
			semSignal(&dev->ctrl->lock);
		};
	};
	
	sdHangup(dev->sd);
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
					// don't bother with devices that do not support 48-bit LBA
					continue;
				};
				
				int index = ctrl->numDevs++;
				ctrl->devs[index].type = IDE_ATA;
				ctrl->devs[index].channel = channel;
				ctrl->devs[index].slot = slot;
				ctrl->devs[index].ctrl = ctrl;
				ctrl->devs[index].sd = sdCreate(&sdpars, "IDE UNKNOWN");
				
				KernelThreadParams thpars;
				memset(&thpars, 0, sizeof(KernelThreadParams));
				thpars.stackSize = 0x4000;
				thpars.name = "IDE/ATA drive";
				ctrl->devs[index].thread = CreateKernelThread(ideAtaThread, &thpars, &ctrl->devs[index]);
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
				ctrl->devs[index].sd = sdCreate(&sdpars, "IDE UNKNOWN");
				
				KernelThreadParams thpars;
				memset(&thpars, 0, sizeof(KernelThreadParams));
				thpars.stackSize = 0x4000;
				thpars.name = "IDE/ATAPI drive";
				ctrl->devs[index].thread = CreateKernelThread(ideAtapiThread, &thpars, &ctrl->devs[index]);
			};
		};
	};
	
	// release lock
	semSignal(&ctrl->lock);
};

MODULE_INIT(const char *opt)
{
	registerIRQHandler(14, ideIrqHandler);
	registerIRQHandler(15, ideIrqHandler);
	
	ctrlFirst = ctrlLast = NULL;
	kprintf("sdide: detecting IDE controllers\n");
	pciEnumDevices(THIS_MODULE, ideEnumerator, NULL);
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
			sdSignal(ctrl->devs[i].sd);
			ReleaseKernelThread(ctrl->devs[i].thread);
		};

		pciReleaseDevice(ctrl->pcidev);
		kfree(ctrl);
		
		ctrl = next;
	};
	
	return 0;
};
