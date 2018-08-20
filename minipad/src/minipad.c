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

#define	DEFAULT_WIDTH			500
#define	DEFAULT_HEIGHT			500

GWMWindow *topWindow;
GWMTextField *txtEditor;
char *filePath = NULL;
int fileDirty = 0;

void setCaption()
{
	const char *base;
	if (filePath == NULL)
	{
		base = "Untitled";
	}
	else
	{
		char *slashPos = strrchr(filePath, '/');
		if (slashPos == NULL)
		{
			base = filePath;
		}
		else
		{
			base = slashPos + 1;
		};
	};
	
	const char *s = "";
	if (fileDirty) s = "*";
	
	char *caption;
	asprintf(&caption, "%s%s - Minipad%s", s, base, s);
	gwmSetWindowCaption(topWindow, caption);
	free(caption);
};

int trySave(int forceChoice)
{
	if (filePath == NULL || forceChoice)
	{
		GWMFileChooser *fc = gwmCreateFileChooser(topWindow, "Select location to save file", GWM_FILE_SAVE);
		char *newPath = gwmRunFileChooser(fc);
		
		if (newPath == NULL) return -1;
		
		free(filePath);
		filePath = newPath;
	};
	
	int fd = open(filePath, O_WRONLY | O_CREAT | O_TRUNC, 0644);
	if (fd == -1)
	{
		char *msg;
		asprintf(&msg, "Cannot write to %s: %s", filePath, strerror(errno));
		gwmMessageBox(topWindow, "Minipad", msg, GWM_MBICON_ERROR | GWM_MBUT_OK);
		free(msg);
		return -1;
	};
	
	const char *text = gwmReadTextField(txtEditor);
	size_t size = strlen(text);
	
	write(fd, text, size);
	if (size == 0 || text[size-1] != '\n') write(fd, "\n", 1);
	close(fd);
	
	fileDirty = 0;
	setCaption();
	return 0;
};

int minipadCommand(GWMCommandEvent *ev)
{
	switch (ev->symbol)
	{
	case GWM_SYM_NEW_FILE:
		if (fork() == 0)
		{
			execl("/proc/self/exe", "minipad", NULL);
			_exit(1);
		};
		return GWM_EVSTATUS_OK;
	case GWM_SYM_OPEN_FILE:
		{
			GWMFileChooser *fc = gwmCreateFileChooser(topWindow, "Select text file", GWM_FILE_OPEN);
			char *path = gwmRunFileChooser(fc);
			
			if (path != NULL)
			{
				if (fork() == 0)
				{
					execl("/proc/self/exe", "minipad", path, NULL);
					_exit(1);
				};
			};
			
			free(path);
		};
		return GWM_EVSTATUS_OK;
	case GWM_SYM_SAVE:
		trySave(0);
		return GWM_EVSTATUS_OK;
	case GWM_SYM_SAVE_AS:
		trySave(1);
		return GWM_EVSTATUS_OK;
	default:
		return GWM_EVSTATUS_CONT;
	};
};

int minipadHandler(GWMEvent *ev, GWMWindow *win, void *context)
{
	switch (ev->type)
	{
	case GWM_EVENT_CLOSE:
		if (fileDirty)
		{
			int answer = gwmMessageBox(topWindow, "Minipad",
						"You have made changes to this file. Do you want to save before exiting?",
						GWM_MBICON_WARN | GWM_MBUT_YESNOCANCEL);
			if (answer == GWM_SYM_CANCEL)
			{
				return GWM_EVSTATUS_OK;
			}
			else if (answer == GWM_SYM_NO)
			{
				return GWM_EVSTATUS_BREAK;
			}
			else
			{
				if (trySave(0) == -1) return GWM_EVSTATUS_OK;
				else return GWM_EVSTATUS_BREAK;
			};
		};
		return GWM_EVSTATUS_BREAK;
	case GWM_EVENT_COMMAND:
		return minipadCommand((GWMCommandEvent*) ev);
	case GWM_EVENT_VALUE_CHANGED:
		if (!fileDirty)
		{
			fileDirty = 1;
			setCaption();
		};
		return GWM_EVSTATUS_OK;
	default:
		return GWM_EVSTATUS_CONT;
	};
};

