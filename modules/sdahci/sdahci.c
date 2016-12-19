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
#include <glidix/dma.h>

#include "sdahci.h"

static AHCIController *firstCtrl = NULL;
static AHCIController *lastCtrl = NULL;
static int numCtrlFound = 0;

static void ahciStopCmd(volatile AHCIPort *port)
{
	port->cmd &= ~(1 << 0);
	__sync_synchronize();
	port->cmd &= ~(1 << 4);
	__sync_synchronize();
	
	while (1)
	{
		__sync_synchronize();
		if (port->cmd & (1 << 14))
		{
			asm volatile ("": : :"memory");
			continue;
		};
		
		if (port->cmd & (1 << 15))
		{
			asm volatile ("": : :"memory");
			continue;
		};
		
		break;
	};
	
	__sync_synchronize();
};

static void ahciStartCmd(volatile AHCIPort *port)
{
	while (port->cmd & (1 << 15)) __sync_synchronize();
	
	port->cmd |= (1 << 4);
	__sync_synchronize();
	port->cmd |= (1 << 0);
	__sync_synchronize();
};

static void ahciAtaThread(void *data)
{
	ATADevice *dev = (ATADevice*) data;

	AHCICommandHeader *cmdhead = (AHCICommandHeader*) dmaGetPtr(&dev->dmabuf);
				
	while (1)
	{
		SDCommand *cmd = sdPop(dev->sd);
		if (cmd->type == SD_CMD_SIGNAL)
		{
			sdPostComplete(cmd);
			break;
		};
		
		dev->port->is = dev->port->is;
		
		// we don't need to protect the device with semaphores, because only this thread
		// accesses its port.

		// first find a free command slot; we may wait for some time if the drive is
		// particularly busy.
		uint32_t slot = 0;
		while (dev->port->ci & (1 << slot))
		{
			slot = (slot + 1) % 32;
			__sync_synchronize();
		};
		
		// fill in the appropriate structure.
		// do not zero it; it was already zeroed before and only this thread updates the
		// structures so we know that any unused fields are already zero.
		cmdhead[slot].cfl = sizeof(FIS_REG_H2D)/4;
		if (cmd->type == SD_CMD_READ_TRACK)
		{
			cmdhead[slot].w = 0;
			cmdhead[slot].p = 0;
			cmdhead[slot].c = 0;
		}
		else
		{
			cmdhead[slot].w = 1;
			cmdhead[slot].p = 1;
			cmdhead[slot].c = 1;
		};
		cmdhead[slot].prdtl = 1;
		__sync_synchronize();
		
		AHCICommandTable *cmdtbl = (AHCICommandTable*) ((char*)dmaGetPtr(&dev->dmabuf) + 1024+256+8*1024+256*slot);
		cmdtbl->prdt_entry[0].dba = (uint64_t) cmd->block;
		cmdtbl->prdt_entry[0].dbc = 0x8000;
		cmdtbl->prdt_entry[0].i = 0;
		
		FIS_REG_H2D *cmdfis = (FIS_REG_H2D*)(&cmdtbl->cfis);
		cmdfis->fis_type = FIS_TYPE_REG_H2D;
		cmdfis->c = 1;
		if (cmd->type == SD_CMD_READ_TRACK)
		{
			cmdfis->command = ATA_CMD_READ_DMA_EXT;
		}
		else
		{
			cmdfis->command = ATA_CMD_WRITE_DMA_EXT;
		};
		
		uint64_t index = cmd->pos >> 9;
		cmdfis->lba0 = (uint8_t)index;
		cmdfis->lba1 = (uint8_t)(index>>8);
		cmdfis->lba2 = (uint8_t)(index>>16);
		cmdfis->device = 1<<6;	// LBA mode
	 
		cmdfis->lba3 = (uint8_t)(index>>24);
		cmdfis->lba4 = (uint8_t)(index>>32);
		cmdfis->lba5 = (uint8_t)(index>>40);
		
		// 32 KB
		cmdfis->countl = 0x40;
		cmdfis->counth = 0;
		
		//char *hwbuf = (char*) dmaGetPtr(&dev->iobuf) + (4096*slot);
		//if (cmd->type == SD_CMD_WRITE)
		//{
		//	memcpy(hwbuf, cmd->block, 512*cmd->count);
		//};
		
		// wait for the port to stop being busy
		int busy = (1 << 7) | (1 << 3);
		while (dev->port->tfd & busy) __sync_synchronize();
		__sync_synchronize();
		
		if (dev->port->tfd & (1 << 0))
		{
			panic("AHCI transfer error!");
		};
		
		dev->port->ci = (1 << slot);
		__sync_synchronize();
		
		while (dev->port->ci & (1 << slot))
		{
			//kprintf("IS: 0x%08x\n", dev->port->is);
			if (dev->port->is & (1 << 27))
			{
				panic("AHCI error!");
			};
			__sync_synchronize();
		};
		__sync_synchronize();
		
		//if (cmd->type == SD_CMD_READ)
		//{
		//	memcpy(cmd->block, hwbuf, 512*cmd->count);
		//};

		sdPostComplete(cmd);
	};
	
	sdHangup(dev->sd);
};

