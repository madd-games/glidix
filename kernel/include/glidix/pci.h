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

#ifndef __glidix_pci_h
#define __glidix_pci_h

#include <glidix/common.h>
#include <glidix/ioctl.h>
#include <glidix/module.h>

#define	PCI_CONFIG_ADDR					0xCF8
#define	PCI_CONFIG_DATA					0xCFC

#define	IOCTL_PCI_DEVINFO				IOCTL_ARG(PCIDevinfoRequest, IOCTL_INT_PCI, 0x01)

typedef union
{
	struct
	{
		uint16_t				vendor;
		uint16_t				device;
		uint16_t				command;
		uint16_t				status;
		uint8_t					rev;
		uint8_t					progif;
		uint8_t					subclass;
		uint8_t					classcode;
		uint8_t					cacheLineSize;
		uint8_t					latencyTimer;
		uint8_t					headerType;
		uint8_t					bist;
		uint32_t				bar[6];
		uint32_t				cardbusCIS;
		uint16_t				subsysVendor;
		uint16_t				subsysID;
		uint32_t				expromBase;
		uint8_t					cap;
		uint8_t					resv[7];
		uint8_t					intline;
		uint8_t					intpin;
		uint8_t					mingrant;
		uint8_t					maxlat;
	} PACKED std;
	
	struct
	{
		uint16_t				vendor;
		uint16_t				device;
		uint16_t				command;
		uint16_t				status;
		uint8_t					rev;
		uint8_t					progif;
		uint8_t					subclass;
		uint8_t					classcode;
		uint8_t					cacheLineSize;
		uint8_t					latencyTimer;
		uint8_t					headerType;
		uint8_t					bist;
		uint32_t				bar[2];
		uint8_t					primaryBus;
		uint8_t					secondaryBus;
		uint8_t					subordinateBus;
		uint8_t					secondaryLatencyTimer;
		uint8_t					iobase;
		uint8_t					iolimit;
		uint16_t				secondaryStatus;
		uint16_t				membase;
		uint16_t				memlimit;
		uint16_t				premembase;
		uint16_t				prememlimit;
		uint32_t				premembaseupper;
		uint32_t				prememlimitupper;
		uint16_t				iobaseupper;
		uint16_t				iolimitupper;
		uint8_t					capability;
		uint8_t					reserved[7];
		uint32_t				expbase;
		uint8_t					intline;
		uint8_t					intpin;
		uint16_t				bridgectl;
	} PACKED bridge;
} PCIDeviceConfig;

typedef struct
{
	uint8_t bus;
	uint8_t slot;
	uint8_t func;
	PCIDeviceConfig config;
} PACKED PCIDevinfoRequest;

typedef struct PCIDevice_
{
	struct PCIDevice_*				next;
	uint8_t						bus;
	uint8_t						slot;
	uint8_t						func;
	int						id;			// glidix-defined device ID
	uint16_t					vendor;
	uint16_t					device;
	uint16_t					type;			// high 8 bits = classcode, low 8 bits = subclass
	uint8_t						intpin;
	Module*						driver;
	char						driverName[128];	// name of driver module
	char						deviceName[128];	// name of device (default = "Unknown")
	uint32_t					bar[6];			// base address registers (BARs)
	uint32_t					barsz[6];		// size of each BAR
	uint8_t						progif;

	// -- END OF USER AREA --
	int						intNo;			// which interrupt the device was mapped to
	
	/**
	 * IRQ handler. Return 0 if the interrupt actually came from this device,
	 * -1 otherwise.
	 */
	int (*irqHandler)(void *param);
	void *irqParam;
	
	// slot and interrupt pin on the root bridge
	int						rootSlot;
	int						rootInt;
} PCIDevice;

typedef struct
{
	uint8_t					bus;
	uint8_t					dev;
	uint8_t					func;
	
	/**
	 * Low 2 bits = pin number (0-3 = A-D), top bit (mask 0x80) decides whether intNo
	 * is a Global System Interupt (0) or an IRQ number (1).
	 */
	uint8_t					pinAndType;
	
	/**
	 * GSI number or IRQ number.
	 */
	int					intNo;
} PCIRouteEntry;

void pciInit();
void pciGetDeviceConfig(uint8_t bus, uint8_t slot, uint8_t func, PCIDeviceConfig *config);

/**
 * System call to get information about a PCI device with the given ID. The PCIDevice structure is copied into the
 * buffer. If the buffer is larger than expected, the real size is returned; if it is too small, the structure is
 * truncated. The pointers in the PCIDevice structure are meaningless in userspace. Returns -1 on error, and sets
 * errno appropriately:
 *  - ENOENT	The device with the given ID does not exist.
 */
ssize_t sys_pcistat(int id, PCIDevice *buffer, size_t bufsize);

/**
 * This function is called by device drivers to decide which devices they can support. This function will scan the
 * list of PCI devices in the system, calling enumerator() on each one (passing @param to that function). The job
 * of the enumerator() is as follows: if it doesn't support the device, return 0 without doing anything at all to
 * the device description. If it does support the device, it should change the @deviceName field - and ONLY that
 * field - to indicate what the device is called, and return 1; the kernel will then assign this device to the module.
 * The @module argument should be THIS_MODULE. The device list is locked when the enumerator() is called, so it must not
 * do anything other than recognise the device, put it in an internal list of devices, and return. Remember that when
 * the module is being unloaded, it must call pciReleaseDevice()!
 */
void pciEnumDevices(Module *module, int (*enumerator)(PCIDevice *dev, void *param), void *param);

/**
 * Release the device. Called by a driver after it has acquired a device and no longer wants to support it (e.g. the module
 * is being unloaded).
 */
void pciReleaseDevice(PCIDevice *dev);

/**
 * Initialize PCI ACPI information.
 */
void pciInitACPI();

/**
 * Called to report a PCI interrupt.
 */
void pciInterrupt(int intNo);

/**
 * Enable or disable bus mastering for the given device.
 */
void pciSetBusMastering(PCIDevice *dev, int enable);

/**
 * Set the IRQ handler for a device. Setting to NULL removes the handler; however, this is done automatically
 * when calling pciReleaseDevice().
 */
void pciSetIrqHandler(PCIDevice *dev, int (*handler)(void *param), void *param);

#endif
