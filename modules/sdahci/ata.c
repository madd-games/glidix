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

#include <glidix/util/memory.h>
#include <glidix/display/console.h>
#include <glidix/util/string.h>

#include "ata.h"

#define	ATA_READ					0
#define	ATA_WRITE					1

int ataTransferBlocks(ATADevice *dev, size_t startBlock, size_t numBlocks, void *buffer, int dir)
{
	mutexLock(&dev->lock);
	
	AHCIOpArea *opArea = (AHCIOpArea*) dmaGetPtr(&dev->dmabuf);
	opArea->cmdlist[0].cfl = sizeof(FIS_REG_H2D) / 4;
	
	if (dir == ATA_READ)
	{
		opArea->cmdlist[0].w = 0;
		opArea->cmdlist[0].p = 0;
		opArea->cmdlist[0].c = 0;
	}
	else
	{
		opArea->cmdlist[0].w = 1;
		opArea->cmdlist[0].p = 1;
		opArea->cmdlist[0].c = 1;
	};
	
	uint16_t prdtl = 0;
	
	DMARegion reg;
	for (dmaFirstRegion(&reg, buffer, 512*numBlocks, 0); reg.physAddr!=0; dmaNextRegion(&reg))
	{
		if (prdtl == 9) panic("unexpected input");
		
		opArea->cmdtab.prdt[prdtl].dba = reg.physAddr;
		opArea->cmdtab.prdt[prdtl].dbc = reg.physSize - 1;
		opArea->cmdtab.prdt[prdtl].i = 0;
		
		prdtl++;
	};
	
	opArea->cmdlist[0].prdtl = prdtl;

	FIS_REG_H2D *cmdfis = (FIS_REG_H2D*)(&opArea->cmdtab.cfis);
	cmdfis->fis_type = FIS_TYPE_REG_H2D;
	cmdfis->c = 1;
	if (dir == ATA_READ)
	{
		cmdfis->command = ATA_CMD_READ_DMA_EXT;
	}
	else
	{
		cmdfis->command = ATA_CMD_WRITE_DMA_EXT;
	};
	
	cmdfis->lba0 = (uint8_t)startBlock;
	cmdfis->lba1 = (uint8_t)(startBlock>>8);
	cmdfis->lba2 = (uint8_t)(startBlock>>16);
	cmdfis->device = 1<<6;	// LBA mode
 
	cmdfis->lba3 = (uint8_t)(startBlock>>24);
	cmdfis->lba4 = (uint8_t)(startBlock>>32);
	cmdfis->lba5 = (uint8_t)(startBlock>>40);
	
	cmdfis->countl = numBlocks & 0xFF;
	cmdfis->counth = (numBlocks >> 8) & 0xFF;
	
	// issue the command
	int status = ahciIssueCmd(dev->port);
	if (status != 0)
	{
		mutexUnlock(&dev->lock);
		return status;
	};
	
	// do a cache flush
	opArea->cmdlist[0].w = 0;
	opArea->cmdlist[0].p = 0;
	opArea->cmdlist[0].c = 0;
	opArea->cmdlist[0].prdtl = 0;

	cmdfis->fis_type = FIS_TYPE_REG_H2D;
	cmdfis->c = 1;
	cmdfis->command = ATA_CMD_CACHE_FLUSH_EXT;
	
	cmdfis->lba0 = 0;
	cmdfis->lba1 = 0;
	cmdfis->lba2 = 0;
	cmdfis->device = 1<<6;	// LBA mode
 
	cmdfis->lba3 = 0;
	cmdfis->lba4 = 0;
	cmdfis->lba5 = 0;
	
	cmdfis->countl = 0;
	cmdfis->counth = 0;
	
	// issue the flush command
	status = ahciIssueCmd(dev->port);
	mutexUnlock(&dev->lock);
	return status;
};

int ataReadBlocks(void *drvdata, size_t startBlock, size_t numBlocks, void *buffer)
{
	ATADevice *dev = (ATADevice*) drvdata;
	return ataTransferBlocks(dev, startBlock, numBlocks, buffer, ATA_READ);
};

int ataWriteBlocks(void *drvdata, size_t startBlock, size_t numBlocks, const void *buffer)
{
	ATADevice *dev = (ATADevice*) drvdata;
	return ataTransferBlocks(dev, startBlock, numBlocks, (void*)buffer, ATA_WRITE);
};

