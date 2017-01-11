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

#ifndef __glidix_usb_h
#define __glidix_usb_h

#include <glidix/common.h>
#include <glidix/semaphore.h>

/**
 * USB status bits.
 */
#define	USB_DISCONNECTED			(1 << 0)

/**
 * USB standard requests (bRequest)
 */
#define	USB_GET_STATUS				0
#define	USB_CLEAR_FEATURE			1
/* Reserved					2 */
#define	USB_SET_FEATURE				3
/* Reserved					4 */
#define	USB_SET_ADDRESS				5
#define	USB_GET_DESCRIPTOR			6
#define	USB_SET_DESCRIPTOR			7
#define	USB_GET_CONFIGURATION			8
#define	USB_SET_CONFIGURATION			9
#define	USB_GET_INTERFACE			10
#define	USB_SET_INTERFACE			11
#define	USB_SYNC_FRAME				12

/**
 * Represents a USB device.
 */
typedef struct USBDevice_
{
	/**
	 * Links the device to the list.
	 */
	struct USBDevice_*			prev;
	struct USBDevice_*			next;
	
	/**
	 * The size of this structure.
	 */
	size_t					size;
	
	/**
	 * Controller-driver-specific data structure for this USB device.
	 */
	void*					data;
	
	/**
	 * Reference count.
	 */
	int					refcount;
	
	/**
	 * Signalled every time the status bits change.
	 */
	Semaphore				semStatus;
	
	/**
	 * Current status of the device.
	 */
	uint64_t				status;
	
	/**
	 * Address of the device ("function address").
	 */
	uint8_t					faddr;
} USBDevice;

/**
 * USB request format.
 */
typedef struct
{
	uint8_t					bmRequestType;
	uint8_t					bRequest;
	uint16_t				wValue;
	uint16_t				wIndex;
	uint16_t				wLength;
} USBRequest;

/**
 * Initialize the USB subsystem.
 */
void usbInit();

/**
 * Allocates a USB device structure and returns a pointer to it. You may then fill out the structure
 * and call usbPostDevice() to present the device to the system. The structure returned by this function
 * contains the default values of all fields, with 'size' set to the size of the USBDevice structure
 * supported by the kernel; if this is smaller than what the driver expects, it means that the driver is
 * running on a kernel version older than what it was built for.
 *
 * The returned structure has a reference count of 0. You must not call any other USB functions on it prior
 * to calling usbPostDevice(), which sets the reference count to 1.
 *
 * The 'faddr' field is set to a unique function address allocated by the kernel. This address will be deallocated
 * when usbDisconnect() is called.
 *
 * Returns NULL on error. The possible reasons are:
 *  - we are out of function addresses.
 */
USBDevice* usbCreateDevice();

/**
 * Adds a USB device to the system device list. This must be the first function called on a device returned by
 * usbCreateDevice(), right after the controller driver fills out the appropriate fields. This function sets the
 * reference count to 1 and allows the device to be enumerated by USB device drivers.
 *
 * Note that this call will result in a bunch of control messages being sent to the device to determine its type
 * etc.
 */
void usbPostDevice(USBDevice *usbdev);

/**
 * Called by the controller driver when a USB device is disconnected. This decreases the reference count, and the
 * controller driver must not reference the USB device anymore.
 */
void usbDisconnect(USBDevice *usbdev);

#endif
