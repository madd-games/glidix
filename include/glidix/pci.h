/*
	Glidix kernel

	Copyright (c) 2014-2015, Madd Games.
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
	// TODO
} PCIDeviceConfig;

typedef struct
{
	uint8_t bus;
	uint8_t slot;
	uint8_t func;
	PCIDeviceConfig config;
} PACKED PCIDevinfoRequest;

void pciInit();
void pciGetDeviceConfig(uint8_t bus, uint8_t slot, uint8_t func, PCIDeviceConfig *config);

#endif
