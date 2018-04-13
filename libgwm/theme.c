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
	{"gwm.toolkit.mbicons",			GWM_TYPE_SURFACE,	offsetof(GWMInfo, imgMessageIcons)},
	{"gwm.toolkit.button",			GWM_TYPE_SURFACE,	offsetof(GWMInfo, imgButton)},
	{"gwm.toolkit.checkbox",		GWM_TYPE_SURFACE,	offsetof(GWMInfo, imgCheckbox)},
	{"gwm.toolkit.radio",			GWM_TYPE_SURFACE,	offsetof(GWMInfo, imgRadioButton)},
	{"gwm.toolkit.optmenu",			GWM_TYPE_SURFACE,	offsetof(GWMInfo, imgOptionMenu)},
	{"gwm.toolkit.slider",			GWM_TYPE_SURFACE,	offsetof(GWMInfo, imgSlider)},
	{"gwm.toolkit.slider.active",		GWM_TYPE_COLOR,		offsetof(GWMInfo, colSliderActive)},
	{"gwm.toolkit.slider.inactive",		GWM_TYPE_COLOR,		offsetof(GWMInfo, colSliderInactive)},
	{"gwm.toolkit.slider.disabled",		GWM_TYPE_COLOR,		offsetof(GWMInfo, colSliderDisabled)},
	{"gwm.toolkit.scrollbar.bg",		GWM_TYPE_COLOR,		offsetof(GWMInfo, colScrollbarBg)},
	{"gwm.toolkit.scrollbar.fg",		GWM_TYPE_COLOR,		offsetof(GWMInfo, colScrollbarFg)},
	{"gwm.toolkit.scrollbar.disabled",	GWM_TYPE_COLOR,		offsetof(GWMInfo, colScrollbarDisabled)},
	{"gwm.sysbar.sysbar",			GWM_TYPE_SURFACE,	offsetof(GWMInfo, imgSysbar)},
	{"gwm.sysbar.menu",			GWM_TYPE_SURFACE,	offsetof(GWMInfo, imgSysbarMenu)},
	{"gwm.sysbar.taskbtn",			GWM_TYPE_SURFACE,	offsetof(GWMInfo, imgTaskButton)},

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
	
	// message box icons
	surf = surfaceSetup(format, &info->imgMessageIcons, 160, 32);
	if (surf == NULL) return -1;
	static DDIColor msgColors[5] = {
		{0x00, 0x00, 0xFF, 0xFF},
		{0xFF, 0x00, 0xFF, 0xFF},
		{0xFF, 0xFF, 0x00, 0xFF},
		{0xFF, 0x00, 0x00, 0xFF},
		{0x00, 0xFF, 0x00, 0xFF}
	};
	for (i=0; i<5; i++)
	{
		ddiFillRect(surf, 32*i, 0, 32, 32, &msgColors[i]);
	};
	
	// button
	surf = surfaceSetup(format, &info->imgButton, 17, 120);
	if (surf == NULL) return -1;
	static DDIColor buttonNormal = {0xDD, 0xDD, 0xDD, 0xFF};
	static DDIColor buttonHover = {0xEE, 0xEE, 0xEE, 0xFF};
	static DDIColor buttonDisabled = {0x55, 0x55, 0x55, 0xFF};
	drawButton(surf, 0, 0, 17, 30, &buttonNormal, 0);
	drawButton(surf, 0, 30, 17, 30, &buttonHover, 0);
	drawButton(surf, 0, 60, 17, 30, &buttonNormal, 1);
	drawButton(surf, 0, 90, 17, 30, &buttonDisabled, 0);
	
	// checkbox
	surf = surfaceSetup(format, &info->imgCheckbox, 60, 80);
	if (surf == NULL) return -1;
	static DDIColor cbUnchecked = {0xFF, 0x00, 0x00, 0xFF};
	static DDIColor cbChecked = {0x00, 0xFF, 0x00, 0xFF};
	static DDIColor cbTristate = {0x00, 0x00, 0xFF, 0xFF};
	drawButton(surf, 0, 0, 20, 20, &cbUnchecked, 0);
	drawButton(surf, 0, 20, 20, 20, &cbUnchecked, 0);
	drawButton(surf, 0, 40, 20, 20, &cbUnchecked, 1);
	drawButton(surf, 0, 60, 20, 20, &buttonDisabled, 1);
	drawButton(surf, 20, 0, 20, 20, &cbChecked, 0);
	drawButton(surf, 20, 20, 20, 20, &cbChecked, 0);
	drawButton(surf, 20, 40, 20, 20, &cbChecked, 1);
	drawButton(surf, 20, 60, 20, 20, &buttonDisabled, 1);
	drawButton(surf, 40, 0, 20, 20, &cbTristate, 0);
	drawButton(surf, 40, 20, 20, 20, &cbTristate, 0);
	drawButton(surf, 40, 40, 20, 20, &cbTristate, 1);
	drawButton(surf, 40, 60, 20, 20, &buttonDisabled, 1);

	// radiobutton
	surf = surfaceSetup(format, &info->imgRadioButton, 40, 80);
	if (surf == NULL) return -1;
	drawButton(surf, 0, 0, 20, 20, &cbUnchecked, 0);
	drawButton(surf, 0, 20, 20, 20, &cbUnchecked, 0);
	drawButton(surf, 0, 40, 20, 20, &cbUnchecked, 1);
	drawButton(surf, 0, 60, 20, 20, &buttonDisabled, 1);
	drawButton(surf, 20, 0, 20, 20, &cbChecked, 0);
	drawButton(surf, 20, 20, 20, 20, &cbChecked, 0);
	drawButton(surf, 20, 40, 20, 20, &cbChecked, 1);
	drawButton(surf, 20, 60, 20, 20, &buttonDisabled, 1);

	// system bar
	surf = surfaceSetup(format, &info->imgSysbar, 1, 40);
	if (surf == NULL) return -1;
	DDIColor sysbarColor = {0x7F, 0x7F, 0x7F, 0xFF};
	int y;
	for (y=0; y<40; y++)
	{
		ddiFillRect(surf, 0, y, 1, 1, &sysbarColor);
		sysbarColor.red += 3;
		sysbarColor.green += 3;
		sysbarColor.blue += 3;
	};
	
	// system menu button
	surf = surfaceSetup(format, &info->imgSysbarMenu, 32, 32);
	if (surf == NULL) return -1;
	drawButton(surf, 0, 0, 32, 32, &blue, 0);
	
	// task bar window buttons
	surf = surfaceSetup(format, &info->imgTaskButton, 160, 32);
	if (surf == NULL) return -1;
	static DDIColor orange = {0x7F, 0x7F, 0x00, 0xFF};
	drawButton(surf, 0, 0, 32, 32, &buttonNormal, 1);
	drawButton(surf, 32, 0, 32, 32, &orange, 0);
	drawButton(surf, 64, 0, 32, 32, &buttonHover, 0);
	drawButton(surf, 96, 0, 32, 32, &buttonNormal, 1);
	drawButton(surf, 128, 0, 32, 32, &buttonNormal, 0);
	
	// option menu
	surf = surfaceSetup(format, &info->imgOptionMenu, 20, 80);
	drawButton(surf, 0, 0, 20, 20, &buttonNormal, 0);
	drawButton(surf, 0, 20, 20, 20, &buttonHover, 0);
	drawButton(surf, 0, 40, 20, 20, &buttonNormal, 1);
	drawButton(surf, 0, 60, 20, 20, &buttonDisabled, 0);
	
	// slider
	surf = surfaceSetup(format, &info->imgSlider, 20, 20);
	static DDIColor green = {0x00, 0x7F, 0x00, 0xFF};
	drawButton(surf, 0, 0, 20, 20, &green, 0);
	
	// slider colors
	memcpy(&info->colSliderActive, &colActiveCap, sizeof(DDIColor));
	memcpy(&info->colSliderInactive, &colInactiveCap, sizeof(DDIColor));
	memcpy(&info->colSliderDisabled, &buttonDisabled, sizeof(DDIColor));
	
	// scrollbar
	memcpy(&info->colScrollbarBg, &colWinInactive, sizeof(DDIColor));
	memcpy(&info->colScrollbarFg, &colWinActive, sizeof(DDIColor));
	memcpy(&info->colScrollbarDisabled, &buttonDisabled, sizeof(DDIColor));
	
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