static void ahciAtapiThread(void *data)
{
	ATADevice *dev = (ATADevice*) data;

	AHCICommandHeader *cmdhead = (AHCICommandHeader*) dmaGetPtr(&dev->dmabuf);
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
			dev->port->is = dev->port->is;

			// first find a free command slot; we may wait for some time if the drive is
			// particularly busy.
			uint32_t slot = 0;
			while (dev->port->ci & (1 << slot))
			{
				slot = (slot + 1) % 32;
				__sync_synchronize();
			};
	
			// fill in the appropriate structure.
			// do not zero it; it was already zeroed before and only this thread updates the
			// structures so we know that any unused fields are already zero.
			cmdhead[slot].cfl = sizeof(FIS_REG_H2D)/4;
			cmdhead[slot].w = 0;
			cmdhead[slot].prdtl = 1;
			cmdhead[slot].a = 1;
			__sync_synchronize();
	
			AHCICommandTable *cmdtbl = (AHCICommandTable*) ((char*)dmaGetPtr(&dev->dmabuf)
							+ 1024+256+8*1024+256*slot);
			memset(cmdtbl->acmd, 0, 16);
			cmdtbl->acmd[0] = 0x25;
			
			cmdtbl->prdt_entry[0].dba = dmaGetPhys(&dev->iobuf) + 4096*slot;
			cmdtbl->prdt_entry[0].dbc = 2048;
			cmdtbl->prdt_entry[0].i = 0;
	
			FIS_REG_H2D *cmdfis = (FIS_REG_H2D*)(&cmdtbl->cfis);
			cmdfis->fis_type = FIS_TYPE_REG_H2D;
			cmdfis->c = 1;
			cmdfis->command = ATA_CMD_PACKET;
	
			cmdfis->lba0 = 0;
			cmdfis->lba1 = 0;
			cmdfis->lba2 = 0;
			cmdfis->device = 0;
		 
			cmdfis->lba3 = 0;
			cmdfis->lba4 = 0;
			cmdfis->lba5 = 0;
	
			cmdfis->countl = 0;
			cmdfis->counth = 0;
	
			char *hwbuf = (char*) dmaGetPtr(&dev->iobuf) + (4096*slot);
	
			// wait for the port to stop being busy
			int busy = (1 << 7) | (1 << 3);
			while (dev->port->tfd & busy) __sync_synchronize();
			__sync_synchronize();
	
			dev->port->ci = (1 << slot);
			__sync_synchronize();
	
			while (dev->port->ci & (1 << slot))
			{
				if (dev->port->is & ((1 << 30) | (1 << 27) | (1 << 29) | (1 << 28)))
				{
					dev->port->is = dev->port->is;
					ahciStopCmd(dev->port);
					ahciStartCmd(dev->port);
					memset(hwbuf, 0, 4);
					//kprintf("TFD: %x\n", dev->port->tfd);
				};
				__sync_synchronize();
			};
			__sync_synchronize();

			*((uint32_t*)cmd->block) = __builtin_bswap32(*((uint32_t*)hwbuf)) * 2048;
			sdPostComplete(cmd);
		}
		else if (cmd->type == SD_CMD_READ_TRACK)
		{	
			dev->port->is = dev->port->is;

			// first find a free command slot; we may wait for some time if the drive is
			// particularly busy.
			uint32_t slot = 0;
			while (dev->port->ci & (1 << slot))
			{
				slot = (slot + 1) % 32;
				__sync_synchronize();
			};
	
			// fill in the appropriate structure.
			// do not zero it; it was already zeroed before and only this thread updates the
			// structures so we know that any unused fields are already zero.
			cmdhead[slot].cfl = sizeof(FIS_REG_H2D)/4;
			cmdhead[slot].w = 0;
			cmdhead[slot].prdtl = 1;
			cmdhead[slot].a = 1;
			__sync_synchronize();
	
			AHCICommandTable *cmdtbl = (AHCICommandTable*) ((char*)dmaGetPtr(&dev->dmabuf)
							+ 1024+256+8*1024+256*slot);
			memset(cmdtbl->acmd, 0, 16);
			uint64_t index = cmd->pos >> 11;
			cmdtbl->acmd[0] = ATAPI_CMD_READ;
			cmdtbl->acmd[1] = 0;
			cmdtbl->acmd[2] = (index >> 24) & 0xFF;
			cmdtbl->acmd[3] = (index >> 16) & 0xFF;
			cmdtbl->acmd[4] = (index >> 8) & 0xFF;
			cmdtbl->acmd[5] = index & 0xFF;
			cmdtbl->acmd[6] = 0;
			cmdtbl->acmd[7] = 0;
			cmdtbl->acmd[8] = 0;
			cmdtbl->acmd[9] = 0x40;
			cmdtbl->acmd[10] = 0;
			cmdtbl->acmd[11] = 0;
			
			cmdtbl->prdt_entry[0].dba = (uint64_t) cmd->block;
			cmdtbl->prdt_entry[0].dbc = 0x8000;
			cmdtbl->prdt_entry[0].i = 0;
	
			FIS_REG_H2D *cmdfis = (FIS_REG_H2D*)(&cmdtbl->cfis);
			cmdfis->fis_type = FIS_TYPE_REG_H2D;
			cmdfis->c = 1;
			cmdfis->command = ATA_CMD_PACKET;
	
			cmdfis->lba0 = 0;
			cmdfis->lba1 = 0;
			cmdfis->lba2 = 0;
			cmdfis->device = 0;
		 
			cmdfis->lba3 = 0;
			cmdfis->lba4 = 0;
			cmdfis->lba5 = 0;
	
			cmdfis->countl = 0x40;
			cmdfis->counth = 0;
	
			//char *hwbuf = (char*) dmaGetPtr(&dev->iobuf) + (4096*slot);
	
			// wait for the port to stop being busy
			int busy = (1 << 7) | (1 << 3);
			while (dev->port->tfd & busy) __sync_synchronize();
			__sync_synchronize();
	
			dev->port->ci = (1 << slot);
			__sync_synchronize();
	
			while (dev->port->ci & (1 << slot))
			{
				if (dev->port->is & ((1 << 30) | (1 << 27) | (1 << 29) | (1 << 28)))
				{
					dev->port->is = dev->port->is;
					ahciStopCmd(dev->port);
					ahciStartCmd(dev->port);
					//memset(hwbuf, 0, 2048*cmd->count);
					//kprintf("TFD: %x\n", dev->port->tfd);
				};
				__sync_synchronize();
			};
			__sync_synchronize();

			//memcpy(cmd->block, hwbuf, 2048*cmd->count);
			sdPostComplete(cmd);
		}
		else if (cmd->type == SD_CMD_EJECT)
		{
			dev->port->is = dev->port->is;

			// first find a free command slot; we may wait for some time if the drive is
			// particularly busy.
			uint32_t slot = 0;
			while (dev->port->ci & (1 << slot))
			{
				slot = (slot + 1) % 32;
				__sync_synchronize();
			};
	
			// fill in the appropriate structure.
			// do not zero it; it was already zeroed before and only this thread updates the
			// structures so we know that any unused fields are already zero.
			cmdhead[slot].cfl = sizeof(FIS_REG_H2D)/4;
			cmdhead[slot].w = 0;
			cmdhead[slot].prdtl = 1;
			cmdhead[slot].a = 1;
			__sync_synchronize();
	
			AHCICommandTable *cmdtbl = (AHCICommandTable*) ((char*)dmaGetPtr(&dev->dmabuf)
							+ 1024+256+8*1024+256*slot);
			memset(cmdtbl->acmd, 0, 16);
			cmdtbl->acmd[0] = 0x1B;
			cmdtbl->acmd[4] = 2;
			
			cmdtbl->prdt_entry[0].dba = 0;
			cmdtbl->prdt_entry[0].dbc = 0;
			cmdtbl->prdt_entry[0].i = 0;
	
			FIS_REG_H2D *cmdfis = (FIS_REG_H2D*)(&cmdtbl->cfis);
			cmdfis->fis_type = FIS_TYPE_REG_H2D;
			cmdfis->c = 1;
			cmdfis->command = ATA_CMD_PACKET;
	
			cmdfis->lba0 = 0;
			cmdfis->lba1 = 0;
			cmdfis->lba2 = 0;
			cmdfis->device = 0;
		 
			cmdfis->lba3 = 0;
			cmdfis->lba4 = 0;
			cmdfis->lba5 = 0;
	
			cmdfis->countl = 0;
			cmdfis->counth = 0;
	
			// wait for the port to stop being busy
			int busy = (1 << 7) | (1 << 3);
			while (dev->port->tfd & busy) __sync_synchronize();
			__sync_synchronize();
	
			dev->port->ci = (1 << slot);
			__sync_synchronize();
	
			while (dev->port->ci & (1 << slot))
			{
				if (dev->port->is & ((1 << 30) | (1 << 27) | (1 << 29) | (1 << 28)))
				{
					dev->port->is = dev->port->is;
					ahciStopCmd(dev->port);
					ahciStartCmd(dev->port);
					//kprintf("TFD: %x\n", dev->port->tfd);
				};
				__sync_synchronize();
			};
			__sync_synchronize();

			sdPostComplete(cmd);
		};
	};
	
	sdHangup(dev->sd);
};

