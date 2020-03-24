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
	{"gwm.toolkit.selection",		GWM_TYPE_COLOR,		offsetof(GWMInfo, colSelection)},
	{"gwm.toolkit.winback",			GWM_TYPE_COLOR,		offsetof(GWMInfo, colWinBack)},
	{"gwm.toolkit.faint",			GWM_TYPE_COLOR,		offsetof(GWMInfo, colFaint)},
	{"gwm.toolkit.editor",			GWM_TYPE_COLOR,		offsetof(GWMInfo, colEditor)},
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
	{"gwm.toolkit.notebook",		GWM_TYPE_SURFACE,	offsetof(GWMInfo, imgNotebook)},
	{"gwm.toolkit.progress.left",		GWM_TYPE_COLOR,		offsetof(GWMInfo, colProgressLeft)},
	{"gwm.toolkit.progress.right",		GWM_TYPE_COLOR,		offsetof(GWMInfo, colProgressRight)},
	{"gwm.toolkit.progress.bg",		GWM_TYPE_COLOR,		offsetof(GWMInfo, colProgressBackground)},
	{"gwm.toolkit.toolbtn",			GWM_TYPE_SURFACE,	offsetof(GWMInfo, imgToolButton)},
	{"gwm.toolkit.combo",			GWM_TYPE_SURFACE,	offsetof(GWMInfo, imgCombo)},
	{"gwm.toolkit.spin",			GWM_TYPE_SURFACE,	offsetof(GWMInfo, imgSpin)},
	{"gwm.toolkit.treeptr",			GWM_TYPE_SURFACE,	offsetof(GWMInfo, imgTreePtr)},
	{"gwm.toolkit.frame",			GWM_TYPE_COLOR,		offsetof(GWMInfo, colFrame)},
	{"gwm.toolkit.stock.back",		GWM_TYPE_SURFACE,	offsetof(GWMInfo, imgStockBack)},
	{"gwm.toolkit.stock.forward",		GWM_TYPE_SURFACE,	offsetof(GWMInfo, imgStockForward)},
	{"gwm.toolkit.stock.up",		GWM_TYPE_SURFACE,	offsetof(GWMInfo, imgStockUp)},
	{"gwm.toolkit.stock.down",		GWM_TYPE_SURFACE,	offsetof(GWMInfo, imgStockDown)},
	{"gwm.toolkit.stock.newfile",		GWM_TYPE_SURFACE,	offsetof(GWMInfo, imgStockNewFile)},
	{"gwm.toolkit.stock.openfile",		GWM_TYPE_SURFACE,	offsetof(GWMInfo, imgStockOpenFile)},
	{"gwm.toolkit.stock.save",		GWM_TYPE_SURFACE,	offsetof(GWMInfo, imgStockSave)},
	{"gwm.toolkit.stock.saveas",		GWM_TYPE_SURFACE,	offsetof(GWMInfo, imgStockSaveAs)},
	{"gwm.toolkit.stock.savecopy",		GWM_TYPE_SURFACE,	offsetof(GWMInfo, imgStockSaveCopy)},
	{"gwm.toolkit.stock.refresh",		GWM_TYPE_SURFACE,	offsetof(GWMInfo, imgStockRefresh)},
	{"gwm.toolkit.stock.cancel",		GWM_TYPE_SURFACE,	offsetof(GWMInfo, imgStockCancel)},
	{"gwm.toolkit.stock.confirm",		GWM_TYPE_SURFACE,	offsetof(GWMInfo, imgStockConfirm)},
	{"gwm.toolkit.stock.cut",		GWM_TYPE_SURFACE,	offsetof(GWMInfo, imgStockCut)},
	{"gwm.toolkit.stock.copy",		GWM_TYPE_SURFACE,	offsetof(GWMInfo, imgStockCopy)},
	{"gwm.toolkit.stock.paste",		GWM_TYPE_SURFACE,	offsetof(GWMInfo, imgStockPaste)},
	{"gwm.toolkit.stock.add",		GWM_TYPE_SURFACE,	offsetof(GWMInfo, imgStockAdd)},
	{"gwm.toolkit.stock.remove",		GWM_TYPE_SURFACE,	offsetof(GWMInfo, imgStockRemove)},
	{"gwm.toolkit.stock.home",		GWM_TYPE_SURFACE,	offsetof(GWMInfo, imgStockHome)},
	{"gwm.toolkit.stock.undo",		GWM_TYPE_SURFACE,	offsetof(GWMInfo, imgStockUndo)},
	{"gwm.toolkit.stock.redo",		GWM_TYPE_SURFACE,	offsetof(GWMInfo, imgStockRedo)},
	{"gwm.toolkit.stock.adjleft",		GWM_TYPE_SURFACE,	offsetof(GWMInfo, imgStockAdjLeft)},
	{"gwm.toolkit.stock.adjcenter",		GWM_TYPE_SURFACE,	offsetof(GWMInfo, imgStockAdjCenter)},
	{"gwm.toolkit.stock.adjright",		GWM_TYPE_SURFACE,	offsetof(GWMInfo, imgStockAdjRight)},
	{"gwm.toolkit.stock.justify",		GWM_TYPE_SURFACE,	offsetof(GWMInfo, imgStockJustify)},
	{"gwm.toolkit.stock.selectall",		GWM_TYPE_SURFACE,	offsetof(GWMInfo, imgStockSelectAll)},
	{"gwm.toolkit.stock.edit",		GWM_TYPE_SURFACE,	offsetof(GWMInfo, imgStockEdit)},
	{"gwm.toolkit.stock.bold",		GWM_TYPE_SURFACE,	offsetof(GWMInfo, imgStockBold)},
	{"gwm.toolkit.stock.italic",		GWM_TYPE_SURFACE,	offsetof(GWMInfo, imgStockItalic)},
	{"gwm.toolkit.stock.underline",		GWM_TYPE_SURFACE,	offsetof(GWMInfo, imgStockUnderline)},
	{"gwm.toolkit.stock.strike",		GWM_TYPE_SURFACE,	offsetof(GWMInfo, imgStockStrike)},
	{"gwm.toolkit.border.light",		GWM_TYPE_COLOR,		offsetof(GWMInfo, colBorderLight)},
	{"gwm.toolkit.border.dark",		GWM_TYPE_COLOR,		offsetof(GWMInfo, colBorderDark)},
	{"gwm.sysbar.sysbar",			GWM_TYPE_SURFACE,	offsetof(GWMInfo, imgSysbar)},
	{"gwm.sysbar.menu",			GWM_TYPE_SURFACE,	offsetof(GWMInfo, imgSysbarMenu)},
	{"gwm.sysbar.taskbtn",			GWM_TYPE_SURFACE,	offsetof(GWMInfo, imgTaskButton)},
	{"gwm.syntax.keyword",			GWM_TYPE_COLOR,		offsetof(GWMInfo, colSyntaxKeyword)},
	{"gwm.syntax.type",			GWM_TYPE_COLOR,		offsetof(GWMInfo, colSyntaxType)},
	{"gwm.syntax.const",			GWM_TYPE_COLOR,		offsetof(GWMInfo, colSyntaxConstant)},
	{"gwm.syntax.number",			GWM_TYPE_COLOR,		offsetof(GWMInfo, colSyntaxNumber)},
	{"gwm.syntax.builtin",			GWM_TYPE_COLOR,		offsetof(GWMInfo, colSyntaxBuiltin)},
	{"gwm.syntax.comment",			GWM_TYPE_COLOR,		offsetof(GWMInfo, colSyntaxComment)},
	{"gwm.syntax.string",			GWM_TYPE_COLOR,		offsetof(GWMInfo, colSyntaxString)},
	{"gwm.syntax.preproc",			GWM_TYPE_COLOR,		offsetof(GWMInfo, colSyntaxPreproc)},

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

