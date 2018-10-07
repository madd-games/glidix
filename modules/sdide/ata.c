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

#include <glidix/hw/port.h>
#include <glidix/display/console.h>

#include "ata.h"
#include "sdide.h"

#define	ATA_READ					0
#define	ATA_WRITE					1

static int ataTransferBlocks(int direction, IDEDevice *dev, size_t startBlock, size_t numBlocks, void *buffer)
{
	IDEController *ctrl = dev->ctrl;
	int channel = dev->channel;
	
	semWait(&dev->ctrl->lock);
	
	ideInts[dev->channel] = 0;
	outb(dev->ctrl->channels[dev->channel].ctrl + ATA_CREG_CONTROL, ATA_CTRL_NIEN);
	
	// wait for it to stop being busy
	while (inb(ctrl->channels[channel].base + ATA_IOREG_STATUS) & ATA_SR_BSY);
	
	// select drive, use LBA
	outb(ctrl->channels[channel].base + ATA_IOREG_HDDEVSEL, 0xE0 | (dev->slot << 4));

	// select starting block and count; first write high-order bytes
	outb(dev->ctrl->channels[dev->channel].ctrl + ATA_CREG_CONTROL, ATA_CTRL_NIEN | ATA_CTRL_HOB);
	outb(ctrl->channels[channel].base + ATA_IOREG_LBA0, (startBlock >> 24) & 0xFF);
	outb(ctrl->channels[channel].base + ATA_IOREG_LBA1, (startBlock >> 32) & 0xFF);
	outb(ctrl->channels[channel].base + ATA_IOREG_LBA2, (startBlock >> 40) & 0xFF);
	outb(ctrl->channels[channel].base + ATA_IOREG_SECCOUNT, (numBlocks >> 8) & 0xFF);

	// now the low-order bytes
	outb(dev->ctrl->channels[dev->channel].ctrl + ATA_CREG_CONTROL, ATA_CTRL_NIEN);
	outb(ctrl->channels[channel].base + ATA_IOREG_LBA0, startBlock & 0xFF);
	outb(ctrl->channels[channel].base + ATA_IOREG_LBA1, (startBlock >> 8) & 0xFF);
	outb(ctrl->channels[channel].base + ATA_IOREG_LBA2, (startBlock >> 16) & 0xFF);
	outb(ctrl->channels[channel].base + ATA_IOREG_SECCOUNT, numBlocks & 0xFF);
	
	
	// send the appropriate command
	uint8_t cmd;
	if (direction == ATA_READ) cmd = ATA_CMD_READ_PIO_EXT;
	else cmd = ATA_CMD_WRITE_PIO_EXT;
	outb(ctrl->channels[channel].base + ATA_IOREG_COMMAND, cmd);
	
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
	
	if (error)
	{
		semSignal(&dev->ctrl->lock);
		return EIO;
	};
	
	size_t i;
	char *scan = (char*) buffer;
	
	for (i=0; i<numBlocks; i++)
	{
		// wait for it to stop being busy
		while (inb(ctrl->channels[channel].base + ATA_IOREG_STATUS) & ATA_SR_BSY);
		
		if (direction == ATA_READ)
		{
			// read
			insw(ctrl->channels[channel].base + ATA_IOREG_DATA, scan, 256);
		}
		else
		{
			// write
			outsw(ctrl->channels[channel].base + ATA_IOREG_DATA, scan, 256);
		};
		
		scan += 512;
	};
	
	// if we're writing, flush the cache
	outb(ctrl->channels[channel].base + ATA_IOREG_COMMAND, ATA_CMD_CACHE_FLUSH_EXT);
	
	// wait for it to stop being busy
	while (inb(ctrl->channels[channel].base + ATA_IOREG_STATUS) & (ATA_SR_BSY | ATA_SR_DRQ));

	// OK
	semSignal(&dev->ctrl->lock);
	return 0;
};

int ataReadBlocks(void *drvdata, size_t startBlock, size_t numBlocks, void *buffer)
{
	IDEDevice *dev = (IDEDevice*) drvdata;
	return ataTransferBlocks(ATA_READ, dev, startBlock, numBlocks, buffer);
};

int ataWriteBlocks(void *drvdata, size_t startBlock, size_t numBlocks, const void *buffer)
{
	IDEDevice *dev = (IDEDevice*) drvdata;
	return ataTransferBlocks(ATA_WRITE, dev, startBlock, numBlocks, (void*) buffer);
};

SDOps ataOps = {
	.size = sizeof(SDOps),
	.readBlocks = ataReadBlocks,
	.writeBlocks = ataWriteBlocks,
};