static void initCtrl(AHCIController *ctrl)
{
	pciSetBusMastering(ctrl->pcidev, 1);
	ctrl->regs = mapPhysMemory((uint64_t) ctrl->pcidev->bar[5], sizeof(AHCIMemoryRegs));
	ctrl->numAtaDevices = 0;
	
	uint32_t pi = ctrl->regs->pi;
	int i;
	
	for (i=0; i<32; i++)
	{
		if (pi&1)
		{
			volatile AHCIPort *port = &ctrl->regs->ports[i];
			
			uint32_t ssts = port->ssts;
			uint8_t ipm = (ssts >> 8) & 0x0F;
			uint8_t det = ssts & 0x0F;
			
			if (det == 3)
			{
				if (ipm == 1)
				{
					if (port->sig == AHCI_SIG_ATA)
					{
						kprintf("sdahci: found ATA device on port %d\n", i);
						ATADevice *dev = NEW(ATADevice);
						ctrl->ataDevices[ctrl->numAtaDevices++] = dev;
						dev->ctrl = ctrl;
						dev->port = port;
						ahciStopCmd(port);
						
						if (dmaCreateBuffer(&dev->dmabuf, 1024+256+8*1024+32*256, 0) != 0)
						{
							panic("failed to allocate AHCI DMA buffer");
						};
						
						if (dmaCreateBuffer(&dev->iobuf, 128*1024, 0) != 0)
						{
							panic("failed to allocate AHCI 128 KB I/O buffer");
						};
						
						memset(dmaGetPtr(&dev->dmabuf), 0, 1024+256+8*1024+32*256);
						__sync_synchronize();
						port->clb = dmaGetPhys(&dev->dmabuf);
						port->fb = dmaGetPhys(&dev->dmabuf) + 1024;
						
						__sync_synchronize();
						AHCICommandHeader *cmdhead = (AHCICommandHeader*) dmaGetPtr(&dev->dmabuf);
						
						int i;
						for (i=0; i<32; i++)
						{
							cmdhead[i].prdtl = 8;
							cmdhead[i].ctba = dmaGetPhys(&dev->dmabuf) + 1024 + 256 + 8*1024 + 256*i;
						};
						
						__sync_synchronize();
						ahciStartCmd(port);
						port->is = port->is;
						
						// we must now identify the device to find out its size
						// just use the first header
						cmdhead->cfl = sizeof(FIS_REG_H2D)/4;
						cmdhead->w = 0;
						cmdhead->prdtl = 1;
						cmdhead->p = 1;
						
						AHCICommandTable *cmdtbl =
							(AHCICommandTable*) ((char*)dmaGetPtr(&dev->dmabuf) + 1024+256+8*1024);
						memset(cmdtbl, 0, sizeof(AHCICommandTable));
						
						cmdtbl->prdt_entry[0].dba = dmaGetPhys(&dev->iobuf);
						cmdtbl->prdt_entry[0].dbc = 511;
						cmdtbl->prdt_entry[0].i = 0;	// interrupt when identify complete
						
						FIS_REG_H2D *cmdfis = (FIS_REG_H2D*) cmdtbl->cfis;
						memset(cmdfis, 0, sizeof(FIS_REG_H2D));
						cmdfis->fis_type = FIS_TYPE_REG_H2D;
						cmdfis->c = 1;
						cmdfis->command = ATA_CMD_IDENTIFY;

						__sync_synchronize();
						port->ci = 1;
						__sync_synchronize();
						
						while (1)
						{
							if ((port->ci & 1) == 0)
							{
								break;
							};
							
							__sync_synchronize();
						};
						
						uint8_t *defer = (uint8_t*) dmaGetPtr(&dev->iobuf);
						uint32_t *cmdsetptr = (uint32_t*) &defer[ATA_IDENT_COMMANDSETS];
						
						uint32_t cmdset = *cmdsetptr;
						uint64_t size;
						if (cmdset & (1 << 26))
						{
							uint64_t *szptr = (uint64_t*) &defer[ATA_IDENT_MAX_LBA_EXT];
							size = (*szptr) & 0xFFFFFFFFFFFF;
						}
						else
						{
							uint32_t *szptr = (uint32_t*) &defer[ATA_IDENT_MAX_LBA];
							size = (uint64_t) (*szptr);
						};
						
						kprintf("Size in MB: %d\n", (int) (size / 1024 / 2));
						
						SDParams pars;
						pars.flags = 0;
						pars.blockSize = 512;
						pars.totalSize = size * 512;
						
						dev->sd = sdCreate(&pars);
						if (dev->sd == NULL)
						{
							kprintf("sdahci: storage device creation failed!\n");
						}
						else
						{
							KernelThreadParams kpars;
							memset(&kpars, 0, sizeof(KernelThreadParams));
							kpars.stackSize = DEFAULT_STACK_SIZE;
							kpars.name = "AHCI ATA device";
							
							dev->ctlThread = CreateKernelThread(ahciAtaThread, &kpars, dev);
							kprintf("sdahci: ATA device initialized!\n");
						};
					}
					else if (port->sig == AHCI_SIG_ATAPI)
					{
						kprintf("sdahci: found ATAPI device on port %d\n", i);
						ATADevice *dev = NEW(ATADevice);
						ctrl->ataDevices[ctrl->numAtaDevices++] = dev;
						dev->ctrl = ctrl;
						dev->port = port;
						ahciStopCmd(port);
						
						if (dmaCreateBuffer(&dev->dmabuf, 1024+256+8*1024+32*256, 0) != 0)
						{
							panic("failed to allocate AHCI DMA buffer");
						};
						
						if (dmaCreateBuffer(&dev->iobuf, 128*1024, 0) != 0)
						{
							panic("failed to allocate AHCI 128 KB I/O buffer");
						};
						
						memset(dmaGetPtr(&dev->dmabuf), 0, 1024+256+8*1024+32*256);
						__sync_synchronize();
						port->clb = dmaGetPhys(&dev->dmabuf);
						port->fb = dmaGetPhys(&dev->dmabuf) + 1024;
						
						__sync_synchronize();
						AHCICommandHeader *cmdhead = (AHCICommandHeader*) dmaGetPtr(&dev->dmabuf);
						
						int i;
						for (i=0; i<32; i++)
						{
							cmdhead[i].prdtl = 8;
							cmdhead[i].ctba = dmaGetPhys(&dev->dmabuf) + 1024 + 256 + 8*1024 + 256*i;
						};
						
						__sync_synchronize();
						ahciStartCmd(port);
						port->is = port->is;
						
						SDParams pars;
						pars.flags = SD_READONLY;
						pars.blockSize = 2048;
						pars.totalSize = 0;
						
						dev->sd = sdCreate(&pars);
						if (dev->sd == NULL)
						{
							kprintf("sdahci: storage device creation failed!\n");
						}
						else
						{
							KernelThreadParams kpars;
							memset(&kpars, 0, sizeof(KernelThreadParams));
							kpars.stackSize = DEFAULT_STACK_SIZE;
							kpars.name = "AHCI ATAPI device";
							
							dev->ctlThread = CreateKernelThread(ahciAtapiThread, &kpars, dev);
							kprintf("sdahci: ATAPI device initialized!\n");
						};
					};
				};
			};
		};
		
		pi >>= 1;
	};
};

