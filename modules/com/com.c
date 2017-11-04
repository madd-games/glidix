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
 * Serial port driver.
 */

#include <glidix/devfs.h>
#include <glidix/common.h>
#include <glidix/module.h>
#include <glidix/port.h>
#include <glidix/memory.h>
#include <glidix/string.h>

uint16_t comPorts[4] = {0x3F8, 0x2F8, 0x3E8, 0x2E8};
const char *comNames[4] = {"com1", "com2", "com3", "com4"};
Device comDevs[4];

static void com_close(File *fp)
{
	// NOP
};

static ssize_t com_write(File *fp, const void *buffer, size_t size)
{
	const uint8_t *fetch = (const uint8_t*) buffer;
	ssize_t sizeOut = 0;
	while (size--)
	{
		while ((inb(*((uint16_t*)fp->fsdata) + 5) & 0x20) == 0);
		outb(*((uint16_t*)fp->fsdata), *fetch++);
		sizeOut++;
	};
	
	return sizeOut;
};

static ssize_t com_pwrite(File *fp, const void *buffer, size_t size, off_t pos)
{
	(void)pos;
	const uint8_t *fetch = (const uint8_t*) buffer;
	ssize_t sizeOut = 0;
	while (size--)
	{
		while ((inb(*((uint16_t*)fp->fsdata) + 5) & 0x20) == 0);
		outb(*((uint16_t*)fp->fsdata), *fetch++);
		sizeOut++;
	};
	
	return sizeOut;
};

static int com_open(void *data, File *fp, size_t szFile)
{
	fp->fsdata = data;
	fp->write = com_write;
	fp->pwrite = com_pwrite;
	fp->close = com_close;
	
	return 0;
};

MODULE_INIT()
{
	int i;
	for (i=0; i<4; i++)
	{
		outb(comPorts[i] + 1, 0x00);	 // Disable all interrupts
		outb(comPorts[i] + 3, 0x80);	 // Enable DLAB (set baud rate divisor)
		outb(comPorts[i] + 0, 0x03);	 // Set divisor to 3 (lo byte) 38400 baud
		outb(comPorts[i] + 1, 0x00);	 //                  (hi byte)
		outb(comPorts[i] + 3, 0x03);	 // 8 bits, no parity, one stop bit
		outb(comPorts[i] + 2, 0xC7);	 // Enable FIFO, clear them, with 14-byte threshold
		outb(comPorts[i] + 4, 0x0B);	 // IRQs enabled, RTS/DSR set
	};
	
	for (i=0; i<4; i++)
	{
		// because devfs frees the 'data' pointer...
		uint16_t *ptr = NEW(uint16_t);
		*ptr = comPorts[i];
		comDevs[i] = AddDevice(comNames[i], ptr, com_open, 0666);
	};
	
	return MODINIT_OK;
};

MODULE_FINI()
{
	int i;
	
	for (i=0; i<4; i++)
	{
		DeleteDevice(comDevs[i]);
	};
	
	return 0;
};
