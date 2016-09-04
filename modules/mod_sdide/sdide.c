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

#include <glidix/module.h>
#include <glidix/console.h>
#include <glidix/sched.h>
#include <glidix/storage.h>
#include <glidix/string.h>
#include <glidix/pci.h>
#include <glidix/memory.h>
#include <glidix/port.h>
#include <glidix/time.h>
#include <glidix/semaphore.h>
#include <glidix/idt.h>
#include "sdide.h"

static Semaphore ideSem;
static volatile ATOMIC(uint8_t) ideWaitIRQ[2];

static void ideHandleIRQ(int irq)
{
	ideWaitIRQ[irq-ATA_IRQ_PRIMARY] = 1;
	__sync_synchronize();
};

static void ideWriteReg(IDEController *ctrl, uint8_t channel, uint8_t reg, uint8_t data)
{
	if (reg > 0x07 && reg < 0x0C)
		ideWriteReg(ctrl, channel, ATA_REG_CONTROL, 0x80 | ctrl->channels[channel].nIEN);
	if (reg < 0x08)
		outb(ctrl->channels[channel].base  + reg - 0x00, data);
	else if (reg < 0x0C)
		outb(ctrl->channels[channel].base  + reg - 0x06, data);
	else if (reg < 0x0E)
		outb(ctrl->channels[channel].ctrl  + reg - 0x0A, data);
	else if (reg < 0x16)
		outb(ctrl->channels[channel].bmide + reg - 0x0E, data);
	if (reg > 0x07 && reg < 0x0C)
		ideWriteReg(ctrl, channel, ATA_REG_CONTROL, ctrl->channels[channel].nIEN);
};

static uint8_t ideReadReg(IDEController *ctrl, uint8_t channel, uint8_t reg)
{
	uint8_t result;
	if (reg > 0x07 && reg < 0x0C)
		ideWriteReg(ctrl, channel, ATA_REG_CONTROL, 0x80 | ctrl->channels[channel].nIEN);
	if (reg < 0x08)
		result = inb(ctrl->channels[channel].base + reg - 0x00);
	else if (reg < 0x0C)
		result = inb(ctrl->channels[channel].base  + reg - 0x06);
	else if (reg < 0x0E)
		result = inb(ctrl->channels[channel].ctrl  + reg - 0x0A);
	else if (reg < 0x16)
		result = inb(ctrl->channels[channel].bmide + reg - 0x0E);
	if (reg > 0x07 && reg < 0x0C)
		ideWriteReg(ctrl, channel, ATA_REG_CONTROL, ctrl->channels[channel].nIEN);
	return result;
};

static void ideReadBuffer(IDEController *ctrl, uint8_t channel, uint8_t reg, void *buffer, uint32_t count)
{
	if (reg > 0x07 && reg < 0x0C)
		ideWriteReg(ctrl, channel, ATA_REG_CONTROL, 0x80 | ctrl->channels[channel].nIEN);
	if (reg < 0x08)
		insd(ctrl->channels[channel].base  + reg - 0x00, buffer, count);
	else if (reg < 0x0C)
		insd(ctrl->channels[channel].base  + reg - 0x06, buffer, count);
	else if (reg < 0x0E)
		insd(ctrl->channels[channel].ctrl  + reg - 0x0A, buffer, count);
	else if (reg < 0x16)
		insd(ctrl->channels[channel].bmide + reg - 0x0E, buffer, count);
	if (reg > 0x07 && reg < 0x0C)
		ideWriteReg(ctrl, channel, ATA_REG_CONTROL, ctrl->channels[channel].nIEN);
};