static int ahci_enumerator(PCIDevice *dev, void *ignore)
{
	if (dev->type == 0x0106)
	{
		kprintf("sdahci: found AHCI controller at %d/%d/%d\n", (int) dev->bus, (int) dev->slot, (int) dev->func);
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
	kprintf("sdahci: scanning for AHCI controllers\n");
	pciEnumDevices(THIS_MODULE, ahci_enumerator, NULL);
	
	kprintf("sdahci: found %d AHCI controllers, intializing\n", numCtrlFound);
	AHCIController *ctrl;
	for (ctrl=firstCtrl; ctrl!=NULL; ctrl=ctrl->next)
	{
		initCtrl(ctrl);
	};
	
	return MODINIT_OK;
};

MODULE_FINI()
{
	kprintf("sdahci: removing AHCI controllers...\n");
	AHCIController *ctrl = firstCtrl;
	while (ctrl != NULL)
	{
		kprintf("sdahci: removing controller with %d ATA devices\n", ctrl->numAtaDevices);
		
		int i;
		for (i=0; i<ctrl->numAtaDevices; i++)
		{
			ATADevice *dev = ctrl->ataDevices[i];
			sdSignal(dev->sd);
			ReleaseKernelThread(dev->ctlThread);
			dmaReleaseBuffer(&dev->dmabuf);
			dmaReleaseBuffer(&dev->iobuf);
			kfree(dev);
		};
		
		pciReleaseDevice(ctrl->pcidev);
		AHCIController *next = ctrl->next;
		kfree(ctrl);
		ctrl = next;
	};
	
	kprintf("sdahci: terminating\n");
	return 0;
};
