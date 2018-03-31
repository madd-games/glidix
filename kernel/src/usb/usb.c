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

#include <glidix/usb/usb.h>
#include <glidix/util/string.h>
#include <glidix/util/memory.h>
#include <glidix/thread/sched.h>
#include <glidix/display/console.h>

static uint8_t addrBitmap[16];
static USBCtrl ctrlNop;

static USBTransferInfo tiGetDeviceDescriptor[] = {
	{USB_SETUP, 0, sizeof(USBSetupPacket)},
	{USB_IN, 1, sizeof(USBDeviceDescriptor)},
	{USB_OUT, 0, 0},
	{USB_END}
};

static USBTransferInfo tiGetConfigurationDescriptor[] = {
	{USB_SETUP, 0, sizeof(USBSetupPacket)},
	{USB_IN, 1, sizeof(USBConfigurationDescriptor)},
	{USB_OUT, 0, 0},
	{USB_END}
};

static USBTransferInfo tiSetConfig[] = {
	{USB_SETUP, 0, sizeof(USBSetupPacket)},
	{USB_IN, 1, 0},
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
	
	dev->ctrlPipe = pipe;
	
	// get the device descriptor
	struct
	{
		USBSetupPacket setup;
		USBDeviceDescriptor desc;
	} reqDev;
	
	memset(&reqDev, 0, sizeof(reqDev));
	reqDev.setup.bmRequestType = USB_REQ_DEVICE_TO_HOST | USB_REQ_STANDARD | USB_REQ_DEVICE;
	reqDev.setup.bRequest = USB_REQ_GET_DESCRIPTOR;
	reqDev.setup.wValue = 0x0100;				// device descriptor zero
	reqDev.setup.wIndex = 0;
	reqDev.setup.wLength = sizeof(USBDeviceDescriptor);

	if (usbControlTransfer(pipe, tiGetDeviceDescriptor, &reqDev) != 0)
	{
		kprintf_debug("[USB] failed to perform GET_DESCRIPTOR command (for device descriptor)\n");
		usbDeleteControlPipe(pipe);
		usbDown(dev);
		return;
	};
	
	kprintf("[USB] device connected:\n");
	usbPrintDeviceDescriptor(&reqDev.desc);
	
	// copy the device descriptor into the device struct
	memcpy(&dev->descDev, &reqDev.desc, sizeof(USBDeviceDescriptor));
	
	// make sure there is at least one configuration descriptor (sanity check), then get it
	if (reqDev.desc.bNumConfigurations < 1)
	{
		kprintf_debug("[USB] This device has no configurations!\n");
		usbDeleteControlPipe(pipe);
		usbDown(dev);
		return;
	};
	
	struct
	{
		USBSetupPacket setup;
		USBConfigurationDescriptor desc;
	} reqConf;
	
	memset(&reqConf, 0, sizeof(reqConf));
	reqConf.setup.bmRequestType = USB_REQ_DEVICE_TO_HOST | USB_REQ_STANDARD | USB_REQ_DEVICE;
	reqConf.setup.bRequest = USB_REQ_GET_DESCRIPTOR;
	reqConf.setup.wValue = 0x0200;				// first configuration descriptor
	reqConf.setup.wIndex = 0;
	reqConf.setup.wLength = sizeof(USBConfigurationDescriptor);
	
	if (usbControlTransfer(pipe, tiGetConfigurationDescriptor, &reqConf) != 0)
	{
		kprintf_debug("[USB] Failed to get the default configuration descriptor\n");
		usbDeleteControlPipe(pipe);
		usbDown(dev);
		return;
	};
	
	kprintf("[USB] Default configuration descriptor:\n");
	usbPrintConfigurationDescriptor(&reqConf.desc);
	
	// set the configuration
	USBSetupPacket setconf;
	memset(&setconf, 0, sizeof(USBSetupPacket));
	setconf.bmRequestType = USB_REQ_HOST_TO_DEVICE | USB_REQ_STANDARD | USB_REQ_DEVICE;
	setconf.bRequest = USB_REQ_SET_CONFIGURATION;
	setconf.wValue = reqConf.desc.bConfigurationValue;
	
	if (usbControlTransfer(pipe, tiSetConfig, &setconf) != 0)
	{
		kprintf_debug("[USB] Failed to set configuration to default\n");
		usbDeleteControlPipe(pipe);
		usbDown(dev);
		return;
	};
	
	kprintf("[USB] Device configured.\n");
	
	// scan through the interface descriptors and create interface objects
	uint8_t *confbuf = (uint8_t*) kmalloc(reqConf.desc.wTotalLength);
	if (usbControlIn(pipe, USB_REQ_DEVICE_TO_HOST | USB_REQ_STANDARD | USB_REQ_DEVICE, USB_REQ_GET_DESCRIPTOR,
				0x0200, 0, reqConf.desc.wTotalLength, confbuf) != 0)
	{
		kprintf_debug("[USB] Failed to read full configuration\n");
		kfree(confbuf);
		usbDeleteControlPipe(pipe);
		usbDown(dev);
	};
	
	kprintf("[USB] The following configuration is in use:\n");
	
	uint8_t *scan = confbuf + reqConf.desc.bLength;
	size_t numInterfaces = reqConf.desc.bNumInterfaces;
	
	while (numInterfaces--)
	{
		// TODO: spawn threads for the interfaces, and create their descriptions.
		
		USBInterfaceDescriptor *descIntf = (USBInterfaceDescriptor*) scan;
		kprintf("INTF (len=0x%02hhX, type=0x%02hhX, intno=0x%02hhX, alt=0x%02hhX, epcount=%hhu, class=0x%02hhX:0x%02hhX:0x%02hhX)\n",
			descIntf->bLength,
			descIntf->bDescriptorType,
			descIntf->bInterfaceNumber,
			descIntf->bAlternateSetting,
			descIntf->bNumEndpoints,
			descIntf->bInterfaceClass,
			descIntf->bInterfaceSubClass,
			descIntf->bInterfaceProtocol
		);
		
		scan += descIntf->bLength;
		
		size_t numEndpoints = descIntf->bNumEndpoints;
		while (numEndpoints--)
		{
			USBEndpointDescriptor *descEp = (USBEndpointDescriptor*) scan;
			kprintf("  ENDPOINT (addr=0x%02hhX, attr=0x%02hhX, maxpacket=%hu, interval=%hhu)\n",
				descEp->bEndpointAddress,
				descEp->bmAttributes,
				descEp->wMaxPacketSize,
				descEp->bInterval
			);
			
			scan += descEp->bLength;
		};
		
		kprintf("\n");
	};
	
	// done; references were taken as necessary
	kfree(confbuf);
	usbDown(dev);
};

