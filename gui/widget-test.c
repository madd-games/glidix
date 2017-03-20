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

#include <sys/glidix.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <libddi.h>
#include <pthread.h>
#include <fcntl.h>
#include <unistd.h>
#include <signal.h>
#include <libgwm.h>

GWMWindow *win;
GWMWindow *menubar;
GWMMenu *menu;
DDIFont *fntExample;

void redraw()
{
	DDISurface *canvas = gwmGetWindowCanvas(win);

	DDIPen *pen = ddiCreatePen(&canvas->format, gwmGetDefaultFont(), 0, 0, canvas->width, canvas->height, 0, 0, NULL);
	ddiWritePen(pen, "Tick me");
	
	int txtWidth, txtHeight;
	ddiGetPenSize(pen, &txtWidth, &txtHeight);
	ddiSetPenPosition(pen, 126, 37-(txtHeight/2));
	ddiExecutePen(pen, canvas);
	ddiDeletePen(pen);

	gwmPostDirty(win);
};

int myEventHandler(GWMEvent *ev, GWMWindow *win)
{
	switch (ev->type)
	{
	case GWM_EVENT_CLOSE:
		return -1;
	case GWM_EVENT_RESIZE_REQUEST:
		gwmMoveWindow(win, ev->x, ev->y);
		gwmResizeWindow(win, ev->width, ev->height);
		redraw();
		gwmMenubarAdjust(menubar);
		return 0;
	case GWM_EVENT_DOUBLECLICK:
		printf("Double-click!\n");
		return 0;
	default:
		return gwmDefaultHandler(ev, win);
	};
};

int sbarCallback(void *context)
{
	GWMWindow *sbar = (GWMWindow*) context;
	printf("You moved a scrollbar to: %d\n", gwmGetScrollbarOffset(sbar));
	return 0;
};

int menuCallback(void *context)
{
	printf("You selected: %s\n", (const char*) context);
	return 0;
};

int menuExit(void *context)
{
	printf("You clicked the exit option!\n");
	return -1;
};

int main()
{
	if (gwmInit() != 0)
	{
		fprintf(stderr, "Failed to initialize GWM!\n");
		return 1;
	};

	win = gwmCreateWindow(NULL, "Widgets Demo", 10, 10, 500, 500, GWM_WINDOW_NOTASKBAR | GWM_WINDOW_HIDDEN);
	if (win == NULL)
	{
		fprintf(stderr, "Failed to create window!\n");
		return 1;
	};

	menubar = gwmCreateMenubar(win);

	GWMMenu *menuTest = gwmCreateMenu();
	gwmMenuAddEntry(menuTest, "Entry 1", menuCallback, (void*) "Entry 1");
	gwmMenuAddEntry(menuTest, "Entry 2", menuCallback, (void*) "Entry 2");
	gwmMenuAddSeparator(menuTest);
	GWMMenu *menuSub = gwmMenuAddSub(menuTest, "My Submenu");
	gwmMenuAddSub(menuTest, "Empty Submenu");
	gwmMenuAddEntry(menuTest, "Exit", menuExit, NULL);
	
	gwmMenuAddEntry(menuSub, "Submenu Item", menuCallback, (void*) "Submenu Item");

	GWMMenu *menuEmpty = gwmCreateMenu();
	gwmMenubarAdd(menubar, "Test", menuTest);
	gwmMenubarAdd(menubar, "Empty", menuEmpty);
	gwmMenubarAdjust(menubar);

	gwmCreateButton(win, "Sample button", 2, 22, 100, 0);
	gwmCreateButton(win, "Disabled button", 2, 54, 100, GWM_BUTTON_DISABLED);
	gwmCreateTextField(win, "Type here :)", 2, 86, 100, 0);
	gwmCreateTextField(win, "Disabled text field", 2, 108, 100, GWM_TXT_DISABLED);
	gwmCreateTextField(win, "Masked", 2, 130, 100, GWM_TXT_MASKED);
	gwmCreateTextField(win, "Disabled, masked", 2, 152, 100, GWM_TXT_MASKED | GWM_TXT_DISABLED);

	gwmCreateCheckbox(win, 104, 27, GWM_CB_TRI, 0);
	
	int i;
	for (i=0; i<3; i++)
	{
		gwmCreateCheckbox(win, 104+32*i, 59, i, GWM_CB_DISABLED);
	};
	
	GWMWindow *sbar;
	sbar = gwmCreateScrollbar(win, 104, 81, 100, 0, 66, 500, GWM_SCROLLBAR_HORIZ);
	gwmSetScrollbarCallback(sbar, sbarCallback, sbar);
	sbar = gwmCreateScrollbar(win, 104, 91, 100, 50, 66, 500, GWM_SCROLLBAR_HORIZ | GWM_SCROLLBAR_DISABLED);
	gwmSetScrollbarCallback(sbar, sbarCallback, sbar);

	sbar = gwmCreateScrollbar(win, 206, 27, 100, 20, 100, 500, 0);
	gwmSetScrollbarCallback(sbar, sbarCallback, sbar);
	sbar = gwmCreateScrollbar(win, 216, 27, 100, 10, 100, 500, GWM_SCROLLBAR_DISABLED);
	gwmSetScrollbarCallback(sbar, sbarCallback, sbar);

	GWMRadioGroup *group = gwmCreateRadioGroup(0);
	for (i=0; i<4; i++)
	{
		int flags = 0;
		if (i == 0) flags = GWM_RADIO_DISABLED;
		gwmCreateRadioButton(win, 104+32*i, 130, group, i, flags);
	};
	
	GWMWindow *notebook = gwmCreateNotebook(win, 2, 174, 300, 200, 0);
	GWMWindow *tab1 = gwmNotebookAdd(notebook, "Tab 1");
	/*GWMWindow *tab2 =*/ gwmNotebookAdd(notebook, "Filesystem");
	/*GWMWindow *tab3 =*/ gwmNotebookAdd(notebook, "Yet another tab");
	gwmNotebookSetTab(notebook, 0);

	int pageWidth, pageHeight;
	gwmNotebookGetDisplaySize(notebook, &pageWidth, &pageHeight);

	DDISurface *canvas = gwmGetWindowCanvas(tab1);
	fntExample = ddiLoadFont("DejaVu Sans", 15, DDI_STYLE_ITALIC, NULL);

	DDIPen *pen = ddiCreatePen(&canvas->format, fntExample, 2, 2, pageWidth-4, pageHeight-4, 0, 0, NULL);
	ddiWritePen(pen, "This is some text on the first page.");
	ddiExecutePen(pen, canvas);
	ddiDeletePen(pen);
	
	///*GWMWindow *treeview =*/ gwmCreateTreeView(tab2, 2, 2, 250, 150, GWM_TREE_FILESYSTEM, GWM_FS_ROOT, 0);
	
	gwmPostDirty(tab1);
	redraw();

	gwmSetEventHandler(win, myEventHandler);
	gwmSetWindowFlags(win, GWM_WINDOW_MKFOCUSED | GWM_WINDOW_RESIZEABLE);
	gwmMainLoop();
	gwmQuit();
	return 0;
};
