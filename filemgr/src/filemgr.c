/*
	Glidix GUI

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

#include <assert.h>
#include <sys/wait.h>
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
#include <errno.h>
#include <pwd.h>

#include "dirview.h"
#include "filemgr.h"
#include "places.h"

GWMWindow *topWindow;
GWMTextField *txtPath;
GWMTextField *txtSearch;
DirView *dirView;
GWMMenu *menuEdit;
DDIFont *fntCategory;
Places *places;
int desktopMode;

static int filemgrCommand(GWMCommandEvent *ev, GWMWindow *win)
{
	char *newpath;
	const char *location = dvGetLocation(dirView);
	struct passwd *pwd;
	
	switch (ev->symbol)
	{
	case GWM_SYM_UP:
		newpath = (char*) malloc(strlen(location)+4);
		sprintf(newpath, "%s/..", location);
		dvGoTo(dirView, newpath);
		free(newpath);
		return GWM_EVSTATUS_OK;
	case GWM_SYM_OK:
		if (ev->header.win == txtPath->id) dvGoTo(dirView, gwmReadTextField(txtPath));
		return GWM_EVSTATUS_OK;
	case GWM_SYM_REFRESH:
		dvGoTo(dirView, dvGetLocation(dirView));
		return GWM_EVSTATUS_OK;
	case GWM_SYM_HOME:
		pwd = getpwuid(geteuid());
		assert(pwd != NULL);
		dvGoTo(dirView, pwd->pw_dir);
		return GWM_EVSTATUS_OK;
	case GWM_SYM_RENAME:
		dvRename(dirView);
		return GWM_EVSTATUS_OK;
	case DV_SYM_MKDIR:
		dvMakeDir(dirView);
		return GWM_EVSTATUS_OK;
	case GWM_SYM_NEW_FILE:
		dvMakeFile(dirView, NULL, 0);
		return GWM_EVSTATUS_OK;
	case DV_SYM_TERMINAL:
		dvTerminal(dirView);
		return GWM_EVSTATUS_OK;
	default:
		return GWM_EVSTATUS_CONT;
	};
};

static int filemgrHandler(GWMEvent *ev, GWMWindow *win, void *context)
{
	switch (ev->type)
	{
	case GWM_EVENT_COMMAND:
		return filemgrCommand((GWMCommandEvent*) ev, win);
	case DV_EVENT_CHDIR:
		if (!desktopMode)
		{
			gwmWriteTextField(txtPath, dvGetLocation(dirView));
			plRedraw(places);
		};
		return GWM_EVSTATUS_OK;
	default:
		return GWM_EVSTATUS_CONT;
	};
};

void makeEditMenu()
{
	DDIPixelFormat screenFormat;
	gwmGetScreenFormat(&screenFormat);
	
	DDISurface *iconTerm = ddiLoadAndConvertPNG(&screenFormat, "/usr/share/images/terminal.png", NULL);
	
	menuEdit = gwmCreateMenu();
	
	GWMMenu *menuNew = gwmMenuAddSub(menuEdit, "New...");
	gwmMenuAddCommand(menuNew, DV_SYM_MKDIR, "Directory", NULL);
	gwmMenuAddCommand(menuNew, GWM_SYM_NEW_FILE, "Empty file", NULL);

	gwmMenuAddSeparator(menuEdit);
	gwmMenuAddCommand(menuEdit, DV_SYM_TERMINAL, "Open terminal", NULL);
	gwmMenuSetIcon(menuEdit, iconTerm);
	
	gwmMenuAddSeparator(menuEdit);
	gwmMenuAddCommand(menuEdit, GWM_SYM_RENAME, NULL, NULL);
};

int main(int argc, char *argv[])
{
	// TODO: command line arguments for selecting a file or directory or whatever
	if (gwmInit() != 0)
	{
		fprintf(stderr, "%s: gwmInit() failed\n", argv[0]);
		return 1;
	};
	
	fntCategory = ddiLoadFont("DejaVu Sans", 12, DDI_STYLE_BOLD, NULL);
	assert(fntCategory != NULL);
	
	if (argv[0][0] == '-')
	{
		desktopMode = 1;
		makeEditMenu();
		
		// requesting to open the desktop window
		int width, height;
		gwmScreenSize(&width, &height);
		
		topWindow = gwmCreateWindow(NULL, "File manager", 0, 0, width, height-40,
						GWM_WINDOW_HIDDEN | GWM_WINDOW_NOTASKBAR | GWM_WINDOW_NODECORATE);
		GWMLayout *boxLayout = gwmCreateBoxLayout(GWM_BOX_VERTICAL);
		gwmSetWindowLayout(topWindow, boxLayout);
		
		dirView = dvNew(topWindow);
		gwmBoxLayoutAddWindow(boxLayout, dirView, 1, 0, GWM_BOX_FILL);
		
		dvGoTo(dirView, "Desktop");
		gwmLayout(topWindow, width, height-40);
		DDISurface *canvas = gwmGetWindowCanvas(topWindow);
		static DDIColor transparent = {0, 0, 0, 0};
		ddiFillRect(canvas, 0, 0, canvas->width, canvas->height, &transparent);
		gwmPostDirty(topWindow);
		gwmPushEventHandler(topWindow, filemgrHandler, NULL);
		gwmSetWindowFlags(topWindow, GWM_WINDOW_NORESTACK | GWM_WINDOW_NOTASKBAR);
		gwmMainLoop();
		return 0;
	};
	
	topWindow = gwmCreateWindow(NULL, "File manager",
						GWM_POS_UNSPEC, GWM_POS_UNSPEC,
						0, 0,
						GWM_WINDOW_HIDDEN | GWM_WINDOW_NOTASKBAR);
	
	gwmPushEventHandler(topWindow, filemgrHandler, NULL);
	
	DDISurface *canvas = gwmGetWindowCanvas(topWindow);
	DDISurface *icon = ddiLoadAndConvertPNG(&canvas->format, "/usr/share/images/fileman.png", NULL);
	assert(icon != NULL);
	gwmSetWindowIcon(topWindow, icon);
	
	GWMLayout *boxLayout = gwmCreateBoxLayout(GWM_BOX_VERTICAL);
	
	GWMWindowTemplate wt;
	wt.wtComps = GWM_WTC_MENUBAR | GWM_WTC_TOOLBAR;
	wt.wtWindow = topWindow;
	wt.wtBody = boxLayout;
	gwmCreateTemplate(&wt);
	
	GWMWindow *menubar = wt.wtMenubar;
	
	GWMMenu *menuFile = gwmCreateMenu();
	gwmMenubarAdd(menubar, "File", menuFile);
	
	gwmMenuAddCommand(menuFile, GWM_SYM_EXIT, NULL, NULL);
	
	makeEditMenu();
	gwmMenubarAdd(menubar, "Edit", menuEdit);
	
	GWMLayout *toolbar = wt.wtToolbar;
	GWMWindow *toolBack = gwmNewToolButton(topWindow);
	gwmSetToolButtonSymbol(toolBack, GWM_SYM_BACK);
	gwmBoxLayoutAddWindow(toolbar, toolBack, 0, 0, 0);

	GWMWindow *toolForward = gwmNewToolButton(topWindow);
	gwmSetToolButtonSymbol(toolForward, GWM_SYM_FORWARD);
	gwmBoxLayoutAddWindow(toolbar, toolForward, 0, 0, 0);

	GWMWindow *toolUp = gwmNewToolButton(topWindow);
	gwmSetToolButtonSymbol(toolUp, GWM_SYM_UP);
	gwmBoxLayoutAddWindow(toolbar, toolUp, 0, 0, 0);
	
	GWMWindow *toolRefresh = gwmNewToolButton(topWindow);
	gwmSetToolButtonSymbol(toolRefresh, GWM_SYM_REFRESH);
	gwmBoxLayoutAddWindow(toolbar, toolRefresh, 0, 0, 0);
	
	txtPath = gwmNewTextField(topWindow);
	gwmBoxLayoutAddWindow(toolbar, txtPath, 3, 2, GWM_BOX_ALL);
	gwmSetTextFieldIcon(txtPath, icon);
	
	txtSearch = gwmNewTextField(topWindow);
	gwmBoxLayoutAddWindow(toolbar, txtSearch, 1, 2, GWM_BOX_ALL);
	gwmSetTextFieldPlaceholder(txtSearch, "Search...");
	
	GWMToolButton *toolHome = gwmNewToolButton(topWindow);
	gwmSetToolButtonSymbol(toolHome, GWM_SYM_HOME);
	gwmBoxLayoutAddWindow(toolbar, toolHome, 0, 0, 0);
	
	GWMSplitter *split = gwmNewSplitter(topWindow);
	gwmSetSplitterFlags(split, GWM_SPLITTER_HORIZ);
	gwmBoxLayoutAddWindow(boxLayout, split, 1, 0, GWM_BOX_FILL);
	
	GWMWindow *panelLeft = gwmGetSplitterPanel(split, 0);
	GWMLayout *layoutLeft = gwmCreateBoxLayout(GWM_BOX_HORIZONTAL);
	gwmSetWindowLayout(panelLeft, layoutLeft);
	
	Places *pl = plNew(panelLeft);
	places = pl;
	gwmBoxLayoutAddWindow(layoutLeft, pl, 1, 0, GWM_BOX_FILL);
	
	struct passwd *pwd = getpwuid(geteuid());
	assert(pwd != NULL);
	
	Category *catPlaces = plNewCat(pl, "Places");
	plAddBookmark(catPlaces, icon, "Computer", "://computer");
	plAddBookmark(catPlaces, icon, "Filesystem", "/");
	plAddBookmark(catPlaces, ddiScale(gwmGetStockIcon(GWM_SYM_HOME), 16, 16, DDI_SCALE_BEST), "Home", pwd->pw_dir);

	GWMWindow *panelRight = gwmGetSplitterPanel(split, 1);
	GWMLayout *layoutRight = gwmCreateBoxLayout(GWM_BOX_HORIZONTAL);
	gwmSetWindowLayout(panelRight, layoutRight);
	
	DirView *dv = dvNew(panelRight);
	dirView = dv;
	gwmBoxLayoutAddWindow(layoutRight, dv, 1, 0, GWM_BOX_FILL);
	if (dvGoTo(dv, ".") != 0) return 1;
	
	GWMScrollbar *sbarDir = gwmNewScrollbar(panelRight);
	gwmSetScrollbarFlags(sbarDir, GWM_SCROLLBAR_DISABLED);
	gwmBoxLayoutAddWindow(layoutRight, sbarDir, 0, 0, GWM_BOX_FILL);
	dvAttachScrollbar(dv, sbarDir);
	
	gwmFit(topWindow);
	gwmSetWindowFlags(topWindow, GWM_WINDOW_MKFOCUSED | GWM_WINDOW_RESIZEABLE);
	gwmMainLoop();
	gwmQuit();
	
	return 0;
};