void usbPrintDeviceDescriptor(USBDeviceDescriptor *desc)
{
	kprintf("  USB Version:               %hu.%hu\n", desc->bcdUSB >> 8, desc->bcdUSB & 0xFF);
	kprintf("  USB Device Class:          0x%02hhX\n", desc->bDeviceClass);
	kprintf("  USB Device Subclass:       0x%02hhX\n", desc->bDeviceSubClass);
	kprintf("  USB Device Protocol:       0x%02hhX\n", desc->bDeviceProtocol);
	kprintf("  Max. Packet Size:          %hhu bytes\n", desc->bMaxPacketSize);
	kprintf("  USB Vendor ID:             0x%04hX\n", desc->idVendor);
	kprintf("  USB Product ID:            0x%04hX\n", desc->idProduct);
	kprintf("  USB Device Version:        %hu.%hu\n", desc->bcdDevice >> 8, desc->bcdDevice & 0xFF);
	kprintf("  Manufacturer String ID:    %hhu\n", desc->iManufacturer);
	kprintf("  Product String ID:         %hhu\n", desc->iProduct);
	kprintf("  Serial Number String ID:   %hhu\n", desc->iSerialNumber);
	kprintf("  Number of Configurations:  %hhu\n", desc->bNumConfigurations);
};

void usbPrintConfigurationDescriptor(USBConfigurationDescriptor *desc)
{
	kprintf("  Total length:              %hu bytes\n", desc->wTotalLength);
	kprintf("  Number of Interfaces:      %hhu\n", desc->bNumInterfaces);
	kprintf("  Configuration Value:       0x%02hhX\n", desc->bConfigurationValue);
	kprintf("  Configuration String ID:   %hhu\n", desc->iConfiguration);
	kprintf("  Attributes:                0x%02hhX\n", desc->bmAttributes);
	kprintf("  Max. Power:                %hu mA\n", 2 * (uint16_t) desc->bMaxPower);
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
	memset(dev, 0, sizeof(USBDevice));
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
	pipe->maxPacketLen = maxPacketLen;
	
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

int usbControlTransfer(USBControlPipe *pipe, USBTransferInfo *packets, void *buffer)
{
	int status;
	
	Semaphore sem;
	semInit2(&sem, 0);
	USBRequest *urb = usbCreateControlRequest(pipe, packets, buffer, &sem);
	if (urb == NULL)
	{
		return -1;
	};
	
	if ((status = usbSubmitRequest(urb)) != 0)
	{
		usbDeleteRequest(urb);
		return status;
	};
	
	semWait(&sem);
	status = usbGetRequestStatus(urb);
	usbDeleteRequest(urb);
	return status;
};

int usbControlIn(USBControlPipe *pipe, uint8_t bmRequestType, uint8_t bRequest,
			uint16_t wValue, uint16_t wIndex, uint16_t wLength,
			void *buffer)
{
	uint8_t *transbuf = (uint8_t*) kmalloc(sizeof(USBSetupPacket) + (size_t) wLength);
	USBSetupPacket *setup = (USBSetupPacket*) transbuf;
	
	setup->bmRequestType = bmRequestType;
	setup->bRequest = bRequest;
	setup->wValue = wValue;
	setup->wIndex = wIndex;
	setup->wLength = wLength;
	
	USBTransferInfo *ti = (USBTransferInfo*) kmalloc(sizeof(USBTransferInfo)
					* (((size_t)wLength + pipe->maxPacketLen-1) / pipe->maxPacketLen + 3));
	
	USBTransferInfo *put = ti;
	put->type = USB_SETUP;
	put->dt = 0;
	put->size = sizeof(USBSetupPacket);
	put++;
	
	usb_dt_t nextDT = 1;
	size_t sizeToGo = (size_t) wLength;
	while (sizeToGo > 0)
	{
		size_t sizeNow = sizeToGo;
		if (sizeNow > pipe->maxPacketLen) sizeNow = pipe->maxPacketLen;
		
		put->type = USB_IN;
		put->dt = nextDT;
		nextDT ^= 1;
		put->size = sizeNow;
		
		sizeToGo -= sizeNow;
		put++;
	};
	
	put->type = USB_OUT;
	put->dt = nextDT;
	put->size = 0;
	
	put++;
	put->type = USB_END;
	
	int status = usbControlTransfer(pipe, ti, transbuf);
	memcpy(buffer, transbuf + sizeof(USBSetupPacket), (size_t) wLength);
	kfree(ti);
	kfree(transbuf);
	return status;
};

void usbPostComplete(USBRequest *urb)
{
	if (urb->header.semComplete != NULL) semSignal(urb->header.semComplete);
};

int usbGetRequestStatus(USBRequest *urb)
{
	return urb->header.status;
};
