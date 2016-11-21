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

#include <glidix/usb.h>
#include <glidix/string.h>
#include <glidix/memory.h>

static Semaphore usbLock;
static USBDevice *firstDevice;
static uint8_t addrBitmap[16];

static uint8_t allocAddr()
{
	// never allocate address 0!
	int i;
	for (i=1; i<=127; i++)
	{
		int byte = i/8;
		int bit = i%8;
		
		if (atomic_test_and_set8(&addrBitmap[byte], bit) == 0)
		{
			return (uint8_t) i;
		};
	};
	
	return 0;
};

void usbInit()
{
	semInit(&usbLock);
	firstDevice = NULL;
	memset(addrBitmap, 0, 16);
};

static void usbDownref(USBDevice *dev)
{
	if (__sync_add_and_fetch(&dev->refcount, -1) == 0)
	{
		kfree(dev);
	};
};

USBDevice* usbCreateDevice()
{
	uint8_t faddr = allocAddr();
	if (faddr == 0)
	{
		return NULL;
	};
	
	USBDevice *dev = NEW(USBDevice);
	memset(dev, 0, sizeof(USBDevice));
	dev->size = sizeof(USBDevice);
	dev->faddr = faddr;
	return dev;
};

void usbPostDevice(USBDevice *dev)
{
	semInit2(&dev->semStatus, 0);
	dev->refcount = 1;
	
	semWait(&usbLock);
	if (firstDevice == NULL)
	{
		dev->prev = dev->next = NULL;
		firstDevice = dev;
	}
	else
	{
		dev->prev = NULL;
		firstDevice->prev = dev;
		dev->next = firstDevice;
		firstDevice = dev;
	};
	semSignal(&usbLock);
};

void usbDisconnect(USBDevice *dev)
{
	semWait(&usbLock);
	if (dev->prev != NULL) dev->prev->next = dev->next;
	if (dev->next != NULL) dev->next->prev = dev->prev;
	if (firstDevice == dev) firstDevice = dev->next;
	semSignal(&usbLock);
	
	__sync_fetch_and_or(&dev->status, USB_DISCONNECTED);
	
	uint8_t byte = dev->faddr / 8;
	uint8_t bit = dev->faddr % 8;
	uint8_t mask = (1 << bit);
	__sync_fetch_and_and(&addrBitmap[byte], ~mask);
	
	usbDownref(dev);
};