static uint8_t idePoll(IDEController *ctrl, uint8_t channel, int adv)
{
	// (I) Delay 400 nanosecond for BSY to be set:
	// -------------------------------------------------
	int i;
	for(i = 0; i < 4; i++)
		ideReadReg(ctrl, channel, ATA_REG_ALTSTATUS); // Reading the Alternate Status port wastes 100ns; loop four times.
 
	// (II) Wait for BSY to be cleared:
	// -------------------------------------------------
	while (ideReadReg(ctrl, channel, ATA_REG_STATUS) & ATA_SR_BSY)
		; // Wait for BSY to be zero.
 
	if (adv) {
		uint8_t state = ideReadReg(ctrl, channel, ATA_REG_STATUS); // Read Status Register.
 
		// (III) Check For Errors:
		// -------------------------------------------------
		if (state & ATA_SR_ERR)
			return 2; // Error.
 
		// (IV) Check If Device fault:
		// -------------------------------------------------
		if (state & ATA_SR_DF)
			return 1; // Device Fault.
 
		// (V) Check DRQ:
		// -------------------------------------------------
		// BSY = 0; DF = 0; ERR = 0 so we should check for DRQ now.
		if ((state & ATA_SR_DRQ) == 0)
			return 3; // DRQ should be set
	};
 
	return 0; // No Error.
};

static void ataThread(void *data)
{
	IDEThreadParams *thparams = (IDEThreadParams*) data;
	IDEController *ctrl = thparams->ctrl;
	IDEDevice *idev = &ctrl->devices[thparams->devidx];
	StorageDevice *sd = thparams->sd;
	kfree(thparams);

	while (1)
	{
		SDCommand *sdcmd = sdPop(sd);
		semWait(&ideSem);

		uint8_t cmd, lbaMode, head;
		uint8_t lbaIO[6];
		uint8_t channel = idev->channel;
		uint8_t slavebit = idev->drive;
		uint16_t bus = ctrl->channels[channel].base;
		unsigned long words = 256 * sdcmd->count;
		uint8_t err;

		ideWriteReg(ctrl, channel, ATA_REG_CONTROL, ctrl->channels[channel].nIEN = 2);
		if (sdcmd->index >= 0x10000000)
		{
			// LBA48
			lbaMode = 2;
			lbaIO[0] = (sdcmd->index & 0x0000000000FF) >> 0;
			lbaIO[1] = (sdcmd->index & 0x00000000FF00) >> 8;
			lbaIO[2] = (sdcmd->index & 0x000000FF0000) >> 16;
			lbaIO[3] = (sdcmd->index & 0x0000FF000000) >> 24;
			lbaIO[4] = (sdcmd->index & 0x00FF00000000) >> 32;
			lbaIO[5] = (sdcmd->index & 0xFF0000000000) >> 40;
			head = 0;
		}
		else
		{
			//LBA24
			lbaMode = 1;
			lbaIO[0] = (sdcmd->index & 0x0000000000FF) >> 0;
			lbaIO[1] = (sdcmd->index & 0x00000000FF00) >> 8;
			lbaIO[2] = (sdcmd->index & 0x000000FF0000) >> 16;
			lbaIO[3] = 0;
			lbaIO[4] = 0;
			lbaIO[5] = 0;
			head = (sdcmd->index & 0xF000000) >> 20;
		};

		while (ideReadReg(ctrl, channel, ATA_REG_STATUS) & ATA_SR_BSY);
		ideWriteReg(ctrl, channel, ATA_REG_HDDEVSEL, 0xE0 | (slavebit << 4) | head);

		if (lbaMode == 2)
		{
			ideWriteReg(ctrl, channel, ATA_REG_SECCOUNT1, 0);
			ideWriteReg(ctrl, channel, ATA_REG_LBA3, lbaIO[3]);
			ideWriteReg(ctrl, channel, ATA_REG_LBA4, lbaIO[4]);
			ideWriteReg(ctrl, channel, ATA_REG_LBA5, lbaIO[5]);
		};
		ideWriteReg(ctrl, channel, ATA_REG_SECCOUNT0, sdcmd->count);
		ideWriteReg(ctrl, channel, ATA_REG_LBA0, lbaIO[0]);
		ideWriteReg(ctrl, channel, ATA_REG_LBA1, lbaIO[1]);
		ideWriteReg(ctrl, channel, ATA_REG_LBA2, lbaIO[2]);

		if ((lbaMode == 1) && (sdcmd->type == SD_CMD_READ)) cmd = ATA_CMD_READ_PIO;
		if ((lbaMode == 2) && (sdcmd->type == SD_CMD_READ)) cmd = ATA_CMD_READ_PIO_EXT;
		if ((lbaMode == 1) && (sdcmd->type == SD_CMD_WRITE)) cmd = ATA_CMD_WRITE_PIO;
		if ((lbaMode == 2) && (sdcmd->type == SD_CMD_WRITE)) cmd = ATA_CMD_WRITE_PIO_EXT;
		ideWriteReg(ctrl, channel, ATA_REG_COMMAND, cmd);

		if (sdcmd->type == SD_CMD_READ)
		{
			//kprintf("[SDIDE] READ SECTOR %p, COUNT=%d\n", sdcmd->index, (int) sdcmd->count);
			//kprintf_debug("sdide: read sector %d\n", sdcmd->index);
			if ((err = idePoll(ctrl, channel, 1)))		// Yes, assignment.
			{
				panic("sdide: error %d\n", err);
			}
			else
			{
				insw(bus, sdcmd->block, words);
			};
		}
		else
		{
			//kprintf("[SDIDE] WRITE SECTOR %p, COUNT=%d\n", sdcmd->index, (int) sdcmd->count);
			// write
			if ((err = idePoll(ctrl, channel, 1)))
			{
				panic("sdide: error %d\n", err);
			};
			//kprintf("sdide: write sector %d\n", sdcmd->index);
			outsw(bus, sdcmd->block, words);
			if (lbaMode == 1)
			{
				ideWriteReg(ctrl, channel, ATA_REG_COMMAND, ATA_CMD_CACHE_FLUSH);
			}
			else
			{
				ideWriteReg(ctrl, channel, ATA_REG_COMMAND, ATA_CMD_CACHE_FLUSH_EXT);
			};
			if ((err = idePoll(ctrl, channel, 0)))
			{
				panic("sdide: error %d\n", err);
			};
		};

		semSignal(&ideSem);
		sdPostComplete(sdcmd);
	};
};