static void drawTab(DDISurface *surf, int x, int y, int width, int height, DDIColor *bg, int active)
{
	static DDIColor black = {0, 0, 0, 0xFF};
	ddiFillRect(surf, x, y, width, height, bg);
	ddiFillRect(surf, x, y, width, 1, &black);
	ddiFillRect(surf, x, y, 1, height, &black);
	ddiFillRect(surf, x+width-1, y, 1, height, &black);
	
	if (!active)
	{
		ddiFillRect(surf, x, y+height-1, width, 1, &black);
	};
};

static void stockFiller(DDIPixelFormat *format, uint32_t *id)
{
	DDISurface *surf = surfaceSetup(format, id, 24, 24);
	static DDIColor green = {0x22, 0xFF, 0x22, 0xFF};
	ddiFillRect(surf, 0, 0, 24, 24, &green);
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
	surf = surfaceSetup(format, &info->imgButton, 34, 120);
	if (surf == NULL) return -1;
	static DDIColor buttonNormal = {0xDD, 0xDD, 0xDD, 0xFF};
	static DDIColor buttonHover = {0xEE, 0xEE, 0xEE, 0xFF};
	static DDIColor buttonDisabled = {0x55, 0x55, 0x55, 0xFF};
	drawButton(surf, 0, 0, 17, 30, &buttonNormal, 0);
	drawButton(surf, 0, 30, 17, 30, &buttonHover, 0);
	drawButton(surf, 0, 60, 17, 30, &buttonNormal, 1);
	drawButton(surf, 0, 90, 17, 30, &buttonDisabled, 0);
	drawButton(surf, 17, 0, 17, 30, &buttonNormal, 0);
	drawButton(surf, 17, 30, 17, 30, &buttonHover, 0);
	drawButton(surf, 17, 60, 17, 30, &buttonNormal, 1);
	drawButton(surf, 17, 90, 17, 30, &buttonDisabled, 0);
	
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

	// selection color
	memcpy(&info->colSelection, &colWinActive, sizeof(DDIColor));
	
	// window background
	DDIColor winBack = {0xDD, 0xDD, 0xDD, 0xFF};
	memcpy(&info->colWinBack, &winBack, sizeof(DDIColor));

	// notebook
	surf = surfaceSetup(format, &info->imgNotebook, 17, 60);
	drawTab(surf, 0, 0, 17, 20, &buttonNormal, 0);
	drawTab(surf, 0, 20, 17, 20, &buttonHover, 0);
	drawTab(surf, 0, 40, 17, 20, &winBack, 1);
	
	// progress bar gradient
	memcpy(&info->colProgressLeft, &colWinInactive, sizeof(DDIColor));
	memcpy(&info->colProgressRight, &colWinActive, sizeof(DDIColor));
	static DDIColor progressBackground = {0xAA, 0xAA, 0xAA, 0xFF};
	memcpy(&info->colProgressBackground, &progressBackground, sizeof(DDIColor));
	
	// tool button
	surf = surfaceSetup(format, &info->imgToolButton, 30, 90);
	static DDIColor transparent = {0, 0, 0, 0};
	ddiFillRect(surf, 0, 0, 30, 30, &transparent);
	drawButton(surf, 0, 30, 30, 30, &transparent, 0);
	drawButton(surf, 0, 60, 30, 30, &transparent, 1);
	
	// stock icons; just use a filler
	stockFiller(format, &info->imgStockBack);
	stockFiller(format, &info->imgStockForward);
	stockFiller(format, &info->imgStockUp);
	stockFiller(format, &info->imgStockDown);
	stockFiller(format, &info->imgStockNewFile);
	stockFiller(format, &info->imgStockOpenFile);
	stockFiller(format, &info->imgStockSave);
	stockFiller(format, &info->imgStockSaveAs);
	stockFiller(format, &info->imgStockSaveCopy);
	stockFiller(format, &info->imgStockRefresh);
	stockFiller(format, &info->imgStockCancel);
	stockFiller(format, &info->imgStockConfirm);
	stockFiller(format, &info->imgStockCut);
	stockFiller(format, &info->imgStockCopy);
	stockFiller(format, &info->imgStockPaste);
	stockFiller(format, &info->imgStockAdd);
	stockFiller(format, &info->imgStockRemove);
	stockFiller(format, &info->imgStockHome);
	stockFiller(format, &info->imgStockUndo);
	stockFiller(format, &info->imgStockRedo);
	stockFiller(format, &info->imgStockAdjLeft);
	stockFiller(format, &info->imgStockAdjCenter);
	stockFiller(format, &info->imgStockAdjRight);
	stockFiller(format, &info->imgStockJustify);
	stockFiller(format, &info->imgStockSelectAll);
	stockFiller(format, &info->imgStockEdit);
	stockFiller(format, &info->imgStockBold);
	stockFiller(format, &info->imgStockItalic);
	stockFiller(format, &info->imgStockUnderline);
	stockFiller(format, &info->imgStockStrike);
	
	// editor color (white)
	static DDIColor white = {0xFF, 0xFF, 0xFF, 0xFF};
	memcpy(&info->colEditor, &white, sizeof(DDIColor));

	// combo box
	surf = surfaceSetup(format, &info->imgCombo, 20, 80);
	drawButton(surf, 0, 0, 20, 20, &buttonNormal, 0);
	drawButton(surf, 0, 20, 20, 20, &buttonHover, 0);
	drawButton(surf, 0, 40, 20, 20, &buttonNormal, 1);
	drawButton(surf, 0, 60, 20, 20, &buttonDisabled, 0);

	// spinner controls
	surf = surfaceSetup(format, &info->imgSpin, 40, 80);
	drawButton(surf, 0, 0, 20, 20, &red, 0);
	drawButton(surf, 0, 20, 20, 20, &red, 0);
	drawButton(surf, 0, 40, 20, 20, &red, 1);
	drawButton(surf, 0, 60, 20, 20, &red, 0);

	drawButton(surf, 20, 0, 20, 20, &green, 0);
	drawButton(surf, 20, 20, 20, 20, &green, 0);
	drawButton(surf, 20, 40, 20, 20, &green, 1);
	drawButton(surf, 20, 60, 20, 20, &green, 0);

	// tree pointer
	surf = surfaceSetup(format, &info->imgTreePtr, 16, 16);
	drawButton(surf, 0, 0, 16, 16, &green, 0);
	
	// frame color
	memcpy(&info->colFrame, &white, sizeof(DDIColor));
	
	// border
	memcpy(&info->colBorderDark, &white, sizeof(DDIColor));
	memcpy(&info->colBorderLight, &white, sizeof(DDIColor));
	
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
