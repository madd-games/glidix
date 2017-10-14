/*
	Glidix GUI

	Copyright (c) 2014-2017, Madd Games.
	All rights reserved.
	
	Redistribution and use in source and binary forms, with or without
	modification, are permitted provided that the following conditions are met:
	
	* Redistributions of source code must retain the above copyright notice, this
	  list of conditions and the following disclaimer.
	
	* Redistributions in binary form must reproduce the above copyright notice,
	  this list of conditions and the following disclaimer in the documentationx
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

#include <sys/mman.h>
#include <fcntl.h>
#include <unistd.h>
#include <libgwm.h>
#include <stddef.h>

/**
 * Describes a property of the theme.
 */
typedef struct
{
	/**
	 * Name of the property, e.g. "gwm.server.wincap".
	 */
	const char *name;
	
	/**
	 * The data type (GWM_TYPE_*).
	 */
	int type;
	
	/**
	 * Offset into the info structure where this property is stored.
	 */
	size_t offset;
} ThemeProperty;

static ThemeProperty themeInfo[] = {
	{"gwm.server.wincap",			GWM_TYPE_SURFACE,	offsetof(GWMInfo, imgWinCap)},
	{"gwm.server.winbuttons",		GWM_TYPE_SURFACE,	offsetof(GWMInfo, imgWinButtons)},
	{"gwm.server.wincolor.inactive",	GWM_TYPE_COLOR,		offsetof(GWMInfo, colWinInactive)},
	{"gwm.server.wincolor.active",		GWM_TYPE_COLOR,		offsetof(GWMInfo, colWinActive)},
	{"gwm.tools.shutdown",			GWM_TYPE_SURFACE,	offsetof(GWMInfo, imgShutdown)},
	
	// LIST TERMINATOR
	{NULL, 0, 0}
};

static DDISurface *surfaceSetup(DDIPixelFormat *format, uint32_t *idOut, unsigned int width, unsigned int height)
{
	if ((*idOut) == 0)
	{
		DDISurface *surf = ddiCreateSurface(format, width, height, NULL, DDI_SHARED);
		if (surf == NULL)
		{
			fprintf(stderr, "gwmGlobalThemeInit: ddiCreateSurface failed\n");
			return NULL;
		};
	
		*idOut = surf->id;
		return surf;
	}
	else
	{
		DDISurface *surf = ddiOpenSurface(*idOut);
		if (surf == NULL)
		{
			fprintf(stderr, "gwmGlobalThemeInit: ddiOpenSurface(0x%08X) failed\n", *idOut);
			return NULL;
		};
		
		return surf;
	};
};

static void drawButton(DDISurface *surf, int x, int y, int width, int height, DDIColor *color, int pressed)
{
	static DDIColor black = {0x00, 0x00, 0x00, 0xFF};
	static DDIColor white = {0xFF, 0xFF, 0xFF, 0xFF};
	
	ddiFillRect(surf, x+1, y+1, width-2, height-2, color);
	if (pressed)
	{
		ddiFillRect(surf, x, y, width, 1, &black);
		ddiFillRect(surf, x, y, 1, height, &black);
		ddiFillRect(surf, x, y+height-1, width, 1, &white);
		ddiFillRect(surf, x+width-1, y, 1, height, &white);
	}
	else
	{
		ddiFillRect(surf, x, y, width, 1, &white);
		ddiFillRect(surf, x, y, 1, height, &white);
		ddiFillRect(surf, x, y+height-1, width, 1, &black);
		ddiFillRect(surf, x+width-1, y, 1, height, &black);
	};
};

