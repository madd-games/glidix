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
#include "glidix/hw/dma.h"
#include "port.h"

#define	ATA_READ					0
#define	ATA_WRITE					1

int ataTransferBlocks(AHCIPort *dev, size_t startBlock, size_t numBlocks, void *buffer, int dir)
{
	mutexLock(&dev->lock);
	dev->ctrl->regs->is = dev->ctrl->regs->is;
	dev->regs->is = dev->regs->is;
	dev->regs->serr = dev->regs->serr;
	
	AHCIOpArea *opArea = (AHCIOpArea*) dmaGetPtr(&dev->dmabuf);
	opArea->cmdlist[0].cfl = sizeof(FIS_REG_H2D) / 4;
	opArea->cmdlist[0].c = 1;				// clear BSY when done
	
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

		if (dir == ATA_READ && startBlock == 0)
		{
			kprintf("ATA_READ 0: Region: physAddr=0x%016lX, physSize=0x%04lX\n", reg.physAddr, reg.physSize);
		};
		
		opArea->cmdtab.prdt[prdtl].dba = reg.physAddr;
		opArea->cmdtab.prdt[prdtl].dbc = reg.physSize - 1;
		opArea->cmdtab.prdt[prdtl].i = 0;
		
		prdtl++;
	};
	
	opArea->cmdlist[0].prdtl = prdtl;
	opArea->cmdlist[0].prdbc = 0;

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
	cmdfis->lba3 = (uint8_t)(startBlock>>24);
	cmdfis->lba4 = (uint8_t)(startBlock>>32);
	cmdfis->lba5 = (uint8_t)(startBlock>>40);
	cmdfis->device = 1<<6;	// LBA mode
	
	cmdfis->countl = numBlocks & 0xFF;
	cmdfis->counth = (numBlocks >> 8) & 0xFF;

	uint8_t *bytes = (uint8_t*) &opArea->cmdlist[0];
	kprintf("COMMAND HEADER: ");
	for (size_t i=0; i<sizeof(AHCICommandHeader); i++)
	{
		kprintf("%02hhx ", bytes[i]);
	};
	kprintf("\n");

	kprintf("COMMAND FIS: ");
	for (size_t i=0; i<sizeof(FIS_REG_H2D); i++)
	{
		kprintf("%02hhx ", opArea->cmdtab.cfis[i]);
	};
	kprintf("\n");
	
	opArea->cmdlist[0].prdbc = 69;
	
	kprintf(
		"BEFORE ISSUE: PRDTL=%u, PRDC=0x%x, IS=0x%08x, SERR=0x%08x, TFD=0x%08x\n",
		opArea->cmdlist[0].prdtl,
		opArea->cmdlist[0].prdbc,
		dev->regs->is,
		dev->regs->serr,
		dev->regs->tfd
	);

	// issue the command
	int status = ahciIssueCmd(dev->regs);
	if (status != 0)
	{
		mutexUnlock(&dev->lock);
		return status;
	};

	kprintf(
		"AFTER ISSUE: PRDTL=%u, PRDC=0x%x, IS=0x%08x, SERR=0x%08x, TFD=0x%08x\n",
		opArea->cmdlist[0].prdtl,
		opArea->cmdlist[0].prdbc,
		dev->regs->is,
		dev->regs->serr,
		dev->regs->tfd
	);
	
	// do a cache flush
	opArea->cmdlist[0].w = 0;
	opArea->cmdlist[0].p = 0;
	opArea->cmdlist[0].c = 1;
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
	status = ahciIssueCmd(dev->regs);
	mutexUnlock(&dev->lock);
	return status;
};

int ataReadBlocks(void *drvdata, size_t startBlock, size_t numBlocks, void *buffer)
{
	int isBootedFromHDD = memcmp(bootInfo->bootID, "\0\0" "ISOBOOT" "\0\0\0\0\0\xF0\x0D", 16) != 0;

	AHCIPort *dev = (AHCIPort*) drvdata;
	AHCIOpArea *opArea = (AHCIOpArea*) dmaGetPtr(&dev->dmabuf);

	if (isBootedFromHDD && startBlock == 0)
	{
		buffer = opArea->id;
		numBlocks = 1;
	};

	// DEBUG
	memset(buffer, 0, 512 * numBlocks);
	
	int result = ataTransferBlocks(dev, startBlock, numBlocks, buffer, ATA_READ);

	if (startBlock == 0)
	{
		kprintf(
			"The controller is [%04hx:%04hx] at (0x%x/0x%x/0x%x)\n",
			dev->ctrl->pcidev->vendor,
			dev->ctrl->pcidev->device,
			dev->ctrl->pcidev->bus,
			dev->ctrl->pcidev->slot,
			dev->ctrl->pcidev->func
		);
		kprintf("Dumping first sector of ATA drive:");

		uint8_t *buf = (uint8_t*) buffer;
		for (size_t i=0; i<512; i++)
		{
			if ((i % 32) == 0)
			{
				kprintf("\n");
			};

			kprintf("%02hhx ", buf[i]);
		};

		kprintf("\n---------------------------\n");

		kprintf("Dump of the received FIS:\n");
		for (size_t i=0; i<256; i++)
		{
			if ((i % 32) == 0)
			{
				kprintf("\n");
			};

			kprintf("%02hhx ", opArea->fisArea[i]);
		};

		kprintf("\n---------------------------\n");

		if (isBootedFromHDD)
		{
			while(1);
		};
	};

	return result;
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

	// send the IDENTIFY command.
	opArea->cmdlist[0].cfl = sizeof(FIS_REG_H2D)/4;		// FIS length in dwords
	opArea->cmdlist[0].w = 0;				// read data
	opArea->cmdlist[0].prdtl = 1;				// only one PRDT entry
	opArea->cmdlist[0].p = 1;
	
	opArea->cmdtab.prdt[0].dba = dmaGetPhys(&port->dmabuf) + __builtin_offsetof(AHCIOpArea, id);
	opArea->cmdtab.prdt[0].dbc = 511;			// length-1
	opArea->cmdtab.prdt[0].i = 0;				// do not interrupt
	
	// set up command FIS
	FIS_REG_H2D *cmdfis = (FIS_REG_H2D*) opArea->cmdtab.cfis;
	cmdfis->fis_type = FIS_TYPE_REG_H2D;
	cmdfis->c = 1;
	cmdfis->command = ATA_CMD_IDENTIFY;
	
	// issue the command and await completion
	if (ahciIssueCmd(port->regs) != 0)
	{
		kprintf("sdahci: error during identification\n");
		return;
	};

	kprintf("IDENTIFY PRDC: 0x%08x\n", opArea->cmdlist[0].prdbc);
	sleep(2000);
	
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

	// do a cache flush
	opArea->cmdlist[0].w = 0;
	opArea->cmdlist[0].p = 0;
	opArea->cmdlist[0].c = 1;
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
	int status = ahciIssueCmd(port->regs);
	kprintf("sdahci: cache flush status: %d\n", status);

	port->sd = sdCreate(&sdpars, model, &ataOps, port);
	if (port->sd == NULL)
	{
		kprintf("sdahci: SD creation failed\n");
		// NOTE: do not free anything; this is done upon removing the driver
	};
};
