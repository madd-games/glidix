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

int testMessageBoxes(void *ignore)
{
	(void)ignore;
	gwmMessageBox(win, "Widget Test", "GWM_MBICON_NONE | GWM_MBUT_OK", GWM_MBICON_NONE | GWM_MBUT_OK);
	gwmMessageBox(win, "Widget Test", "GWM_MBICON_NONE | GWM_MBUT_YESNO", GWM_MBICON_NONE | GWM_MBUT_YESNO);
	gwmMessageBox(win, "Widget Test", "GWM_MBICON_NONE | GWM_MBUT_OKCANCEL", GWM_MBICON_NONE | GWM_MBUT_OKCANCEL);
	gwmMessageBox(win, "Widget Test", "GWM_MBICON_NONE | GWM_MBUT_YESNOCANCEL", GWM_MBICON_NONE | GWM_MBUT_YESNOCANCEL);

	gwmMessageBox(win, "Widget Test", "GWM_MBICON_INFO | GWM_MBUT_OK", GWM_MBICON_INFO | GWM_MBUT_OK);
	gwmMessageBox(win, "Widget Test", "GWM_MBICON_INFO | GWM_MBUT_YESNO", GWM_MBICON_INFO | GWM_MBUT_YESNO);
	gwmMessageBox(win, "Widget Test", "GWM_MBICON_INFO | GWM_MBUT_OKCANCEL", GWM_MBICON_INFO | GWM_MBUT_OKCANCEL);
	gwmMessageBox(win, "Widget Test", "GWM_MBICON_INFO | GWM_MBUT_YESNOCANCEL", GWM_MBICON_INFO | GWM_MBUT_YESNOCANCEL);

	gwmMessageBox(win, "Widget Test", "GWM_MBICON_QUEST | GWM_MBUT_OK", GWM_MBICON_QUEST | GWM_MBUT_OK);
	gwmMessageBox(win, "Widget Test", "GWM_MBICON_QUEST | GWM_MBUT_YESNO", GWM_MBICON_QUEST | GWM_MBUT_YESNO);
	gwmMessageBox(win, "Widget Test", "GWM_MBICON_QUEST | GWM_MBUT_OKCANCEL", GWM_MBICON_QUEST | GWM_MBUT_OKCANCEL);
	gwmMessageBox(win, "Widget Test", "GWM_MBICON_QUEST | GWM_MBUT_YESNOCANCEL", GWM_MBICON_QUEST | GWM_MBUT_YESNOCANCEL);

	gwmMessageBox(win, "Widget Test", "GWM_MBICON_WARN | GWM_MBUT_OK", GWM_MBICON_WARN | GWM_MBUT_OK);
	gwmMessageBox(win, "Widget Test", "GWM_MBICON_WARN | GWM_MBUT_YESNO", GWM_MBICON_WARN | GWM_MBUT_YESNO);
	gwmMessageBox(win, "Widget Test", "GWM_MBICON_WARN | GWM_MBUT_OKCANCEL", GWM_MBICON_WARN | GWM_MBUT_OKCANCEL);
	gwmMessageBox(win, "Widget Test", "GWM_MBICON_WARN | GWM_MBUT_YESNOCANCEL", GWM_MBICON_WARN | GWM_MBUT_YESNOCANCEL);

	gwmMessageBox(win, "Widget Test", "GWM_MBICON_ERROR | GWM_MBUT_OK", GWM_MBICON_ERROR | GWM_MBUT_OK);
	gwmMessageBox(win, "Widget Test", "GWM_MBICON_ERROR | GWM_MBUT_YESNO", GWM_MBICON_ERROR | GWM_MBUT_YESNO);
	gwmMessageBox(win, "Widget Test", "GWM_MBICON_ERROR | GWM_MBUT_OKCANCEL", GWM_MBICON_ERROR | GWM_MBUT_OKCANCEL);
	gwmMessageBox(win, "Widget Test", "GWM_MBICON_ERROR | GWM_MBUT_YESNOCANCEL", GWM_MBICON_ERROR | GWM_MBUT_YESNOCANCEL);
	return 0;
};

int testInputDialog(void *ignore)
{
	(void)ignore;
	char *text = gwmGetInput("Widget Test", "Enter some text:", "Hello world");
	if (text == NULL)
	{
		gwmMessageBox(win, "Widget Test", "You clicked cancel.", GWM_MBICON_ERROR | GWM_MBUT_OK);
	}
	else
	{
		gwmMessageBox(win, "Widget Test", text, GWM_MBICON_INFO | GWM_MBUT_OK);
		free(text);
	};
	
	return 0;
};

