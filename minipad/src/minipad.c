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

#define	DEFAULT_WIDTH			500
#define	DEFAULT_HEIGHT			500

GWMWindow *topWindow;
GWMWindow *textArea;
GWMWindow *menubar;

char *openedFile = NULL;
int fileDirty = 0;

int doFileChoice()
{
	GWMWindow *fc = gwmCreateFileChooser(topWindow, "Save as...", GWM_FILE_SAVE);
	char *path = gwmRunFileChooser(fc);
	
	if (path != NULL)
	{
		free(openedFile);
		openedFile = path;
		// TODO: update window caption
		return 0;
	};
	
	return -1;
};

int doFileSave()
{
	FILE *fp = fopen(openedFile, "w");
	if (fp == NULL)
	{
		char *msg;
		asprintf(&msg, "Could not write to %s: %s", openedFile, strerror(errno));
		gwmMessageBox(topWindow, "Error", msg, GWM_MBICON_ERROR | GWM_MBUT_OK);
		free(msg);
		return -1;
	}
	else
	{
		size_t len = gwmGetTextAreaLen(textArea);
		char *buffer = (char*) malloc(len+1);
		gwmReadTextArea(textArea, 0, len, buffer);
		
		if (buffer[len-1] == '\n')
		{
			fprintf(fp, "%s", buffer);
		}
		else
		{
			fprintf(fp, "%s\n", buffer);
		};
		
		fclose(fp);
		free(buffer);
		fileDirty = 0;
		return 0;
	};
};

int onExit(void *ignore)
{
	if (fileDirty)
	{
		int resp = gwmMessageBox(topWindow, "Minipad",
				"You have made changes to this file. Would you like to save before exiting?",
				GWM_MBICON_WARN | GWM_MBUT_YESNOCANCEL);
		switch (resp)
		{
		case 0:		/* yes */
			if (openedFile == NULL)
			{
				if (doFileChoice() != 0) return 0;
			};
			if (doFileSave() == 0) return -1;
			return 0;
		case 1:		/* no */
			return -1;
		case 2:		/* cancel */
			return 0;
		};
	};
	
	return -1;
};

int eventHandler(GWMEvent *ev, GWMWindow *win)
{
	switch (ev->type)
	{
	case GWM_EVENT_CLOSE:
		return onExit(NULL);
	default:
		return gwmDefaultHandler(ev, win);
	};
};

int onFileNew(void *ignore)
{
	pid_t pid = fork();
	if (pid == 0)
	{
		execl("/usr/bin/grexec", "grexec", "minipad", NULL);
		_exit(1);
	}
	else
	{
		waitpid(pid, NULL, 0);
	};
	
	return 0;
};

int onFileOpen(void *ignore)
{
	GWMWindow *fc = gwmCreateFileChooser(topWindow, "Open...", GWM_FILE_OPEN);
	char *path = gwmRunFileChooser(fc);
	
	if (path != NULL)
	{
		pid_t pid = fork();
		if (pid == 0)
		{
			execl("/usr/bin/grexec", "grexec", "minipad", path, NULL);
			_exit(1);
		}
		else
		{
			waitpid(pid, NULL, 0);
		};
		
		free(path);
	};
	
	return 0;
};

int onFileSave(void *ignore)
{
	if (openedFile == NULL)
	{
		if (doFileChoice() != 0) return 0;
	};
	
	doFileSave();
	return 0;
};

int onFileSaveAs(void *ignore)
{
	if (doFileChoice() == 0) doFileSave();
	return 0;
};

void onTextUpdate(void *ignore)
{
	fileDirty = 1;
};

int main(int argc, char *argv[])
{
	if (argc > 1)
	{
		openedFile = strdup(argv[1]);
	};
	
	if (gwmInit() != 0)
	{
		fprintf(stderr, "Failed to initialize GWM!\n");
		return 1;
	};
	
	char *caption;
	char *baseName;
	if (openedFile == NULL)
	{
		baseName = "Untitled";
	}
	else if (strrchr(openedFile, '/') == NULL)
	{
		baseName = openedFile;
	}
	else
	{
		baseName = strrchr(openedFile, '/') + 1;
	};
	
	asprintf(&caption, "%s - Minipad", baseName);
	topWindow = gwmCreateWindow(NULL, caption, GWM_POS_UNSPEC, GWM_POS_UNSPEC,
						DEFAULT_WIDTH, DEFAULT_HEIGHT, GWM_WINDOW_NOTASKBAR | GWM_WINDOW_HIDDEN);
	DDISurface *canvas = gwmGetWindowCanvas(topWindow);
	DDISurface *icon = ddiLoadAndConvertPNG(&canvas->format, "/usr/share/images/minipad.png", NULL);
	if (icon != NULL)
	{
		gwmSetWindowIcon(topWindow, icon);
		ddiDeleteSurface(icon);
	};
	
	DDIFont *font = ddiLoadFont("DejaVu Sans Mono", 14, 0, NULL);
	textArea = gwmCreateTextArea(topWindow, 0, 20, DEFAULT_WIDTH, DEFAULT_HEIGHT-20, 0);
	if (font == NULL)
	{
		fprintf(stderr, "Failed to load font!\n");
	}
	else
	{
		gwmSetTextAreaStyle(textArea, font, NULL, NULL, NULL);
	};
	
	gwmSetTextAreaUpdateCallback(textArea, onTextUpdate, NULL);
	
	if (openedFile != NULL)
	{
		char linebuf[1024];
		FILE *fp = fopen(openedFile, "r");
		if (fp == NULL)
		{
			char *msg;
			asprintf(&msg, "Could not read %s: %s", openedFile, strerror(errno));
			
			gwmMessageBox(NULL, "Minipad", msg, GWM_MBICON_ERROR | GWM_MBUT_OK);
			free(msg);
			gwmQuit();
			return 1;
		};
		
		while (fgets(linebuf, 1024, fp) != NULL)
		{
			gwmAppendTextArea(textArea, linebuf);
		};
		
		fclose(fp);
	};
	
	menubar = gwmCreateMenubar(topWindow);
	
	GWMMenu *menuFile = gwmCreateMenu();
	gwmMenuAddEntry(menuFile, "New", onFileNew, NULL);
	gwmMenuAddSeparator(menuFile);
	gwmMenuAddEntry(menuFile, "Open", onFileOpen, NULL);
	gwmMenuAddSeparator(menuFile);
	gwmMenuAddEntry(menuFile, "Save", onFileSave, NULL);
	gwmMenuAddEntry(menuFile, "Save as...", onFileSaveAs, NULL);
	gwmMenuAddSeparator(menuFile);
	gwmMenuAddEntry(menuFile, "Exit", onExit, NULL);
	
	gwmMenubarAdd(menubar, "File", menuFile);
	gwmMenubarAdjust(menubar);
	
	gwmSetWindowFlags(topWindow, 0);
	gwmSetWindowFlags(textArea, GWM_WINDOW_MKFOCUSED);
	gwmSetEventHandler(topWindow, eventHandler);
	gwmMainLoop();
	gwmQuit();
	return 0;
};