int gwmGlobalThemeInit(DDIPixelFormat *format)
{
	int fd = open("/run/gwminfo", O_RDWR);
	if (fd == -1)
	{
		perror("gwmGlobalThemeInit: open /run/gwminfo");
		return -1;
	};
	
	void *addr = mmap(NULL, sizeof(GWMInfo), PROT_READ | PROT_WRITE, MAP_SHARED, fd, 0);
	if (addr == MAP_FAILED)
	{
		perror("gwmGlobalThemeInit: mmap");
		return -1;
	};
	
	close(fd);
	
	GWMInfo *info = (GWMInfo*) addr;
	DDISurface *surf;
	
	// window caption bar
	surf = surfaceSetup(format, &info->imgWinCap, 21, 40);
	if (surf == NULL) return -1;
	
	static DDIColor colActiveCap = {0x77, 0x77, 0xCC, 0xFF};
	static DDIColor colInactiveCap = {0x77, 0x77, 0x77, 0xFF};
	ddiFillRect(surf, 0, 0, 21, 20, &colActiveCap);
	ddiFillRect(surf, 0, 20, 21, 20, &colInactiveCap);
	
	// window buttons
	surf = surfaceSetup(format, &info->imgWinButtons, 70, 80);
	if (surf == NULL) return -1;
	
	static DDIColor buttonColors[3] = {
		{0x00, 0x00, 0xFF, 0xFF},
		{0x00, 0xFF, 0x00, 0xFF},
		{0xFF, 0x00, 0x00, 0xFF}
	};
	
	int i;
	for (i=0; i<3; i++)
	{
		int width = 20;
		if (i == 2) width = 30;
		
		static DDIColor disabledColor = {0x33, 0x33, 0x33, 0xFF};
		drawButton(surf, 20*i, 0, width, 20, &buttonColors[i], 0);
		drawButton(surf, 20*i, 20, width, 20, &buttonColors[i], 0);
		drawButton(surf, 20*i, 40, width, 20, &buttonColors[i], 1);
		drawButton(surf, 20*i, 60, width, 20, &disabledColor, 0);
	};
	
	// window decoration colors
	static DDIColor colWinActive = {0x77, 0x77, 0xCC, 0xFF};
	static DDIColor colWinInactive = {0x77, 0x77, 0x77, 0xFF};
	memcpy(&info->colWinActive, &colWinActive, sizeof(DDIColor));
	memcpy(&info->colWinInactive, &colWinInactive, sizeof(DDIColor));
	
	// the shutdown image.
	static DDIColor red = {0xFF, 0x00, 0x00, 0xFF};
	static DDIColor blue = {0x00, 0x00, 0xFF, 0xFF};
	static DDIColor bgr = {0xDD, 0xDD, 0xDD, 0xFF};
	surf = surfaceSetup(format, &info->imgShutdown, 320, 200);
	if (surf == NULL) return -1;
	ddiFillRect(surf, 0, 0, 320, 200, &bgr);
	ddiFillRect(surf, 74, 68, 64, 64, &blue);
	ddiFillRect(surf, 180, 68, 64, 64, &red);
	
	munmap(addr, sizeof(GWMInfo));
	return 0;
};

void* gwmGetThemeProp(const char *name, int type, int *errOut)
{
	ThemeProperty *prop;
	for (prop=themeInfo; prop->name!=NULL; prop++)
	{
		if (strcmp(prop->name, name) == 0)
		{
			if (prop->type != type)
			{
				if (errOut != NULL) *errOut = GWM_ERR_INVAL;
				return NULL;
			};
			
			DDISurface *surf;
			char *ptr = (char*) gwmGetInfo() + prop->offset;
			switch (type)
			{
			case GWM_TYPE_SURFACE:
				surf = ddiOpenSurface(*((uint32_t*)ptr));
				if (surf ==  NULL)
				{
					if (errOut != NULL) *errOut = GWM_ERR_NOSURF;
					return NULL;
				};
				return surf;
			case GWM_TYPE_COLOR:
			case GWM_TYPE_INT:
				return ptr;
			};
		};
	};
	
	if (errOut != NULL) *errOut = GWM_ERR_NOENT;
	return NULL;
};
