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
#include <glidix/hw/dma.h>

#include "ata.h"
#include "port.h"
#include "sdahci.h"

#define	ATA_READ					0
#define	ATA_WRITE					1

static int ataIssueCmd(
	AHCIPort *port,
	uint8_t cmd,
	size_t startBlock,
	size_t numBlocks,
	void *buffer,
	int dir
)
{
	mutexLock(&port->lock);
	port->ctrl->regs->is = port->ctrl->regs->is;
	port->regs->is = port->regs->is;
	port->regs->serr = port->regs->serr;
	
	AHCIOpArea *opArea = (AHCIOpArea*) dmaGetPtr(&port->dmabuf);
	opArea->cmdlist[0].cfl = sizeof(FIS_REG_H2D) / 4;
	
	if (dir == ATA_READ)
	{
		opArea->cmdlist[0].w = 0;
		opArea->cmdlist[0].p = 1;
	}
	else
	{
		opArea->cmdlist[0].w = 1;
		opArea->cmdlist[0].p = 0;
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
	opArea->cmdlist[0].prdbc = 0;

	FIS_REG_H2D *cmdfis = (FIS_REG_H2D*) opArea->cmdtab.cfis;
	cmdfis->fis_type = FIS_TYPE_REG_H2D;
	cmdfis->c = 1;
	cmdfis->command = cmd;
	
	cmdfis->lba0 = (uint8_t)startBlock;
	cmdfis->lba1 = (uint8_t)(startBlock>>8);
	cmdfis->lba2 = (uint8_t)(startBlock>>16);
	cmdfis->lba3 = (uint8_t)(startBlock>>24);
	cmdfis->lba4 = (uint8_t)(startBlock>>32);
	cmdfis->lba5 = (uint8_t)(startBlock>>40);
	cmdfis->device = 1<<6;	// LBA mode
	cmdfis->count = numBlocks;

	// Issue the command.
	int status = portIssueCmd(port);
	mutexUnlock(&port->lock);
	return status;
};

int ataTransferBlocks(AHCIPort *port, size_t startBlock, size_t numBlocks, void *buffer, int dir)
{
	int cmd = dir == ATA_WRITE ? ATA_CMD_WRITE_DMA_EXT : ATA_CMD_READ_DMA_EXT;
	int status = ataIssueCmd(port, cmd, startBlock, numBlocks, buffer, dir);

	if (status != 0)
	{
		return status;
	};

	return ataIssueCmd(port, ATA_CMD_CACHE_FLUSH_EXT, 0, 0, NULL, ATA_READ);
};

int ataReadBlocks(void *drvdata, size_t startBlock, size_t numBlocks, void *buffer)
{
	AHCIPort *port = (AHCIPort*) drvdata;
	return ataTransferBlocks(port, startBlock, numBlocks, buffer, ATA_READ);
};

int ataWriteBlocks(void *drvdata, size_t startBlock, size_t numBlocks, const void *buffer)
{
	AHCIPort *dev = (AHCIPort*) drvdata;
	return ataTransferBlocks(dev, startBlock, numBlocks, (void*)buffer, ATA_WRITE);
};

SDOps ataOps = {
	.size = sizeof(SDOps),
	.readBlocks = ataReadBlocks,
	.writeBlocks = ataWriteBlocks,
};

void ataInit(AHCIPort *port)
{
	AHCIOpArea *opArea = (AHCIOpArea*) dmaGetPtr(&port->dmabuf);
	if (ataIssueCmd(port, ATA_CMD_IDENTIFY, 0, 1, opArea->id, ATA_READ) != 0)
	{
		kprintf("sdahci: error during identification\n");
		return;
	};
	
	uint32_t *cmdsetptr = (uint32_t*) &opArea->id[ATA_IDENT_COMMANDSETS];
	
	uint32_t cmdset = *cmdsetptr;
	uint64_t size;
	if (cmdset & ATA_CMDSETS_LBA_EXT)
	{
		uint64_t *szptr = (uint64_t*) &opArea->id[ATA_IDENT_MAX_LBA_EXT];
		size = (*szptr) & 0xFFFFFFFFFFFF;
	}
	else
	{
		kprintf("sdahci: LBA48 not supported, skipping drive.\n");
		return;
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

	port->sd = sdCreate(&sdpars, model, &ataOps, port);
	if (port->sd == NULL)
	{
		kprintf("sdahci: SD creation failed\n");
		// NOTE: do not free anything; this is done upon removing the driver
	};
};
