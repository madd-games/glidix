/*
	Glidix Shell Utilities

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

#include <stdio.h>
#include <signal.h>
#include <stdlib.h>
#include <unistd.h>
#include <sys/glidix.h>
#include <string.h>
#include <fcntl.h>
#include <sys/wait.h>
#include <sys/ioctl.h>
#include <sys/stat.h>
#include <dirent.h>
#include <time.h>
#include <sys/call.h>
#include <errno.h>

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

int main(int argc, char *argv[])
{
	uint64_t devs[256];
	if (__syscall(__SYS_usb_list, devs) != 0)
	{
		fprintf(stderr, "%s: usb_list: %s\n", argv[0], strerror(errno));
		return 1;
	};
	
	int i;
	for (i=0; i<256; i++)
	{
		if (devs[i] == 0) break;
		
		USBDeviceDescriptor desc;
		if (__syscall(__SYS_usb_devdesc, devs[i], &desc) != 0)
		{
			continue;
		};

		uint16_t langids[128];
		int langCount;
		
		if (__syscall(__SYS_usb_langids, devs[i], langids, &langCount) != 0)
		{
			continue;
		};
		
		int haveEnglish = 0;
		int j;
		for (j=0; j<langCount; j++)
		{
			if (langids[j] == 0x0409)
			{
				haveEnglish = 1;
				break;
			};
		};
		
		char devname[128];
		if (desc.iProduct == 0 || !haveEnglish)
		{
			strcpy(devname, "(no name detected)\n");
		}
		else
		{
			if (__syscall(__SYS_usb_getstr, devs[i], desc.iProduct, (uint16_t)0x0409, devname) != 0)
			{
				continue;
			};
		};
		
		printf("[%04hX:%04hX] (class %02hhX/%02hhX) %s\n", desc.idVendor, desc.idProduct, desc.bDeviceClass,
								desc.bDeviceSubClass, devname);
	};
	
	return 0;
};
