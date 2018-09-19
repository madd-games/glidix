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

/**
 * VMWare SVGA-II GPU driver.
 *
 * This is a virtual GPU available in some virtual machines such as VMWare and QEMU. The aim of this driver
 * is to create a reasonable GPU infrastructure within Glidix, before we proceed to create proper GPU drivers.
 * The card supports hardware acceleration, and so we must prepare the system and applications to work properly
 * with acceleration.
 */

#include <glidix/module/module.h>
#include <glidix/hw/pci.h>
#include <glidix/display/console.h>
#include <glidix/util/string.h>
#include <glidix/util/memory.h>
#include <glidix/hw/port.h>

#include "vmgpu.h"

static VideoDriver *vmgpuVideoDrv;
static VMGPU_Device *firstDev;
static VMGPU_Device *lastDev;

void vmgpuRegWrite(VMGPU_Device *vmgpu, uint32_t regno, uint32_t value)
{
	mutexLock(&vmgpu->lock);
	outd(vmgpu->iobase + VMGPU_INDEX_PORT, regno);
	outd(vmgpu->iobase + VMGPU_VALUE_PORT, value);
	mutexUnlock(&vmgpu->lock);
};

uint32_t vmgpuRegRead(VMGPU_Device *vmgpu, uint32_t regno)
{
	mutexLock(&vmgpu->lock);
	outd(vmgpu->iobase + VMGPU_INDEX_PORT, regno);
	uint32_t result = ind(vmgpu->iobase + VMGPU_VALUE_PORT);
	mutexUnlock(&vmgpu->lock);
	return result;
};

static int vmgpuSetMode(VideoDisplay *disp, VideoModeRequest *req)
{
	VMGPU_Device *vmgpu = (VMGPU_Device*) disp->data;
	
	// console must be disabled
	disableConsole();
	vmgpuRegWrite(vmgpu, VMGPU_REG_CONFIG_DONE, 0);
	vmgpuRegWrite(vmgpu, VMGPU_REG_ENABLE, 0);
	
	// set resolution
	if (req->res == VIDEO_RES_AUTO || req->res == VIDEO_RES_SAFE)
	{
		req->res = VIDEO_RES_SPECIFIC(640UL, 480UL);
	};
	
	uint32_t width = VIDEO_RES_WIDTH(req->res);
	uint32_t height = VIDEO_RES_HEIGHT(req->res);
	
	vmgpuRegWrite(vmgpu, VMGPU_REG_WIDTH, width);
	vmgpuRegWrite(vmgpu, VMGPU_REG_HEIGHT, height);
	vmgpuRegWrite(vmgpu, VMGPU_REG_BITS_PER_PIXEL, 32);
	
	vmgpuRegWrite(vmgpu, VMGPU_REG_ENABLE, 1);
	
	vmgpu->fbufStart = vmgpuRegRead(vmgpu, VMGPU_REG_FB_START);
	vmgpu->fbufSize = vmgpuRegRead(vmgpu, VMGPU_REG_FB_SIZE);
	vmgpu->fbufOffset = vmgpuRegRead(vmgpu, VMGPU_REG_FB_OFFSET);
	
	// configure the FIFO
	vmgpu->fifo[VMGPU_FIFO_MIN] = 16;
	vmgpu->fifo[VMGPU_FIFO_MAX] = 16 + (10 * 1024);
	vmgpu->fifo[VMGPU_FIFO_NEXT_CMD] = 16;
	vmgpu->fifo[VMGPU_FIFO_STOP] = 16;
	
	vmgpuRegWrite(vmgpu, VMGPU_REG_CONFIG_DONE, 1);
	
	// figure out the pixel format
	PixelFormat format;
	format.bpp = 4;
	format.redMask = vmgpuRegRead(vmgpu, VMGPU_REG_RED_MASK);
	format.greenMask = vmgpuRegRead(vmgpu, VMGPU_REG_GREEN_MASK);
	format.blueMask = vmgpuRegRead(vmgpu, VMGPU_REG_BLUE_MASK);
	format.alphaMask = ~(format.redMask | format.greenMask | format.blueMask);
	format.pixelSpacing = 0;
	format.scanlineSpacing = vmgpuRegRead(vmgpu, VMGPU_REG_BYTES_PER_LINE) - (4 * width);
	
	setConsoleFrameBuffer(mapPhysMemory(vmgpu->fbufStart, vmgpu->fbufSize), &format, width, height);
	memcpy(&req->format, &format, sizeof(PixelFormat));
	return 0;
};

