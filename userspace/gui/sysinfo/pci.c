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
#include <errno.h>

typedef struct
{
	int			dev;
	int			index;
} BarNode;

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
	sprintf((char*)info->niValues[2], "%04hx:%04hx", dev.vendor, dev.device);
	info->niValues[3] = malloc(5);
	sprintf((char*)info->niValues[3], "%04hx", dev.type);
	info->niValues[4] = strdup(dev.driverName);
	
	return 0;
};

const char *barCols[] = {
	"BAR#", "Type", "Start", "Size"
};

char *barGetColCaption(int index)
{
	return strdup(barCols[index]);
};

void *barOpenNode(const void *path)
{
	BarNode *node = (BarNode*) malloc(sizeof(BarNode));
	node->dev = *((int*)path);
	node->index = 0;
	return node;
};

void formatSize(char *buffer, uint32_t size)
{
	static const char *units[] = {"B", "KB", "MB", "GB"};
	
	int unitIndex = 0;
	while ((size >= 1024) && (unitIndex < 3))
	{
		unitIndex++;
		size /= 1024;
	};
	
	sprintf(buffer, "%u %s", size, units[unitIndex]);
};

int barGetNext(void *context, GWMNodeInfo *info, size_t infoSize)
{
	BarNode *node = (BarNode*) context;
	if (node->index == 6) return -1;
	
	_glidix_pcidev dev;
	if (_glidix_pcistat(node->dev, &dev, sizeof(_glidix_pcidev)) == -1)
	{
		return -1;
	};
	
	memcpy(info->niPath, &node->index, sizeof(int));
	info->niFlags = 0;
	info->niIcon = NULL;
	
	info->niValues[0] = malloc(5);
	sprintf((char*)info->niValues[0], "BAR%d", node->index);
	
	if (dev.bar[node->index] & 1)
	{
		info->niValues[1] = strdup("Port");
		info->niValues[2] = malloc(16);
		sprintf((char*)info->niValues[2], "0x%04x", dev.bar[node->index] & ~0x3);
		info->niValues[3] = malloc(128);
		sprintf((char*)info->niValues[3], "%u (0x%04x) ports", dev.barsz[node->index], dev.barsz[node->index]);
	}
	else
	{
		info->niValues[1] = strdup("Memory");
		info->niValues[2] = malloc(16);
		sprintf((char*)info->niValues[2], "0x%08x", dev.bar[node->index] & ~0xF);
		info->niValues[3] = malloc(32);
		formatSize((char*)info->niValues[3], dev.barsz[node->index]);
	};
	
	node->index++;
	return 0;
};

void barCloseNode(void *node)
{
	free(node);
};

GWMTreeEnum enumBar = {
	.teSize =				sizeof(GWMTreeEnum),
	.tePathSize =				sizeof(int),
	.teNumCols =				sizeof(barCols) / sizeof(const char*),
	.teGetColCaption =			barGetColCaption,
	.teOpenNode =				barOpenNode,
	.teGetNext =				barGetNext,
	.teCloseNode =				barCloseNode
};

int pciInfoHandler(GWMEvent *ev, GWMWindow *win)
{
	switch (ev->type)
	{
	case GWM_EVENT_CLOSE:
		gwmDestroyTreeView((GWMWindow*) win->data);
		gwmDestroyWindow(win);
		return 0;
	default:
		return gwmDefaultHandler(ev, win);
	};
};

int pciActivate(void *context)
{
	GWMWindow *treeview = (GWMWindow*) context;
	int index;
	if (gwmTreeViewGetSelection(treeview, &index) == 0)
	{
		_glidix_pcidev dev;
		if (_glidix_pcistat(index, &dev, sizeof(_glidix_pcidev)) == -1)
		{
			char errmsg[256];
			sprintf(errmsg, "Failed to read PCI device information: %s", strerror(errno));
			gwmMessageBox(NULL, "Error", errmsg, GWM_MBICON_ERROR | GWM_MBUT_OK);
			return 0;
		};
		
		GWMWindow *win = gwmCreateWindow(NULL, "PCI Device Info", GWM_POS_UNSPEC, GWM_POS_UNSPEC, 500, 300,
							GWM_WINDOW_HIDDEN | GWM_WINDOW_NOTASKBAR);
		
		DDISurface *canvas = gwmGetWindowCanvas(win);
		DDIPen *pen = ddiCreatePen(&canvas->format, gwmGetDefaultFont(), 2, 2, 496, 296, 0, 0, NULL);
		char intpin[8];
		if (dev.intpin == 0)
		{
			strcpy(intpin, "None");
		}
		else
		{
			sprintf(intpin, "INT%c#", (char) dev.intpin + 'A');
		};
		
		char info[4096];
		sprintf(info,	"Address: %hhu/%hhu/%hhu\n"
				"Vendor ID: %04hx\n"
				"Device ID: %04hx\n"
				"Type: %04hx\n"
				"Interrupt: %s\n"
				"Driver: %s\n"
				"Device name: %s\n",
				dev.bus, dev.slot, dev.func,	// Address
				dev.vendor,			// Vendor ID
				dev.device,			// Device ID
				dev.type,			// Type
				intpin,				// Interrupt
				dev.driverName,			// Driver
				dev.deviceName			// Device
		);
		
		ddiWritePen(pen, info);
		
		int width, height;
		ddiGetPenSize(pen, &width, &height);
		
		ddiExecutePen(pen, canvas);
		ddiDeletePen(pen);
		
		GWMWindow *barTree = gwmCreateTreeView(win, 2, height+2, 496, 294-height, &enumBar, &index, 0);
		win->data = barTree;
		
		gwmPostDirty(win);
		gwmSetEventHandler(win, pciInfoHandler);
		gwmSetWindowFlags(win, 0);
	};
	
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
	gwmTreeViewSetActivateCallback(treeview, pciActivate, treeview);
};
