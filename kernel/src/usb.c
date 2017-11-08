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

#include <glidix/usb.h>
#include <glidix/string.h>
#include <glidix/memory.h>
#include <glidix/sched.h>
#include <glidix/console.h>

static uint8_t addrBitmap[16];
static USBCtrl ctrlNop;

static USBTransferInfo tiGetDeviceDescriptor[] = {
	{USB_SETUP, 0, sizeof(USBSetupPacket)},
	{USB_IN, 1, sizeof(USBDeviceDescriptor)},
	{USB_OUT, 0, 0},
	{USB_END}
};

void usbUp(USBDevice *dev)
{
	__sync_fetch_and_add(&dev->refcount, 1);
};

void usbDown(USBDevice *dev)
{
	if (__sync_add_and_fetch(&dev->refcount, -1) == 0)
	{
		usbReleaseAddr(dev->addr);
		kfree(dev);
	};
};

void usbDevInitFunc(void *context)
{
	// the device is alread upreffered for us
	USBDevice *dev = (USBDevice*) context;
	
	// detach us. the idea is that this thread gets spawned upon detection of a device,
	// and we look for the appropriate driver to handle the device. if the driver is found,
	// then this thread shall continue to exist, running the driver code, managing the device,
	// and exiting once the device is disconnected. if the driver isn't found, we just exit.
	detachMe();
	
	// set up the default control pipe
	USBControlPipe *pipe = usbCreateControlPipe(dev, 0, 64);
	if (pipe == NULL)
	{
		kprintf_debug("usb: failed to create default control pipe\n");
		usbDown(dev);
		return;
	};
	
	// get the device descriptor
	struct
	{
		USBSetupPacket setup;
		USBDeviceDescriptor desc;
	} req;
	
	memset(&req, 0, sizeof(req));
	req.setup.bmRequestType = USB_REQ_DEVICE_TO_HOST | USB_REQ_STANDARD | USB_REQ_DEVICE;
	req.setup.bRequest = USB_REQ_GET_DESCRIPTOR;
	req.setup.wValue = 0x0100;				// device descriptor zero
	req.setup.wIndex = 0;
	req.setup.wLength = sizeof(USBDeviceDescriptor);
	
	Semaphore semComplete;
	semInit2(&semComplete, 0);
	USBRequest *urb = usbCreateControlRequest(pipe, tiGetDeviceDescriptor, &req, &semComplete);
	if (urb == NULL)
	{
		kprintf_debug("usb: failed to create GET_DESCRIPTOR request\n");
		usbDeleteControlPipe(pipe);
		usbDown(dev);
		return;
	};
	
	int status = usbSubmitRequest(urb);
	if (status != 0)
	{
		kprintf_debug("usb: failed to submit GET_DESCRIPTOR request: status %d\n", status);
		usbDeleteRequest(urb);
		usbDeleteControlPipe(pipe);
		usbDown(dev);
		return;
	};
	
	semWait(&semComplete);
	status = usbGetRequestStatus(urb);
	if (status != 0)
	{
		kprintf_debug("usb: execution of GET_DESCRIPTOR failed: status %d\n", status);
		usbDeleteRequest(urb);
		usbDeleteControlPipe(pipe);
		usbDown(dev);
		return;
	};
	
	kprintf("i got all the info!\n");
};

void usbInit()
{
	addrBitmap[0] = 1;		// address 0 reserved
};

usb_addr_t usbAllocAddr()
{
	usb_addr_t i;
	for (i=1; i<128; i++)
	{
		uint8_t index = i / 8;
		uint8_t mask = (1 << (i % 8));
		
		if ((__sync_fetch_and_or(&addrBitmap[index], mask) & mask) == 0)
		{
			break;
		};
	};
	
	return i;
};

void usbReleaseAddr(usb_addr_t addr)
{
	uint8_t index = addr / 8;
	uint8_t mask = (1 << (addr % 8));
	__sync_fetch_and_and(&addrBitmap[index], ~mask);
};

USBDevice* usbCreateDevice(usb_addr_t addr, USBCtrl *ctrl, int flags, void *data, int usbver)
{
	USBDevice *dev = NEW(USBDevice);
	dev->addr = addr;
	dev->ctrl = ctrl;
	dev->flags = flags;
	dev->data = data;
	dev->usbver = usbver;
	dev->refcount = 2;		/* one returned, one for the init thread */
	semInit(&dev->lock);

	KernelThreadParams pars;
	memset(&pars, 0, sizeof(KernelThreadParams));
	pars.stackSize = DEFAULT_STACK_SIZE;
	pars.name = "USB Device Initializer Thread";
	CreateKernelThread(usbDevInitFunc, &pars, dev);
	
	return dev;
};

void usbHangup(USBDevice *dev)
{
	semWait(&dev->lock);
	dev->flags |= USB_DEV_HANGUP;
	dev->ctrl = &ctrlNop;
	semSignal(&dev->lock);
	
	usbDown(dev);
};

USBControlPipe* usbCreateControlPipe(USBDevice *dev, usb_epno_t epno, size_t maxPacketLen)
{
	// create the pipe driver data
	semWait(&dev->lock);
	if (dev->ctrl->createControlPipe == NULL)
	{
		semSignal(&dev->lock);
		return NULL;
	};
	
	void *pipedata = dev->ctrl->createControlPipe(dev, epno, maxPacketLen);
	semSignal(&dev->lock);
	
	if (pipedata == NULL)
	{
		return NULL;
	};
	
	// upref the device to place into the pipe object
	usbUp(dev);
	
	// create the pipe
	USBControlPipe *pipe = NEW(USBControlPipe);
	pipe->dev = dev;
	pipe->data = pipedata;
	
	return pipe;
};

void usbDeleteControlPipe(USBControlPipe *pipe)
{
	USBDevice *dev = pipe->dev;
	
	semWait(&dev->lock);
	if (dev->ctrl->deleteControlPipe != NULL)
	{
		dev->ctrl->deleteControlPipe(pipe->data);
	};
	semSignal(&dev->lock);
	
	usbDown(dev);
	kfree(pipe);
};

USBRequest* usbCreateControlRequest(USBControlPipe *pipe, USBTransferInfo *packets, void *buffer, Semaphore *semComplete)
{
	USBRequest *urb = (USBRequest*) kmalloc(sizeof(urb->control));
	memset(urb, 0, sizeof(urb->control));
	urb->header.type = USB_TRANS_CONTROL;
	urb->header.semComplete = semComplete;
	urb->header.dev = pipe->dev;
	usbUp(pipe->dev);
	urb->header.size = sizeof(urb->control);
	
	urb->control.pipe = pipe;
	urb->control.packets = packets;
	urb->control.buffer = buffer;
	
	return urb;
};

int usbSubmitRequest(USBRequest *urb)
{
	USBCtrl *ctrl = urb->header.dev->ctrl;
	if (ctrl->submit == NULL)
	{
		return ENOSYS;
	};
	
	return ctrl->submit(urb);
};

void usbDeleteRequest(USBRequest *urb)
{
	kfree(urb);
};

void usbPostComplete(USBRequest *urb)
{
	if (urb->header.semComplete != NULL) semSignal(urb->header.semComplete);
};

int usbGetRequestStatus(USBRequest *urb)
{
	return urb->header.status;
};