static void atapiThread(void *data)
{
	IDEThreadParams *thparams = (IDEThreadParams*) data;
	IDEController *ctrl = thparams->ctrl;
	IDEDevice *idev = &ctrl->devices[thparams->devidx];
	StorageDevice *sd = thparams->sd;
	kfree(thparams);

	while (1)
	{
		SDCommand *sdcmd = sdPop(sd);
		semWait(&ideSem);

		if (sdcmd->type == SD_CMD_READ)
		{
			//kprintf_debug("sdide: read atapi device, sector %d\n", sdcmd->index);

			uint32_t channel = idev->channel;
			uint32_t slavebit = idev->drive;
			uint16_t bus = ctrl->channels[channel].base;
			unsigned long words = 1024 * sdcmd->count;
			uint8_t err;
			int i;

			ideWriteReg(ctrl, channel, ATA_REG_CONTROL, ctrl->channels[channel].nIEN = ideWaitIRQ[channel] = 0);
			ctrl->atapiPacket[0] = ATAPI_CMD_READ;
			ctrl->atapiPacket[1] = 0;
			ctrl->atapiPacket[2] = (sdcmd->index >> 24) & 0xFF;
			ctrl->atapiPacket[3] = (sdcmd->index >> 16) & 0xFF;
			ctrl->atapiPacket[4] = (sdcmd->index >> 8) & 0xFF;
			ctrl->atapiPacket[5] = sdcmd->index & 0xFF;
			ctrl->atapiPacket[6] = 0;
			ctrl->atapiPacket[7] = 0;
			ctrl->atapiPacket[8] = 0;
			ctrl->atapiPacket[9] = sdcmd->count;
			ctrl->atapiPacket[10] = 0;
			ctrl->atapiPacket[11] = 0;

			ideWriteReg(ctrl, channel, ATA_REG_HDDEVSEL, slavebit << 4);
			for (i=0; i<4; i++)
			{
				ideReadReg(ctrl, channel, ATA_REG_ALTSTATUS);
			};

			ideWriteReg(ctrl, channel, ATA_REG_FEATURES, 0);
			ideWriteReg(ctrl, channel, ATA_REG_LBA1, (words * 2) & 0xFF);
			ideWriteReg(ctrl, channel, ATA_REG_LBA2, ((words * 2) >> 8) & 0xFF);
			ideWriteReg(ctrl, channel, ATA_REG_COMMAND, ATA_CMD_PACKET);

			err = idePoll(ctrl, channel, 1);
			if (err)
			{
				//kprintf_debug("atapi read error: %d\n", err);
			}
			else
			{
				outsw(bus, ctrl->atapiPacket, 6);
				while (!ideWaitIRQ[channel]);
				ideWaitIRQ[channel] = 0;
				err = idePoll(ctrl, channel, 1);
				if (err)
				{
					//kprintf_debug("atapi read error: %d\n", err);
				}
				else
				{
					insw(bus, sdcmd->block, words);
					while (!ideWaitIRQ[channel]);
					while (ideReadReg(ctrl, channel, ATA_REG_STATUS) & (ATA_SR_BSY | ATA_SR_DRQ));
				};
			};
		}
		else if (sdcmd->type == SD_CMD_GET_SIZE)
		{
			uint32_t channel = idev->channel;
			uint32_t slavebit = idev->drive;
			int k;
			
			ideWriteReg(ctrl, channel, ATA_REG_CONTROL, ctrl->channels[channel].nIEN = ideWaitIRQ[channel] = 0);
			ideWriteReg(ctrl, channel, ATA_REG_HDDEVSEL, slavebit << 4);
			for (k=0; k<4; k++)
			{
				ideReadReg(ctrl, channel, ATA_REG_ALTSTATUS);
			};

			ideWriteReg(ctrl, channel, ATA_REG_FEATURES, 0);
			ideWriteReg(ctrl, channel, ATA_REG_LBA1, 8);
			ideWriteReg(ctrl, channel, ATA_REG_LBA2, 0);

			memset(&ctrl->atapiPacket, 0, 24);
			ctrl->atapiPacket[0] = 0x25;
			ideWriteReg(ctrl, channel, ATA_REG_COMMAND, ATA_CMD_PACKET);
			if (idePoll(ctrl, channel, 1))
			{
				kprintf_debug("sdide: error while attempting to read medium size, skipping\n");
			}
			else
			{
				outsw(ctrl->channels[channel].base, ctrl->atapiPacket, 6);
				while (!ideWaitIRQ[channel]);
				if (idePoll(ctrl, channel, 1))
				{
					kprintf_debug("sdide: error while attempting to read medium size, skipping\n");
				}
				else
				{
					uint8_t buffer[8];
					insw(ctrl->channels[channel].base, buffer, 4);
					idePoll(ctrl, channel, 0);
					uint8_t rdbuf[4];				// converting to little endian
					rdbuf[0] = buffer[3];
					rdbuf[1] = buffer[2];
					rdbuf[2] = buffer[1];
					rdbuf[3] = buffer[0];
					uint32_t sectors = *((uint32_t*)&rdbuf[0]);
					kprintf_debug("sdide: ATAPI sector count: %d\n", sectors);
					*((off_t*)sdcmd->block) = (off_t) sectors * 2048;
				};
			};
		}
		else if (sdcmd->type == SD_CMD_EJECT)
		{
			kprintf_debug("sdide: ejecting ATAPI device\n");

			uint32_t channel = idev->channel;
			uint32_t slavebit = idev->drive;
			int k;
			
			ideWriteReg(ctrl, channel, ATA_REG_CONTROL, ctrl->channels[channel].nIEN = ideWaitIRQ[channel] = 0);
			ideWriteReg(ctrl, channel, ATA_REG_HDDEVSEL, slavebit << 4);
			for (k=0; k<4; k++)
			{
				ideReadReg(ctrl, channel, ATA_REG_ALTSTATUS);
			};

			ideWriteReg(ctrl, channel, ATA_REG_FEATURES, 0);
			ideWriteReg(ctrl, channel, ATA_REG_LBA1, 8);
			ideWriteReg(ctrl, channel, ATA_REG_LBA2, 0);

			memset(&ctrl->atapiPacket, 0, 24);
			ctrl->atapiPacket[0] = 0x1B;
			ctrl->atapiPacket[4] = 2;
			ideWriteReg(ctrl, channel, ATA_REG_COMMAND, ATA_CMD_PACKET);
			if (idePoll(ctrl, channel, 1))
			{
				kprintf_debug("sdide: error while attempting to eject, skipping\n");
			}
			else
			{
				outsw(ctrl->channels[channel].base, ctrl->atapiPacket, 6);
				while (!ideWaitIRQ[channel]);
				if (idePoll(ctrl, channel, 1))
				{
					kprintf_debug("sdide: error while attempting to eject, skipping\n");
				};
			};
			
		};

		semSignal(&ideSem);
		sdPostComplete(sdcmd);
	};
};

