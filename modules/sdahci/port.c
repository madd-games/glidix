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

#include "port.h"
#include "sdahci.h"
#include "ata.h"
#include "atapi.h"

static void portStart(AHCIPort *port)
{
	port->regs->cmd = (port->regs->cmd & ~CMD_ICC_MASK) | CMD_SUD | CMD_POD | CMD_FRE | CMD_ICC_ACTIVE;
	while ((port->regs->cmd & CMD_FRE) == 0);
};

static void portStop(AHCIPort *port)
{
	port->regs->cmd &= ~CMD_ST;
	port->regs->cmd &= ~CMD_FRE;
	port->regs->cmd &= ~CMD_ICC_MASK;

	while (port->regs->cmd & CMD_FR);
	while (port->regs->cmd & CMD_CR);

	port->regs->sctl = 0;
	port->regs->serr = -1;
};

int portIssueCmd(AHCIPort *port)
{
	uint64_t startTime = getNanotime();

	if (port->regs->tfd & STS_BSY)
	{
		panic("sdahci: device is busy prior to issuing a command!");
	};
	
	port->regs->is = port->regs->is;
	port->regs->ci = 1;
	
	while (1)
	{
		if (getNanotime()-startTime > 8*NANO_PER_SEC)
		{
			// taking longer than 8 seconds
			kprintf("sdahci: timeout; aborting command. IS=0x%08X, SERR=0x%08X, TFD=0x%08X\n", port->regs->is, port->regs->serr, port->regs->tfd);
			portStop(port);
			portStart(port);
			port->regs->serr = port->regs->serr;
			return EIO;
		};
		
		if (port->regs->is & IS_ERR_FATAL)
		{
			// a fatal error occured
			kprintf("sdahci: fatal error. IS=0x%08X, SERR=0x%08X\n", port->regs->is, port->regs->serr);
			
			portStop(port);
			portStart(port);
			port->regs->serr = port->regs->serr;
			return EIO;
		};
		
		if ((port->regs->ci & 1) == 0)
		{
			break;
		};
	};
	
	int busy = STS_BSY | STS_DRQ;
	while (port->regs->tfd & busy)
	{
		if (getNanotime()-startTime > 8*NANO_PER_SEC)
		{
			kprintf("sdahci: timeout; aborting command. IS=0x%08X, SERR=0x%08X, TFD=0x%08X\n", port->regs->is, port->regs->serr, port->regs->tfd);
			portStop(port);
			portStart(port);
			port->regs->serr = port->regs->serr;
			return EIO;
		};
	};
	
	if (port->regs->tfd & STS_ERR)
	{
		return EIO;
	};
	
	return 0;
};

static void portReset(AHCIPort *port)
{
	port->regs->sctl = 0;
	port->regs->sctl = SCTL_DET_COMRESET;

	sleep(10);

	port->regs->sctl = 0;

	sleep(10);

	port->regs->serr = port->regs->serr;
	port->regs->is = port->regs->is;
};

void portInit(AHCIController *ctrl, int portno)
{
	AHCIPort *port = NEW(AHCIPort);
	port->ctrl = ctrl;
	port->portno = portno;
	port->regs = &ctrl->regs->ports[portno];
	port->sd = NULL;
	mutexInit(&port->lock);

	portStop(port);

	// Allocate the operations area.
	if (dmaCreateBuffer(&port->dmabuf, sizeof(AHCIOpArea), 0) != 0)
	{
		kprintf("sdahci: failed to allocate operations area\n");
		kfree(port);
		return;
	};
	
	// Set up the operations area.
	AHCIOpArea *opArea = (AHCIOpArea*) dmaGetPtr(&port->dmabuf);
	memset(opArea, 0, sizeof(AHCIOpArea));
	
	// Set the command list and FIS area.
	port->regs->clb = dmaGetPhys(&port->dmabuf) + __builtin_offsetof(AHCIOpArea, cmdlist);
	port->regs->fb = dmaGetPhys(&port->dmabuf) + __builtin_offsetof(AHCIOpArea, fisArea);
	
	// We only use the first command header, so initialize it to point to the table.
	opArea->cmdlist[0].ctba = dmaGetPhys(&port->dmabuf) + __builtin_offsetof(AHCIOpArea, cmdtab);

	portStart(port);
	portReset(port);
	portStart(port);
	port->regs->cmd |= CMD_ST;

	uint32_t ssts = port->regs->ssts;
	
	uint8_t ipm = (ssts >> 8) & 0x0F;
	uint8_t det = ssts & 0x0F;
	
	if (det != SSTS_DET_OK)
	{
		portRelease(port);
		return;
	};
	
	if (ipm != SSTS_IPM_ACTIVE)
	{
		portRelease(port);
		return;
	};

	ctrl->ports[ctrl->numPorts++] = port;

	uint32_t sig = port->regs->sig;
	if (sig == AHCI_SIG_ATA)
	{
		kprintf("sdahci: detected ATA drive on port %d\n", portno);
		ataInit(port);
	}
	else if (sig == AHCI_SIG_ATAPI)
	{
		kprintf("sdahci: detected ATAPI drive on port %d\n", portno);
		atapiInit(port);
	}
	else if (sig == AHCI_SIG_EMPTY)
	{
		kprintf(
			"sdahci: port %d reporing empty signature: serr=0x%08x, tfd=0x%08x\n",
			portno, port->regs->serr, port->regs->tfd
		);
	}
	else
	{
		kprintf("sdahci: unknown device: signature 0x%08X (port %d)\n", sig, portno);
	};
};

void portRelease(AHCIPort *port)
{
	if (port->sd != NULL) sdHangup(port->sd);
	portStop(port);
	dmaReleaseBuffer(&port->dmabuf);
};