int main()
{
	if (gwmInit() != 0)
	{
		fprintf(stderr, "Failed to initialize GWM!\n");
		return 1;
	};

	win = gwmCreateWindow(NULL, "Widgets Demo", 10, 10, 304, 464, GWM_WINDOW_NOTASKBAR | GWM_WINDOW_HIDDEN);
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
	
	GWMMenu *menuDialogs = gwmCreateMenu();
	gwmMenuAddEntry(menuDialogs, "Message Boxes", testMessageBoxes, NULL);
	gwmMenuAddEntry(menuDialogs, "Input Dialog", testInputDialog, NULL);
	
	gwmMenubarAdd(menubar, "Test", menuTest);
	gwmMenubarAdd(menubar, "Empty", menuEmpty);
	gwmMenubarAdd(menubar, "Dialogs", menuDialogs);
	gwmMenubarAdjust(menubar);

	gwmCreateButton(win, "Sample button", 2, 22, 100, 0);
	gwmCreateButton(win, "Disabled button", 2, 54, 100, GWM_BUTTON_DISABLED);
	gwmCreateTextField(win, "Type here :)", 2, 86, 100, 0);
	gwmCreateTextField(win, "Disabled text field", 2, 108, 100, GWM_TXT_DISABLED);
	gwmCreateTextField(win, "Masked", 2, 130, 100, GWM_TXT_MASKED);
	gwmCreateTextField(win, "Disabled, masked", 2, 152, 100, GWM_TXT_MASKED | GWM_TXT_DISABLED);
	GWMWindow *combo = gwmCreateCombo(win, "Combo box!!", 2, 174, 100, 0);
	gwmAddComboOption(combo, "Some option");
	gwmAddComboOption(combo, "Hello world!");
	gwmAddComboOption(combo, "Option 3");
	gwmCreateCombo(win, "Disable combo!!", 2, 196, 100, GWM_COMBO_DISABLED);
	GWMWindow *optmenu = gwmCreateOptionMenu(win, 0, "Option menu", 2, 218, 100, 0);
	gwmAddOptionMenu(optmenu, 1, "Cucumber");
	gwmAddOptionMenu(optmenu, 2, "Tomato");
	gwmAddOptionMenu(optmenu, 3, "Banana");
	gwmCreateOptionMenu(win, 0, "Disabled menu", 2, 240, 100, GWM_OPTMENU_DISABLED);
	
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
	gwmCreateSlider(win, 104, 101, 100, 30, 100, GWM_SLIDER_HORIZ);
	gwmCreateSlider(win, 104, 131, 100, 30, 100, GWM_SLIDER_HORIZ | GWM_SLIDER_DISABLED);
	
	sbar = gwmCreateScrollbar(win, 206, 27, 100, 20, 100, 500, 0);
	gwmSetScrollbarCallback(sbar, sbarCallback, sbar);
	sbar = gwmCreateScrollbar(win, 216, 27, 100, 10, 100, 500, GWM_SCROLLBAR_DISABLED);
	gwmSetScrollbarCallback(sbar, sbarCallback, sbar);
	gwmCreateSlider(win, 226, 27, 100, 40, 100, GWM_SLIDER_VERT);
	gwmCreateSlider(win, 256, 27, 100, 40, 100, GWM_SLIDER_VERT | GWM_SLIDER_DISABLED);
	
	GWMRadioGroup *group = gwmCreateRadioGroup(0);
	for (i=0; i<4; i++)
	{
		int flags = 0;
		if (i == 0) flags = GWM_RADIO_DISABLED;
		gwmCreateRadioButton(win, 104+32*i, 150, group, i, flags);
	};
	
	gwmCreateFrame(win, "Frame example", 104, 175, 200, 80);
	
	GWMWindow *notebook = gwmCreateNotebook(win, 2, 262, 300, 200, 0);
	GWMWindow *tab1 = gwmNotebookAdd(notebook, "Tab 1");
	GWMWindow *tab2 = gwmNotebookAdd(notebook, "Filesystem");
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
	
	gwmCreateTreeView(tab2, 2, 2, 250, 150, GWM_TREE_FILESYSTEM, GWM_FS_ROOT, 0);
	
	gwmPostDirty(tab1);
	redraw();

	gwmSetEventHandler(win, myEventHandler);
	gwmSetWindowFlags(win, GWM_WINDOW_MKFOCUSED | GWM_WINDOW_RESIZEABLE);
	gwmMainLoop();
	gwmQuit();
	return 0;
};
