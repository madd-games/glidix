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

#include <sys/wait.h>
#include <sys/stat.h>
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
#include <fcntl.h>

int previewSizes[] = {
	8, 14, 20, 25, 30, 35, 40
};

int fontviewCommand(GWMCommandEvent *ev, GWMWindow *win)
{
	switch (ev->symbol)
	{
	case GWM_SYM_ABOUT:
		{
			GWMAboutDialog *about = gwmNewAboutDialog(win);
			gwmSetAboutIcon(about, gwmGetFileIcon("font", GWM_FICON_LARGE), 0, 0, 64, 64);
			gwmSetAboutCaption(about, "Glidix Font Viewer");
			gwmSetAboutDesc(about, "Allows for previewing of font files.");
			gwmSetAboutLicense(about, GWM_LICENSE);
			gwmRunAbout(about);
		};
		return GWM_EVSTATUS_OK;
	default:
		return GWM_EVSTATUS_CONT;
	};
};

int fontviewHandler(GWMEvent *ev, GWMWindow *win, void *context)
{
	switch (ev->type)
	{
	case GWM_EVENT_COMMAND:
		return fontviewCommand((GWMCommandEvent*) ev, win);
	default:
		return GWM_EVSTATUS_CONT;
	};
};

int main(int argc, char *argv[])
{
	if (gwmInit() != 0)
	{
		fprintf(stderr, "Failed to initialize GWM\n");
		return 1;
	};
	
	const char *filename;
	if (argc >= 2)
	{
		filename = argv[1];
	}
	else
	{
		GWMFileChooser *fc = gwmCreateFileChooser(NULL, "Select a font file", GWM_FILE_OPEN);
		gwmAddFileChooserFilter(fc, "TrueType fonts (*.ttf)", "*.ttf", ".ttf");
		filename = gwmRunFileChooser(fc);
		if (filename == NULL) return 1;
	};
	
	int numPreviews = sizeof(previewSizes)/sizeof(int);
	DDIFont* fonts[numPreviews];
	
	int i;
	for (i=0; i<numPreviews; i++)
	{
		const char *error;
		DDIFont *font = ddiOpenFont(filename, previewSizes[i], &error);
		
		if (font == NULL)
		{
			char *errmsg;
			asprintf(&errmsg, "Failed to load %s (size %d): %s", filename, previewSizes[i], error);
			gwmMessageBox(NULL, "Font viewer", errmsg, GWM_MBICON_ERROR | GWM_MBUT_OK);
			return 1;
		};
		
		fonts[i] = font;
	};
	
	GWMWindow *topWindow = gwmCreateWindow(NULL, "Font viewer", GWM_POS_UNSPEC, GWM_POS_UNSPEC, 0, 0,
					GWM_WINDOW_HIDDEN | GWM_WINDOW_NOTASKBAR);

	DDISurface *surface = gwmGetWindowCanvas(topWindow);
	
	const char *error;
	DDISurface *icon = ddiLoadAndConvertPNG(&surface->format, "/usr/share/images/fontview.png", &error);
	if (icon == NULL)
	{
		printf("Failed to load fontview icon: %s\n", error);
	}
	else
	{
		gwmSetWindowIcon(topWindow, icon);
	};

	GWMLayout *boxLayout = gwmCreateBoxLayout(GWM_BOX_VERTICAL);
	
	GWMWindowTemplate wt;
	wt.wtComps = GWM_WTC_MENUBAR;
	wt.wtWindow = topWindow;
	wt.wtBody = boxLayout;
	gwmCreateTemplate(&wt);
	
	GWMWindow *menubar = wt.wtMenubar;
	
	GWMMenu *menuHelp = gwmCreateMenu();
	gwmMenubarAdd(menubar, "Help", menuHelp);
	
	gwmMenuAddCommand(menuHelp, GWM_SYM_ABOUT, NULL, NULL);
	
	for (i=0; i<numPreviews; i++)
	{
		GWMLabel *lbl = gwmNewLabel(topWindow);
		gwmBoxLayoutAddWindow(boxLayout, lbl, 0, 5, GWM_BOX_ALL | GWM_BOX_FILL);
		gwmSetLabelText(lbl, "The quick brown fox jumped over the lazy dog.");
		gwmSetLabelFont(lbl, fonts[i]);
	};
	
	gwmFit(topWindow);
	gwmPushEventHandler(topWindow, fontviewHandler, NULL);
	gwmSetWindowFlags(topWindow, GWM_WINDOW_MKFOCUSED | GWM_WINDOW_RESIZEABLE);
	gwmMainLoop();
	gwmQuit();
	return 0;
};
