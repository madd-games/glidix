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

#include "sdide.h"
#include "atapi.h"

int atapiReadBlocks(void *drvdata, size_t startBlock, size_t numBlocks, void *buffer)
{
	IDEDevice *dev = (IDEDevice*) drvdata;
	IDEController *ctrl = dev->ctrl;
	int channel = dev->channel;
	
	semWait(&dev->ctrl->lock);
	
	// enable interrupts
	outb(dev->ctrl->channels[dev->channel].ctrl + ATA_CREG_CONTROL, 0);
	ideInts[dev->channel] = 0;
	
	// ATAPI packet to send
	uint8_t atapiPacket[12];
	atapiPacket[0] = ATAPI_CMD_READ;
	atapiPacket[1] = 0;
	atapiPacket[2] = (startBlock >> 24) & 0xFF;
	atapiPacket[3] = (startBlock >> 16) & 0xFF;
	atapiPacket[4] = (startBlock >> 8) & 0xFF;
	atapiPacket[5] = startBlock & 0xFF;
	atapiPacket[6] = 0;
	atapiPacket[7] = 0;
	atapiPacket[8] = 0;
	atapiPacket[9] = numBlocks;
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
	
	// specify the size in bytes
	size_t totalSize = numBlocks * 0x800;
	outb(ctrl->channels[channel].base + ATA_IOREG_LBA1, totalSize & 0xFF);			// low byte
	outb(ctrl->channels[channel].base + ATA_IOREG_LBA2, (totalSize >> 8) & 0xFF);		// high byte
	
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
		
		// receive the data
		insw(ctrl->channels[channel].base + ATA_IOREG_DATA, buffer, totalSize >> 1);
		
		// wait for it to stop being busy
		while (!ideInts[dev->channel]);
		ideInts[dev->channel] = 0;
		while (inb(ctrl->channels[channel].base + ATA_IOREG_STATUS) & (ATA_SR_BSY | ATA_SR_DRQ));
	};

	semSignal(&dev->ctrl->lock);
	
	if (error)
	{
		return EIO;
	};
	
	return 0;
};

int atapiWriteBlocks(void *drvdata, size_t startBlock, size_t numBlocks, const void *buffer)
{
	// technically speaking this shouldn't be called, but if it is, return the "read-only" error,
	// because you can't write to ATAPI
	return EROFS;
};

SDOps atapiOps = {
	.size = sizeof(SDOps),
	.readBlocks = atapiReadBlocks,
	.writeBlocks = atapiWriteBlocks,
};