static uint16_t getPortNumber(uint32_t bar, uint16_t def)
{
	bar &= ~3;
	if (bar < 2) return def;
	else return (uint16_t) bar;
};

static void ideInit(uint32_t *bars)
{
	registerIRQHandler(ATA_IRQ_PRIMARY, ideHandleIRQ);
	registerIRQHandler(ATA_IRQ_SECONDARY, ideHandleIRQ);

	semInit(&ideSem);
	semWait(&ideSem);

	IDEController *ctrl = (IDEController*) kmalloc(sizeof(IDEController));
	ctrl->channels[ATA_PRIMARY].base = getPortNumber(bars[0], 0x1F0);
	ctrl->channels[ATA_PRIMARY].ctrl = getPortNumber(bars[1], 0x3F6);
	ctrl->channels[ATA_SECONDARY].base = getPortNumber(bars[2], 0x170);
	ctrl->channels[ATA_SECONDARY].ctrl = getPortNumber(bars[3], 0x376);
	ctrl->channels[ATA_PRIMARY].bmide = bars[4] & ~2;
	ctrl->channels[ATA_SECONDARY].bmide = (bars[4] & ~2) + 8;

	ideWriteReg(ctrl, ATA_PRIMARY, ATA_REG_CONTROL, 2);
	ideWriteReg(ctrl, ATA_SECONDARY, ATA_REG_CONTROL, 2);

	int i, j, k, count=0;
	for (i=0; i<2/*1*/; i++)
	{
		for (j=0; j<2; j++)
		{
			//kprintf("loop %d, %d\n", i, j);
			uint8_t err = 0, type = IDE_ATA, status;
			ctrl->devices[count].exists = 0;

			ideWriteReg(ctrl, i, ATA_REG_HDDEVSEL, 0xA0 | (j << 4));
			sleep(1);

			ideWriteReg(ctrl, i, ATA_REG_COMMAND, ATA_CMD_IDENTIFY);
			sleep(1);

			if (ideReadReg(ctrl, i, ATA_REG_STATUS) == 0) continue;			// no device

			while (1)
			{
				status = ideReadReg(ctrl, i, ATA_REG_STATUS);
				if ((status & ATA_SR_ERR)) {err = 1; break;}			// not ATA (could be ATAPI)
				if (!(status & ATA_SR_BSY) && (status & ATA_SR_DRQ)) break;	// ATA
			};

			if (err != 0)
			{
				// check if this is ATAPI
				uint8_t cl = ideReadReg(ctrl, i, ATA_REG_LBA1);
				uint8_t ch = ideReadReg(ctrl, i, ATA_REG_LBA2);

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
					continue;			// not even real
				};

				ideWriteReg(ctrl, i, ATA_REG_COMMAND, ATA_CMD_IDENTIFY_PACKET);
				sleep(1);
			};

			ideReadBuffer(ctrl, i, ATA_REG_DATA, ctrl->idbuf, 128);
			ctrl->devices[count].exists = 1;
			ctrl->devices[count].channel = i;
			ctrl->devices[count].drive = j;
			ctrl->devices[count].type = type;
			ctrl->devices[count].sig = *((uint16_t*)(ctrl->idbuf + ATA_IDENT_DEVICETYPE));
			ctrl->devices[count].cap = *((uint16_t*)(ctrl->idbuf + ATA_IDENT_CAPABILITIES));
			if ((ctrl->devices[count].cap & 0x200) == 0)
			{
				kprintf("sdide: skipping non-LBA device\n");
				continue;
			};
			ctrl->devices[count].cmdset = *((uint32_t*)(ctrl->idbuf + ATA_IDENT_COMMANDSETS));

			if (ctrl->devices[count].cmdset & (1 << 26))
			{
				ctrl->devices[count].size = *((uint32_t*)(ctrl->idbuf + ATA_IDENT_MAX_LBA_EXT));
			}
			else
			{
				ctrl->devices[count].size = *((uint32_t*)(ctrl->idbuf + ATA_IDENT_MAX_LBA));
			};

			// again, I don't know who decided to mangle the name like this but they must've been high
			for (k=0; k<40; k+=2)
			{
				ctrl->devices[count].model[k] = ctrl->idbuf[ATA_IDENT_MODEL + k + 1];
				ctrl->devices[count].model[k+1] = ctrl->idbuf[ATA_IDENT_MODEL + k];
			};
			ctrl->devices[count].model[40] = 0;

			kprintf("sdide: found %s device (%dMB): %s\n",
				(const char*[]){"ATA", "ATAPI"}[ctrl->devices[count].type],
				ctrl->devices[count].size / 1024 / 2,
				ctrl->devices[count].model);

			if (type == IDE_ATA)
			{
				SDParams sdparams;
				sdparams.flags = 0;
				sdparams.blockSize = 512;
				sdparams.totalSize = (uint64_t)ctrl->devices[count].size * (uint64_t)512;

				IDEThreadParams *thparams = (IDEThreadParams*) kmalloc(sizeof(IDEThreadParams));
				thparams->ctrl = ctrl;
				thparams->devidx = count;
				thparams->sd = sdCreate(&sdparams);

				KernelThreadParams ataPars;
				memset(&ataPars, 0, sizeof(KernelThreadParams));
				ataPars.stackSize = 0x4000;
				ataPars.name = "ATA device";
				CreateKernelThread(ataThread, &ataPars, thparams);
			}
			else
			{
				// ATAPI; dynamically determined size
				SDParams sdparams;
				sdparams.flags = SD_READONLY;
				sdparams.blockSize = 2048;
				sdparams.totalSize = 0;

				IDEThreadParams *thparams = (IDEThreadParams*) kmalloc(sizeof(IDEThreadParams));
				thparams->ctrl = ctrl;
				thparams->devidx = count;
				thparams->sd = sdCreate(&sdparams);
				
				KernelThreadParams atapiPars;
				memset(&atapiPars, 0, sizeof(KernelThreadParams));
				atapiPars.stackSize = 0x4000;
				atapiPars.name = "ATAPI device";
				CreateKernelThread(atapiThread, &atapiPars, thparams);
			};

			count++;
		};
	};

	kprintf("sdide: SDI interface ready\n");
	semSignal(&ideSem);
};

static void checkBus(uint8_t bus)
{
	uint8_t slot, func;
	PCIDeviceConfig config;

	for (slot=0; slot<32; slot++)
	{
		for (func=0; func<8; func++)
		{
			pciGetDeviceConfig(bus, slot, func, &config);
			if ((config.std.classcode == 1) && (config.std.subclass == 1))
			{
				kprintf("sdide: found IDE controller on PCI bus %d, slot %d, func %d, IRQ %d, intpin %d\n",
					bus, slot, func, config.std.intline, config.std.intpin);
				ideInit(config.std.bar);
				return;
			};
		};
	};
};

MODULE_INIT()
{
	kprintf("sdide: scanning for IDE controllers on primary bus\n");
	checkBus(0);
	return 0;
};

MODULE_FINI()
{
	return 0;
};
