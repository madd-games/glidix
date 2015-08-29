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

#include <glidix/module.h>
#include <glidix/console.h>
#include <glidix/pci.h>
#include <glidix/string.h>
#include <glidix/netif.h>
#include <glidix/memory.h>

typedef struct
{
	uint16_t			vendor;
	uint16_t			device;
	const char*			name;
} NeDevice;

static NeDevice knownDevices[] = {
	{0x10EC, 0x8029, "Realtek RTL8191SE Wireless LAN 802.11n PCI-E NIC"},
	{0, 0, NULL}
};

typedef struct NeInterface_
{
	struct NeInterface_*		next;
	PCIDevice*			pcidev;
	NetIf*				netif;
} NeInterface;

static NeInterface *interfaces;
static NeInterface *lastIf;

static void ne2k_send(NetIf *netif, const void *addr, size_t addrlen, const void *packet, size_t packetlen)
{
	(void)netif;
	(void)addr;
	(void)addrlen;
	(void)packet;
	(void)packetlen;
};

static int ne2k_enumerator(PCIDevice *dev, void *ignore)
{
	(void)ignore;
	
	NeDevice *nedev;
	for (nedev=knownDevices; nedev->name!=NULL; nedev++)
	{
		if ((nedev->vendor == dev->vendor) && (nedev->device == dev->device))
		{
			strcpy(dev->deviceName, nedev->name);
			NeInterface *intf = (NeInterface*) kmalloc(sizeof(NeInterface));
			intf->next = NULL;
			intf->pcidev = dev;
			intf->netif = NULL;
			if (lastIf == NULL)
			{
				interfaces = intf;
				lastIf = intf;
			}
			else
			{
				lastIf->next = intf;
				lastIf = intf;
			};
			return 1;
		};
	};
	
	return 0;
};

MODULE_INIT(const char *opt)
{
	kprintf("ne2k: enumerating ne2000-compatible PCI devices\n");
	pciEnumDevices(THIS_MODULE, ne2k_enumerator, NULL);
	
	kprintf("ne2k: creating network interfaces\n");
	NeInterface *nif;
	for (nif=interfaces; nif!=NULL; nif=nif->next)
	{
		NetIfConfig ifconfig;
		ifconfig.ethernet.type = IF_ETHERNET;
		memset(ifconfig.ethernet.mac, 0xF0, 6);			// TODO!
		
		nif->netif = CreateNetworkInterface(nif, &ifconfig, ne2k_send);
		if (nif->netif == NULL)
		{
			kprintf("ne2k: CreateNetworkInterface() failed\n");
		}
		else
		{
			kprintf("ne2k: created interface: %s\n", nif->netif->name);
		};
	};
	
	return 0;
};

MODULE_FINI()
{
	kprintf("ne2k: releasing devices\n");
	NeInterface *nif = interfaces;
	NeInterface *next;
	while (nif != NULL)
	{
		pciReleaseDevice(nif->pcidev);
		next = nif->next;
		kfree(nif);
		nif = next;
	};
	
	return 0;
};