static void vmgpuGetInfo(VideoDisplay *disp, VideoInfo *info)
{
	strcpy(info->renderer, "vmrender");
	info->numPresentable = 1;
	info->features = 0;
};

static uint64_t vmgpuGetPage(VideoDisplay *disp, off_t off)
{
	VMGPU_Device *vmgpu = (VMGPU_Device*) disp->data;
	off += vmgpu->fbufOffset;
	
	if (off > vmgpu->fbufSize) return 0;
	else return (vmgpu->fbufStart + off) >> 12;
};

static VideoOps vmgpuVideoOps = {
	.setmode = vmgpuSetMode,
	.getinfo = vmgpuGetInfo,
	.getpage = vmgpuGetPage,
};

static int vmgpu_enumerator(PCIDevice *dev, void *ignore)
{
	if (dev->vendor == VMGPU_PCI_VENDOR && dev->device == VMGPU_PCI_DEVICE)
	{
		strcpy(dev->deviceName, "VMWave SVGA-II GPU");
		
		VMGPU_Device *vmgpu = NEW(VMGPU_Device);
		memset(vmgpu, 0, sizeof(VMGPU_Device));
		vmgpu->pcidev = dev;
		
		if (firstDev == NULL)
		{
			firstDev = lastDev = vmgpu;
		}
		else
		{
			lastDev->next = vmgpu;
			lastDev = vmgpu;
		};
		
		return 1;
	};
	
	return 0;
};

MODULE_INIT(const char *opt)
{
	vmgpuVideoDrv = videoCreateDriver("vmgpu");
	firstDev = NULL;
	lastDev = NULL;
	
	kprintf("vmgpu: enumerating VMGPU devices\n");
	pciEnumDevices(THIS_MODULE, vmgpu_enumerator, NULL);
	
	kprintf("vmgpu: initializing\n");
	VMGPU_Device *vmgpu;
	for (vmgpu=firstDev; vmgpu!=NULL; vmgpu=vmgpu->next)
	{
		vmgpu->iobase = vmgpu->pcidev->bar[0] & 0xFFFFFFFC;
		uint32_t fifoPhys = vmgpu->pcidev->bar[1] & ~0xF;
		vmgpu->fifoSize = 16 + (10 * 1024);
		vmgpu->fifo = (uint32_t*) mapPhysMemory(fifoPhys, vmgpu->fifoSize);
		mutexInit(&vmgpu->lock);
		
		// make sure it supports hardware ID 1
		vmgpuRegWrite(vmgpu, VMGPU_REG_ID, VMGPU_ID_1);
		if (vmgpuRegRead(vmgpu, VMGPU_REG_ID) != VMGPU_ID_1)
		{
			kprintf("vmgpu: hardware ID 1 not supported\n");
			continue;
		};

		// get information
		uint32_t maxWidth = vmgpuRegRead(vmgpu, VMGPU_REG_MAX_WIDTH);
		uint32_t maxHeight = vmgpuRegRead(vmgpu, VMGPU_REG_MAX_HEIGHT);
		
		kprintf("vmgpu: maximum dimensions: %ux%u\n", maxWidth, maxHeight);
		
		vmgpu->maxWidth = maxWidth;
		vmgpu->maxHeight = maxHeight;
	
		// OK, we can create the display
		vmgpu->disp = videoCreateDisplay(vmgpuVideoDrv, vmgpu, &vmgpuVideoOps);
	};
	
	return MODINIT_OK;
};

MODULE_FINI()
{
	kprintf("vmgpu: removing GPU devices\n");
	
	while (firstDev != NULL)
	{
		VMGPU_Device *vmgpu = firstDev;
		firstDev = vmgpu->next;
		
		vmgpuRegWrite(vmgpu, VMGPU_REG_CONFIG_DONE, 0);
		vmgpuRegWrite(vmgpu, VMGPU_REG_ENABLE, 0);
		unmapPhysMemory(vmgpu->fifo, vmgpu->fifoSize);
		videoDeleteDisplay(vmgpu->disp);
		pciReleaseDevice(vmgpu->pcidev);
		kfree(vmgpu);
	};
	
	videoDeleteDriver(vmgpuVideoDrv);
	return 0;
};