SDOps ataOps = {
	.size = sizeof(SDOps),
	.readBlocks = ataReadBlocks,
	.writeBlocks = ataWriteBlocks,
};

void ataInit(AHCIController *ctrl, int portno)
{
	ATADevice *dev = NEW(ATADevice);
	ctrl->ataDevices[ctrl->numAtaDevices++] = dev;
	mutexInit(&dev->lock);
	
	dev->ctrl = ctrl;
	dev->port = &ctrl->regs->ports[portno];
	dev->sd = NULL;
	
	// stop the command engine while setting up the commands and stuff
	ahciStopCmd(dev->port);
	
	// create the operations area
	if (dmaCreateBuffer(&dev->dmabuf, sizeof(AHCIOpArea), 0) != 0)
	{
		// it didn't work
		kprintf("sdahci: failed to allocate operations area\n");
		kfree(dev);
		ctrl->numAtaDevices--;
		return;
	};
	
	// set up the operations area
	AHCIOpArea *opArea = (AHCIOpArea*) dmaGetPtr(&dev->dmabuf);
	memset(opArea, 0, sizeof(AHCIOpArea));
	
	// set the command list and FIS area
	dev->port->clb = dmaGetPhys(&dev->dmabuf) + __builtin_offsetof(AHCIOpArea, cmdlist);
	dev->port->fb = dmaGetPhys(&dev->dmabuf) + __builtin_offsetof(AHCIOpArea, fisArea);
	
	// we only use the first command header, so initialize it to point to the table
	opArea->cmdlist[0].ctba = dmaGetPhys(&dev->dmabuf) + __builtin_offsetof(AHCIOpArea, cmdtab);
	
	// start the command engine
	ahciStartCmd(dev->port);
	dev->port->is = dev->port->is;
	
	// send the IDENTIFY command.
	opArea->cmdlist[0].cfl = sizeof(FIS_REG_H2D)/4;		// FIS length in dwords
	opArea->cmdlist[0].w = 0;				// read data
	opArea->cmdlist[0].prdtl = 1;				// only one PRDT entry
	opArea->cmdlist[0].p = 1;
	
	opArea->cmdtab.prdt[0].dba = dmaGetPhys(&dev->dmabuf) + __builtin_offsetof(AHCIOpArea, id);
	opArea->cmdtab.prdt[0].dbc = 511;			// length-1
	opArea->cmdtab.prdt[0].i = 0;				// do not interrupt
	
	// set up command FIS
	FIS_REG_H2D *cmdfis = (FIS_REG_H2D*) opArea->cmdtab.cfis;
	cmdfis->fis_type = FIS_TYPE_REG_H2D;
	cmdfis->c = 1;
	cmdfis->command = ATA_CMD_IDENTIFY;
	
	// issue the command and await completion
	__sync_synchronize();
	if (ahciIssueCmd(dev->port) != 0)
	{
		kprintf("sdahci: error during identification\n");
		return;
	};
	
	uint32_t *cmdsetptr = (uint32_t*) &opArea->id[ATA_IDENT_COMMANDSETS];
	
	uint32_t cmdset = *cmdsetptr;
	uint64_t size;
	if (cmdset & (1 << 26))
	{
		uint64_t *szptr = (uint64_t*) &opArea->id[ATA_IDENT_MAX_LBA_EXT];
		size = (*szptr) & 0xFFFFFFFFFFFF;
	}
	else
	{
		uint32_t *szptr = (uint32_t*) &opArea->id[ATA_IDENT_MAX_LBA];
		size = (uint64_t) (*szptr);
	};

	char model[41];
	int k;
	for (k=0; k<40; k+=2)
	{
		model[k] = opArea->id[ATA_IDENT_MODEL + k + 1];
		model[k+1] = opArea->id[ATA_IDENT_MODEL + k];
	};
	model[40] = 0;
	char *check = &model[39];
	while (*check == ' ')
	{
		if (check == model) break;
		*check-- = 0;
	};

	kprintf("sdahci: size in MB: %lu, model: %s\n", size / 1024 / 2, model);
	
	SDParams sdpars;
	sdpars.flags = 0;
	sdpars.blockSize = 512;
	sdpars.totalSize = size * 512;
	
	dev->sd = sdCreate(&sdpars, model, &ataOps, dev);
	if (dev->sd == NULL)
	{
		kprintf("sdahci: SD creation failed\n");
		// NOTE: do not free anything; this is done upon removing the driver
	};
};