int main(int argc, char *argv[])
{
	if (gwmInit() != 0)
	{
		fprintf(stderr, "Failed to initialize GWM!\n");
		return 1;
	};
	
	topWindow = gwmCreateWindow(NULL, "Minipad", GWM_POS_UNSPEC, GWM_POS_UNSPEC, 0, 0,
					GWM_WINDOW_HIDDEN | GWM_WINDOW_NOTASKBAR);

	DDISurface *surface = gwmGetWindowCanvas(topWindow);
	
	const char *error;
	DDISurface *icon = ddiLoadAndConvertPNG(&surface->format, "/usr/share/images/minipad.png", &error);
	if (icon == NULL)
	{
		printf("Failed to load minipad icon: %s\n", error);
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
	
	GWMMenu *menuFile = gwmCreateMenu();
	gwmMenubarAdd(menubar, "File", menuFile);
	
	gwmMenuAddCommand(menuFile, GWM_SYM_NEW_FILE, NULL, NULL);
	gwmMenuAddCommand(menuFile, GWM_SYM_OPEN_FILE, NULL, NULL);
	gwmMenuAddSeparator(menuFile);
	gwmMenuAddCommand(menuFile, GWM_SYM_SAVE, NULL, NULL);
	gwmMenuAddCommand(menuFile, GWM_SYM_SAVE_AS, NULL, NULL);
	gwmMenuAddSeparator(menuFile);
	gwmMenuAddCommand(menuFile, GWM_SYM_EXIT, NULL, NULL);
	
	txtEditor = gwmNewTextField(topWindow);
	gwmBoxLayoutAddWindow(boxLayout, txtEditor, 1, 0, GWM_BOX_FILL);
	gwmSetTextFieldFlags(txtEditor, GWM_TXT_MULTILINE);
	
	DDIFont *font = ddiLoadFont("DejaVu Sans Mono", 14, 0, NULL);
	if (font == NULL)
	{
		fprintf(stderr, "Failed to load font!\n");
	}
	else
	{
		gwmSetTextFieldFont(txtEditor, font);
	};
	
	if (argc >= 2)
	{
		filePath = realpath(argv[1], NULL);
		if (filePath == NULL)
		{
			gwmMessageBox(NULL, "Minipad", "Could not resolve the specified path", GWM_MBICON_ERROR | GWM_MBUT_OK);
			return 1;
		};
		
		int fd = open(filePath, O_RDONLY);
		if (fd == -1)
		{
			char *msg;
			asprintf(&msg, "Could not open %s: %s", filePath, strerror(errno));
			gwmMessageBox(NULL, "Minipad", msg, GWM_MBICON_ERROR | GWM_MBUT_OK);
			return 1;
		};
		
		struct stat st;
		if (fstat(fd, &st) != 0)
		{
			char *msg;
			asprintf(&msg, "Could not stat file: %s", strerror(errno));
			gwmMessageBox(NULL, "Minipad", msg, GWM_MBICON_ERROR | GWM_MBUT_OK);
			return 1;
		};
		
		char *buffer = (char*) malloc(st.st_size+1);
		read(fd, buffer, st.st_size);
		buffer[st.st_size] = 0;
		close(fd);
		
		gwmWriteTextField(txtEditor, buffer);
	};
	
	setCaption();
	gwmLayout(topWindow, DEFAULT_WIDTH, DEFAULT_HEIGHT);
	gwmPushEventHandler(topWindow, minipadHandler, NULL);
	gwmSetWindowFlags(topWindow, GWM_WINDOW_MKFOCUSED | GWM_WINDOW_RESIZEABLE);
	gwmMainLoop();
	gwmQuit();
	return 0;
};
