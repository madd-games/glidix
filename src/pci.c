/*
	Glidix kernel

	Copyright (c) 2014-2015, Madd Games.
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

#include <glidix/pci.h>
#include <glidix/port.h>
#include <glidix/devfs.h>
#include <glidix/vfs.h>
#include <glidix/spinlock.h>
#include <glidix/string.h>
#include <glidix/sched.h>
#include <glidix/errno.h>

static Spinlock pciLock;

int pci_ioctl(File *fp, uint64_t cmd, void *argp)
{
	if (cmd == IOCTL_PCI_DEVINFO)
	{
		PCIDevinfoRequest *rq = (PCIDevinfoRequest*) argp;
		PCIDeviceConfig config;
		pciGetDeviceConfig(rq->bus, rq->slot, rq->func, &config);
		memcpy(&rq->config, &config, sizeof(PCIDeviceConfig));
		return 0;
	}
	else
	{
		getCurrentThread()->therrno = ENOTTY;
		return -1;
	};
};

int openpcictl(void *data, File *fp, size_t szFile)
{
	fp->ioctl = pci_ioctl;
	return 0;
};

void pciInit()
{
	spinlockRelease(&pciLock);
	if (AddDevice("pcictl", NULL, openpcictl, 0) == NULL)
	{
		panic("failed to create /dev/pcictl");
	};
};

void pciGetDeviceConfig(uint8_t bus, uint8_t slot, uint8_t func, PCIDeviceConfig *config)
{
	spinlockAcquire(&pciLock);
	uint32_t addr;
	uint32_t lbus = (uint32_t) bus;
	uint32_t lslot = (uint32_t) slot;
	uint32_t lfunc = (uint32_t) func;
	addr = (lbus << 16) | (lslot << 11) | (lfunc << 8) | (1 << 31);

	uint32_t *put = (uint32_t*) config;
	int count = 16;

	while (count--)
	{
		outd(PCI_CONFIG_ADDR, addr);
		*put++ = ind(PCI_CONFIG_DATA);
		addr += 4;
	};
	spinlockRelease(&pciLock);
};
