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

#ifndef VMGPU_H_
#define VMGPU_H_

#include <glidix/display/video.h>
#include <glidix/hw/pci.h>
#include <glidix/thread/mutex.h>

/**
 * PCI device ID.
 */
#define	VMGPU_PCI_VENDOR			0x15ad			/* VMWare */
#define	VMGPU_PCI_DEVICE			0x0405			/* SVGA II */

/**
 * Port offsets relative to iobase.
 */
#define VMGPU_INDEX_PORT			0x0
#define VMGPU_VALUE_PORT			0x1
#define VMGPU_BIOS_PORT				0x2
#define VMGPU_IRQSTATUS_PORT			0x8

/**
 * ID making.
 */
#define VMGPU_MAGIC				0x900000UL
#define VMGPU_MAKE_ID(ver)			(VMGPU_MAGIC << 8 | (ver))

/**
 * Hardware version supported by driver.
 */
#define VMGPU_ID_1				VMGPU_MAKE_ID(1)

/**
 * VMGPU registers.
 */
enum
{
	VMGPU_REG_ID = 0,
	VMGPU_REG_ENABLE = 1,
	VMGPU_REG_WIDTH = 2,
	VMGPU_REG_HEIGHT = 3,
	VMGPU_REG_MAX_WIDTH = 4,
	VMGPU_REG_MAX_HEIGHT = 5,
	VMGPU_REG_DEPTH = 6,
	VMGPU_REG_BITS_PER_PIXEL = 7,
	VMGPU_REG_PSEUDOCOLOR = 8,
	VMGPU_REG_RED_MASK = 9,
	VMGPU_REG_GREEN_MASK = 10,
	VMGPU_REG_BLUE_MASK = 11,
	VMGPU_REG_BYTES_PER_LINE = 12,
	VMGPU_REG_FB_START = 13,
	VMGPU_REG_FB_OFFSET = 14,
	VMGPU_REG_VRAM_SIZE = 15,
	VMGPU_REG_FB_SIZE = 16,
	VMGPU_REG_CAPABILITIES = 17,
	VMGPU_REG_MEM_START = 18,
	VMGPU_REG_MEM_SIZE = 19,
	VMGPU_REG_CONFIG_DONE = 20,
	VMGPU_REG_SYNC = 21,
	VMGPU_REG_BUSY = 22,
	VMGPU_REG_GUEST_ID = 23,
	VMGPU_REG_CURSOR_ID = 24,
	VMGPU_REG_CURSOR_X = 25,
	VMGPU_REG_CURSOR_Y = 26,
	VMGPU_REG_CURSOR_ON = 27,
	VMGPU_REG_HOST_BITS_PER_PIXEL = 28,
	VMGPU_REG_SCRATCH_SIZE = 29,
	VMGPU_REG_MEM_REGS = 30,
	VMGPU_REG_NUM_DISPLAYS = 31,
	VMGPU_REG_PITCHLOCK = 32,
	VMGPU_REG_IRQMASK = 33,
};

/**
 * FIFO registers.
 */
enum
{
	VMGPU_FIFO_MIN = 0,
	VMGPU_FIFO_MAX,
	VMGPU_FIFO_NEXT_CMD,
	VMGPU_FIFO_STOP,
};

/**
 * Describes a VMGPU device.
 */
typedef struct VMGPU_Device_
{
	struct VMGPU_Device_*			next;
	PCIDevice*				pcidev;
	VideoDisplay*				disp;
	uint16_t				iobase;
	Mutex					lock;
	uint32_t				fbufStart, fbufSize, fbufOffset;
	uint32_t*				fifo;
	uint32_t				fifoSize;
	uint32_t				maxWidth;
	uint32_t				maxHeight;
} VMGPU_Device;

/**
 * Write to a VMGPU register.
 */
void vmgpuRegWrite(VMGPU_Device *vmgpu, uint32_t regno, uint32_t value);

/**
 * Read from a VMGPU register.
 */
uint32_t vmgpuRegRead(VMGPU_Device *vmgpu, uint32_t regno);

#endif
