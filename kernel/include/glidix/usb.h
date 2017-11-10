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
 * bmRequestType
 */
#define	USB_REQ_HOST_TO_DEVICE				0
#define	USB_REQ_DEVICE_TO_HOST				(1 << 7)

#define	USB_REQ_STANDARD				(0 << 5)
#define	USB_REQ_CLASS					(1 << 5)
#define	USB_REQ_VENDOR					(2 << 5)

#define	USB_REQ_DEVICE					0
#define	USB_REQ_INTERFACE				1
#define	USB_REQ_ENDPOINT				2
#define	USB_REQ_OTHER					3

/**
 * bRequest
 */
#define	USB_REQ_GET_STATUS				0x00
#define	USB_REQ_CLEAR_FEATURE				0x01
#define	USB_REQ_SET_FEATURE				0x03
#define	USB_REQ_SET_ADDRESS				0x05
#define	USB_REQ_GET_DESCRIPTOR				0x06
#define	USB_REQ_SET_DESCRIPTOR				0x07
#define	USB_REQ_GET_CONFIGURATION			0x08
#define	USB_REQ_SET_CONFIGURATION			0x09

/**
 * USB packet types.
 */
#define	USB_END						0		/* list terminator */
#define	USB_SETUP					1
#define	USB_IN						2
#define	USB_OUT						3

/**
 * Types of USB transactions.
 */
#define	USB_TRANS_CONTROL				1

/**
 * USB device flags.
 */
#define	USB_DEV_HANGUP					(1 << 0)	/* device has been disconnected */

/**
 * USB-related data types.
 */
typedef	uint8_t						usb_addr_t;
typedef uint8_t						usb_epno_t;
typedef int						usb_dt_t;

/**
 * USB setup packet.
 */
typedef struct
{
	uint8_t						bmRequestType;
	uint8_t						bRequest;
	uint16_t					wValue;
	uint16_t					wIndex;
	uint16_t					wLength;
} USBSetupPacket;

/**
 * Device descriptor.
 */
typedef struct
{
	uint8_t						bLength;
	uint8_t						bDescriptorType;
	uint16_t					bcdUSB;
	uint8_t						bDeviceClass;
	uint8_t						bDeviceSubClass;
	uint8_t						bDeviceProtocol;
	uint8_t						bMaxPacketSize;
	uint16_t					idVendor;
	uint16_t					idProduct;
	uint16_t					bcdDevice;
	uint8_t						iManufacturer;
	uint8_t						iProduct;
	uint8_t						iSerialNumber;
	uint8_t						bNumConfigurations;
} USBDeviceDescriptor;

/**
 * Configuration descriptor.
 */
typedef struct
{
	uint8_t						bLength;
	uint8_t						bDescriptorType;
	uint16_t					wTotalLength;
	uint8_t						bNumInterfaces;
	uint8_t						bConfigurationValue;
	uint8_t						iConfiguration;
	uint8_t						bmAttributes;
	uint8_t						bMaxPower;
} USBConfigurationDescriptor;

/**
 * Interface descriptor.
 */
typedef struct
{
	uint8_t						bLength;
	uint8_t						bDescriptorType;
	uint8_t						bInterfaceNumber;
	uint8_t						bAlternateSetting;
	uint8_t						bNumEndpoints;
	uint8_t						bInterfaceClass;
	uint8_t						bInterfaceSubClass;
	uint8_t						bInterfaceProtocol;
	uint8_t						iInterface;
} USBInterfaceDescriptor;

/**
 * Endpoint descriptor.
 */
typedef struct
{
	uint8_t						bLength;
	uint8_t						bDescriptorType;
	uint8_t						bEndpointAddress;
	uint8_t						bmAttributes;
	uint16_t					wMaxPacketSize;
	uint8_t						bInterval;
} USBEndpointDescriptor;

/**
 * Describes an element of a transaction. Used for control and bulk transactions.
 */
typedef struct
{
	/**
	 * Type of transaction (USB_SETUP, USB_IN, USB_OUT).
	 */
	int						type;
	
	/**
	 * Value of the data toggle bit (0 or 1).
	 */
	usb_dt_t					dt;
	
	/**
	 * Size of the data to transfer, in the buffer. The sum of
	 * previous transfers' sizes is the offset.
	 */
	size_t						size;
} USBTransferInfo;

/**
 * Request header.
 */
struct USBDevice_;
typedef struct
{
	/**
	 * Type of request (USB_TRANS_*).
	 */
	int						type;
	
	/**
	 * A semaphore to be signalled when this request is completed, if
	 * not NULL.
	 */
	Semaphore*					semComplete;
	
	/**
	 * The device on which this request is being performed.
	 */
	struct USBDevice_*				dev;
	
	/**
	 * Size of the URB in bytes.
	 */
	size_t						size;
	
	/**
	 * Status (0 = good, anything else = error). Set after the request is completed.
	 */
	int						status;
} USBRequestHeader;

/**
 * Describes a USB request.
 */
struct USBControlPipe_;
typedef union
{
	/**
	 * Request header, common for all types.
	 */
	USBRequestHeader				header;
	
	/**
	 * For control transfers (USB_TRANS_CONTROL).
	 */
	struct
	{
		USBRequestHeader			header;
		
		/**
		 * The pipe to transfer to/from.
		 */
		struct USBControlPipe_*			pipe;
		
		/**
		 * List of packets.
		 */
		USBTransferInfo*			packets;
		
		/**
		 * The buffer containing transfer data. For USB_SETUP and USB_OUT, it must
		 * contain the data to be sent, and for USB_IN, it must have space to store
		 * the inputted values. The buffer is not updated except for fields filled
		 * in by USB_IN transfers, after completion of the request.
		 *
		 * There MUST be write permission for the entire buffer, regardless of transfer
		 * types, as the host controller driver may wish to transfer the contents to a
		 * temporary location, and then copy back.
		 */
		void*					buffer;
	} control;
} USBRequest;

