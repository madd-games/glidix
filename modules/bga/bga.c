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

#include <glidix/common.h>
#include <glidix/memory.h>
#include <glidix/string.h>
#include <glidix/video.h>
#include <glidix/devfs.h>
#include <glidix/sched.h>
#include <glidix/errno.h>
#include <glidix/port.h>
#include <glidix/pci.h>
#include <glidix/console.h>

#define VBE_DISPI_TOTAL_VIDEO_MEMORY_MB  16
#define VBE_DISPI_4BPP_PLANE_SHIFT       22

#define VBE_DISPI_BANK_ADDRESS           0xA0000
#define VBE_DISPI_BANK_SIZE_KB           64

#define VBE_DISPI_MAX_XRES               2560
#define VBE_DISPI_MAX_YRES               1600
#define VBE_DISPI_MAX_BPP                32

#define VBE_DISPI_IOPORT_INDEX           0x01CE
#define VBE_DISPI_IOPORT_DATA            0x01CF

#define VBE_DISPI_INDEX_ID               0x0
#define VBE_DISPI_INDEX_XRES             0x1
#define VBE_DISPI_INDEX_YRES             0x2
#define VBE_DISPI_INDEX_BPP              0x3
#define VBE_DISPI_INDEX_ENABLE           0x4
#define VBE_DISPI_INDEX_BANK             0x5
#define VBE_DISPI_INDEX_VIRT_WIDTH       0x6
#define VBE_DISPI_INDEX_VIRT_HEIGHT      0x7
#define VBE_DISPI_INDEX_X_OFFSET         0x8
#define VBE_DISPI_INDEX_Y_OFFSET         0x9
#define VBE_DISPI_INDEX_VIDEO_MEMORY_64K 0xa

#define VBE_DISPI_ID0                    0xB0C0
#define VBE_DISPI_ID1                    0xB0C1
#define VBE_DISPI_ID2                    0xB0C2
#define VBE_DISPI_ID3                    0xB0C3
#define VBE_DISPI_ID4                    0xB0C4
#define VBE_DISPI_ID5                    0xB0C5

#define VBE_DISPI_BPP_4                  0x04
#define VBE_DISPI_BPP_8                  0x08
#define VBE_DISPI_BPP_15                 0x0F
#define VBE_DISPI_BPP_16                 0x10
#define VBE_DISPI_BPP_24                 0x18
#define VBE_DISPI_BPP_32                 0x20

#define VBE_DISPI_DISABLED               0x00
#define VBE_DISPI_ENABLED                0x01
#define VBE_DISPI_GETCAPS                0x02
#define VBE_DISPI_8BIT_DAC               0x20
#define VBE_DISPI_LFB_ENABLED            0x40
#define VBE_DISPI_NOCLEARMEM             0x80

#define	VBE_DISPI_IOPORT_INDEX		0x01CE
#define	VBE_DISPI_IOPORT_DATA		0x01CF

static int bgaFound = 0;
static VideoDisplay *bgaDisp = NULL;
static uint64_t fbufAddr;
static size_t fbufSize;

static void bgaWriteRegister(unsigned short IndexValue, unsigned short DataValue)
{
	outw(VBE_DISPI_IOPORT_INDEX, IndexValue);
	outw(VBE_DISPI_IOPORT_DATA, DataValue);
}
 
unsigned short bgaReadRegister(unsigned short IndexValue)
{
	outw(VBE_DISPI_IOPORT_INDEX, IndexValue);
	return inw(VBE_DISPI_IOPORT_DATA);
}

static int bga_setmode(VideoDisplay *disp, VideoModeRequest *req)
{
	int width, height;

	if (req->res == VIDEO_RES_AUTO)
	{
		width = 720;
		height = 480;
	}
	else if (req->res == VIDEO_RES_SAFE)
	{
		width = 720;
		height = 480;
	}
	else
	{
		width = VIDEO_RES_WIDTH(req->res);
		height = VIDEO_RES_HEIGHT(req->res);
	};
	
	fbufSize = (width * height * 4 + 0xFFF) & ~0xFFF;
	
	bgaWriteRegister(VBE_DISPI_INDEX_ENABLE, VBE_DISPI_DISABLED);
	bgaWriteRegister(VBE_DISPI_INDEX_BPP, VBE_DISPI_BPP_32);
	bgaWriteRegister(VBE_DISPI_INDEX_XRES, width);
	bgaWriteRegister(VBE_DISPI_INDEX_YRES, height);
	bgaWriteRegister(VBE_DISPI_INDEX_ENABLE, VBE_DISPI_ENABLED | VBE_DISPI_LFB_ENABLED);
	
	return 0;
};

static int bga_getflags(VideoDisplay *disp)
{
	return 0;
};

static uint64_t bga_getfbuf(VideoDisplay *disp, size_t *sizeOut)
{
	*sizeOut = fbufSize;
	return fbufAddr;
};

VideoOps bgaOps = {
	.setmode = bga_setmode,
	.getflags = bga_getflags,
	.getfbuf = bga_getfbuf
};

static void initBGA(uint32_t bar0)
{
	fbufAddr = bar0 & ~0xF;
	
	VideoDriver *drv = videoCreateDriver("bga");
	bgaDisp = videoCreateDisplay(drv, NULL, &bgaOps);
	videoDeleteDriver(drv);
};

static int bga_enumerator(PCIDevice *dev, void *ignore)
{
	(void)ignore;
	
	if (
		((dev->vendor == 0x1234) && (dev->device == 0x1111))
	||	((dev->vendor == 0x80EE) && (dev->device == 0xBEEF))
	)
	{
		kprintf("bga: found BGA\n");
		bgaFound = 1;
		strcpy(dev->deviceName, "Bochs Graphics Adapter (BGA)");
		initBGA(dev->bar[0]);
		return 1;
	};
	
	return 0;
};

MODULE_INIT()
{
	bgaFound = 0;
	fbufSize = 0;
	kprintf("bga: enumerating BGA-compatible devices\n");
	pciEnumDevices(THIS_MODULE, bga_enumerator, NULL);

	if (!bgaFound)
	{
		kprintf("bga: device not found\n");
		return MODINIT_CANCEL;
	};

	return MODINIT_OK;
};

MODULE_FINI()
{
	if (bgaDisp != NULL)
	{
		videoDeleteDisplay(bgaDisp);
	};
	
	return 0;
};
