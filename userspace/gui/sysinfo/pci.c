/*
	Glidix GUI

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

#include <stdio.h>
#include <libgwm.h>
#include <time.h>
#include <sys/time.h>
#include <stdlib.h>
#include <string.h>
#include <signal.h>
#include <unistd.h>
#include <dirent.h>
#include <pthread.h>
#include <poll.h>
#include <sys/glidix.h>

const char *pciCols[] = {
	"Name", "Address", "Device ID", "Type", "Driver"
};

char* pciGetColCaption(int index)
{
	return strdup(pciCols[index]);
};

void *pciOpenNode(const void *path)
{
	int *node = (int*) malloc(sizeof(int));
	*node = 0;
	return node;
};

int pciGetNext(void *context, GWMNodeInfo *info, size_t infoSize)
{
	int *node = (int*) context;
	int index = (*node)++;
	
	_glidix_pcidev dev;
	if (_glidix_pcistat(index, &dev, sizeof(_glidix_pcidev)) == -1)
	{
		return -1;
	};
	
	memcpy(info->niPath, &index, sizeof(int));
	info->niFlags = 0;
	info->niIcon = NULL;
	
	info->niValues[0] = strdup(dev.deviceName);
	info->niValues[1] = malloc(256);
	sprintf((char*)info->niValues[1], "%hhu/%hhu/%hhu", dev.bus, dev.slot, dev.func);
	info->niValues[2] = malloc(16);
	sprintf((char*)info->niValues[2], "%04x:%04x", dev.vendor, dev.device);
	info->niValues[3] = malloc(5);
	sprintf((char*)info->niValues[3], "%04x", dev.type);
	info->niValues[4] = strdup(dev.driverName);
	
	return 0;
};

void pciCloseNode(void *context)
{
	free(context);
};

GWMTreeEnum enumPci = {
	.teSize =				sizeof(GWMTreeEnum),
	.tePathSize =				sizeof(int),
	.teNumCols =				sizeof(pciCols) / sizeof(const char*),
	.teGetColCaption =			pciGetColCaption,
	.teOpenNode =				pciOpenNode,
	.teGetNext =				pciGetNext,
	.teCloseNode =				pciCloseNode
};

void initPCITab(GWMWindow *notebook)
{
	int width, height;
	gwmNotebookGetDisplaySize(notebook, &width, &height);
	
	GWMWindow *tab = gwmNotebookAdd(notebook, "PCI Devices");
	int root = -1;
	GWMWindow *treeview = gwmCreateTreeView(tab, 0, 0, width, height, &enumPci, &root, 0);
};