/**
 * Describes the operations of a USB controller. This is passed to usbCreateDevice()
 * to tell the kernel how to perform various operations on the device.
 *
 * All pointers are allowed to be NULL, indicating a no-op.
 */
typedef struct
{
	/**
	 * Create a new control pipe. Return a controller-driver-specific opaque structure describing
	 * the new pipe, or NULL if a fatal error occured. The endpoint number for the pipe is
	 * 'epno'.
	 */
	void* (*createControlPipe)(struct USBDevice_ *dev, usb_epno_t epno, size_t maxPacketLen);
	
	/**
	 * Delete a control pipe. 'data' is a value previously returned by createControlPipe().
	 */
	void (*deleteControlPipe)(void *data);
	
	/**
	 * Submit a USB transfer. Return 0 on success, errno on error.
	 */
	int (*submit)(USBRequest* urb);
} USBCtrl;

/**
 * Describes a USB device.
 */
typedef struct USBDevice_
{
	/**
	 * Address of the device (1-127).
	 */
	usb_addr_t					addr;
	
	/**
	 * Controller description.
	 */
	USBCtrl*					ctrl;
	
	/**
	 * Device flags.
	 */
	int						flags;
	
	/**
	 * Controller-specific data.
	 */
	void*						data;
	
	/**
	 * Version of USB that this device conforms to. This actually indicates which controller
	 * driver the device is using (e.g. 2 means EHCI). For the actual USB version implemented
	 * by the device, see the device descriptor.
	 */
	int						usbver;
	
	/**
	 * Reference count.
	 */
	int						refcount;
	
	/**
	 * Device lock.
	 */
	Semaphore					lock;
	
	/**
	 * The device descriptor.
	 */
	USBDeviceDescriptor				descDev;
	
	/**
	 * Default control pipe.
	 */
	struct USBControlPipe_*				ctrlPipe;
} USBDevice;

/**
 * Describes a USB control pipe.
 */
typedef struct USBControlPipe_
{
	/**
	 * The device on which this pipe's endpoints are located.
	 */
	USBDevice*					dev;
	
	/**
	 * Controller driver data.
	 */
	void*						data;
	
	/**
	 * Maximum packet size.
	 */
	size_t						maxPacketLen;
} USBControlPipe;

/**
 * Initialize the USB subsystem.
 */
void usbInit();

/**
 * Allocate a new device address and return it. Returns 0 if out of addresses.
 */
usb_addr_t usbAllocAddr();

/**
 * Release a device address.
 */
void usbReleaseAddr(usb_addr_t addr);

/**
 * Create a device object. This is called by the controller driver when a USB device is detected.
 */
USBDevice* usbCreateDevice(usb_addr_t addr, USBCtrl *ctrl, int flags, void *data, int usbver);

/**
 * Hang up a USB device. This is called by the controller driver when the device is unplugged from
 * a port. It also decrements the reference count of the device, so the reference becomes invalid.
 *
 * Prior to calling this function, the controller driver should make a copy of the "data" field (if
 * used) and after calling this function, release any other resources associated with the device.
 * This function sets the "ctrl" field to a no-op controller.
 */
void usbHangup(USBDevice *dev);

/**
 * Create a USB control pipe object for the specified device, with the endpoint number 'epno',
 * and maximum packet length 'maxPacketLen'. Returns the object on success, or NULL on failure.
 */
USBControlPipe* usbCreateControlPipe(USBDevice *dev, usb_epno_t epno, size_t maxPacketLen);

/**
 * Delete a USB control pipe.
 */
void usbDeleteControlPipe(USBControlPipe *dev);

/**
 * Create a control request made up of the specified packets and buffer, on the given control pipe.
 * Returns NULL on a fatal error.
 */
USBRequest* usbCreateControlRequest(USBControlPipe *pipe, USBTransferInfo *packets, void *buffer, Semaphore *semComplete);

/**
 * Submit a USB request. This function returns immediately when the request gets queued; in order to wait
 * for completion, you must wait on the completion semaphore. Returns 0 on success, or errno on error.
 */
int usbSubmitRequest(USBRequest *urb);

/**
 * Delete a USB request object.
 */
void usbDeleteRequest(USBRequest *urb);

/**
 * A shorthand for performing control transfers. Do a control transfer on the given pipe, with the given packets
 * and with the given buffer. Wait for completion, and return the status. The status is zero on success, and
 * nonzero on error.
 */
int usbControlTransfer(USBControlPipe *pipe, USBTransferInfo *packets, void *buffer);

/**
 * Control input. Send the specified command, and input the specified number of bytes into the buffer. Returns the
 * status.
 */
int usbControlIn(USBControlPipe *pipe, uint8_t bmRequestType, uint8_t bRequest,
			uint16_t wValue, uint16_t wIndex, uint16_t wLength,
			void *buffer);

/**
 * Called by the controller driver. Mark a USB request as completed.
 */
void usbPostComplete(USBRequest *urb);

/**
 * Get the request status. Call only after the request has been completed!
 */
int usbGetRequestStatus(USBRequest *urb);

/**
 * Print a device descriptor, for debugging purposes.
 */
void usbPrintDeviceDescriptor(USBDeviceDescriptor *desc);

/**
 * Print a configuration descriptor, for debugging purposes.
 */
void usbPrintConfigurationDescriptor(USBConfigurationDescriptor *desc);

#endif
