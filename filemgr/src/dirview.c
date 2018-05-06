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

#include <fstools.h>
#include <assert.h>
#include <sys/stat.h>

#include "dirview.h"
#include "filemgr.h"

extern GWMWindow *topWindow;

static int dvHandler(GWMEvent *ev, DirView *dv, void *context);

static void dvRedraw(DirView *dv)
{
	DirViewData *data = (DirViewData*) gwmGetData(dv, dvHandler);
	DDISurface *canvas = gwmGetWindowCanvas(dv);
	
	int entsPerLine = canvas->width/100;
	if (entsPerLine == 0) return;

	ddiFillRect(canvas, 0, 0, canvas->width, canvas->height, GWM_COLOR_EDITOR);
	
	int entIndex = 0;
	int totalHeight = 0;
	
	DirEntry *ent;
	for (ent=data->ents; ent!=NULL; ent=ent->next)
	{
		int y = (entIndex / entsPerLine) * 100 - data->scroll;
		int x = (entIndex % entsPerLine) * 100;
		
		ent->baseX = x;
		ent->baseY = y;
		
		if (ent->selected)
		{
			ddiFillRect(canvas, x, y, 100, 100, GWM_COLOR_SELECTION);
		};
		
		ddiBlit(ent->icon, 0, 0, canvas, x+18, y+2, 64, 64);
		totalHeight = y + 100 + data->scroll;
		 
		DDIPen *pen = ddiCreatePen(&canvas->format, gwmGetDefaultFont(), x+2, y+68, 98, 100-68-2, 0, 0, NULL);
		assert(pen != NULL);
		ddiSetPenAlignment(pen, DDI_ALIGN_CENTER);
		ddiWritePen(pen, ent->name);
		ddiExecutePen(pen, canvas);
		ddiDeletePen(pen);
		
		entIndex++;
	};
	
	data->totalHeight = totalHeight;
	if (data->sbar != NULL)
	{
		if (totalHeight <= canvas->height)
		{
			gwmSetScrollbarFlags(data->sbar, GWM_SCROLLBAR_DISABLED);
		}
		else
		{
			gwmSetScrollbarFlags(data->sbar, GWM_SCROLLBAR_VERT);
			gwmSetScrollbarLength(data->sbar, (float) canvas->height / (float) totalHeight);
		};
	};
	
	gwmPostDirty(dv);
};

static int dvScrollHandler(GWMEvent *ev, GWMScrollbar *sbar, void *context)
{
	DirView *dv = (DirView*) context;
	DirViewData *data = (DirViewData*) gwmGetData(dv, dvHandler);
	
	switch (ev->type)
	{
	case GWM_EVENT_VALUE_CHANGED:
		data->scroll = data->totalHeight * gwmGetScaleValue(sbar);
		dvRedraw(dv);
		// no return
	default:
		return GWM_EVSTATUS_CONT;
	};
};

char** getOpenArgs(const char *mimename)
{
	char specpath[256];
	sprintf(specpath, "/etc/filemgr/openwith/%s", mimename);
	
	FILE *fp = fopen(specpath, "r");
	if (fp == NULL) return NULL;
	
	char linebuf[1024];
	if (fgets(linebuf, 1024, fp) == NULL)
	{
		fclose(fp);
		return NULL;
	};
	
	fclose(fp);
	char *newline = strchr(linebuf, '\n');
	if (newline != NULL) *newline = 0;
	
	char *saveptr;
	
	char **argv = (char**) malloc(sizeof(char*));
	argv[0] = strdup("grexec");
	size_t argc = 1;
	
	char *param;
	for (param=strtok_r(linebuf, " \t", &saveptr); param!=NULL; param=strtok_r(NULL, " \t", &saveptr))
	{
		size_t index = argc++;
		argv = realloc(argv, sizeof(char*) * argc);
		argv[index] = strdup(param);
	};
	
	argv = realloc(argv, sizeof(char*) * (argc+1));
	argv[argc] = NULL;
	
	return argv;
};

// returns 0 if we should continue opening other selected files, else -1
static int doOpen(DirView *dv, DirEntry *ent)
{
	if (strcmp(ent->mime->mimename, "inode/directory") == 0)
	{
		dvGoTo(dv, ent->path);
		return -1;
	}
	else
	{
		char **argv = getOpenArgs(ent->mime->mimename);
		if (argv != NULL)
		{
			pid_t pid = fork();
			if (pid == -1)
			{
				char errbuf[2048];
				sprintf(errbuf, "Failed to start a new process: %s", strerror(errno));
				gwmMessageBox(topWindow, ent->name, errbuf, GWM_MBUT_OK | GWM_MBICON_ERROR);
				return -1;
			}
			else if (pid == 0)
			{
				chdir(dvGetLocation(dv));
				
				char **scan;
				for (scan=argv; *scan!=NULL; scan++)
				{
					if (strcmp(*scan, "%F") == 0 || strcmp(*scan, "%f") == 0)
					{
						*scan = ent->path;
					};
				};
				
				execv("/usr/bin/grexec", argv);
				_exit(1);
			};
		}
		else
		{
			char errbuf[2048];
			sprintf(errbuf, "There is no known application that can open files of type '%s' (%s)",
					ent->mime->label, ent->mime->mimename);
			gwmMessageBox(topWindow, ent->name, errbuf, GWM_MBUT_OK | GWM_MBICON_ERROR);
			return -1;
		};
		
		return 0;
	};
};

static void dvOpen(DirView *dv)
{
	DirViewData *data = (DirViewData*) gwmGetData(dv, dvHandler);
	
	DirEntry *ent;
	for (ent=data->ents; ent!=NULL; ent=ent->next)
	{
		if (ent->selected)
		{
			if (doOpen(dv, ent) != 0) break;
		};
	};
};

static void doRename(DirView *dv, DirViewData *data)
{
	char *dir = strdup(data->editing->path);
	strrchr(dir, '/')[1] = 0;
	
	if (chdir(dir) != 0)
	{
		gwmMessageBox(topWindow, "Rename", "Failed to switch directory", GWM_MBUT_OK | GWM_MBICON_ERROR);
	}
	else
	{
		const char *newname = gwmReadTextField(data->txtEdit);
		if (strchr(newname, '/') != NULL)
		{
			gwmMessageBox(topWindow, "Rename", "A file name is not allwoed to contain slashes (/)",
					GWM_MBUT_OK | GWM_MBICON_ERROR);
		}
		else
		{
			if (rename(data->editing->name, newname) != 0)
			{
				char errbuf[2048];
				sprintf(errbuf, "Failed to rename file: %s", strerror(errno));
				gwmMessageBox(topWindow, "Rename", errbuf, GWM_MBUT_OK | GWM_MBICON_ERROR);
			}
			else
			{
				free(data->editing->name);
				data->editing->name = strdup(newname);
				free(data->editing->path);
				
				data->editing->path = (char*) malloc(strlen(dir) + strlen(newname) + 1);
				sprintf(data->editing->path, "%s%s", dir, newname);
			};
		};
	};
	
	free(dir);
	data->editing = NULL;
	gwmSetWindowFlags(dv, GWM_WINDOW_MKFOCUSED);
	gwmDestroyTextField(data->txtEdit);
	data->txtEdit = NULL;
	dvRedraw(dv);
};

static int dvHandler(GWMEvent *ev, DirView *dv, void *context)
{
	DirViewData *data = (DirViewData*) context;
	
	switch (ev->type)
	{
	case GWM_EVENT_DOWN:
		if (ev->keycode == GWM_KC_MOUSE_LEFT)
		{
			DDISurface *canvas = gwmGetWindowCanvas(dv);
			int entsPerLine = canvas->width/100;
			if (entsPerLine == 0) return GWM_EVSTATUS_CONT;
			
			int tileX = ev->x / 100;
			int tileY = (ev->y + data->scroll) / 100;
			int clickIndex = entsPerLine * tileY + tileX;
			
			int entIndex = 0;
			DirEntry *ent;
			for (ent=data->ents; ent!=NULL; ent=ent->next)
			{
				if (entIndex == clickIndex)
				{
					if (ev->keymod & GWM_KM_CTRL)
					{
						ent->selected = !ent->selected;
					}
					else
					{
						ent->selected = 1;
					};
				}
				else
				{
					if ((ev->keymod & GWM_KM_CTRL) == 0)
					{
						ent->selected = 0;
					};
				};
				
				entIndex++;
			};
			
			dvRedraw(dv);
		}
		else if (ev->keycode == GWM_KC_RETURN)
		{
			dvOpen(dv);
		};
		return GWM_EVSTATUS_CONT;
	case GWM_EVENT_DOUBLECLICK:
		dvOpen(dv);
		return GWM_EVSTATUS_OK;
	case GWM_EVENT_RETHEME:
		dvRedraw(dv);
		return GWM_EVSTATUS_CONT;
	case GWM_EVENT_EDIT_END:
	case GWM_EVENT_COMMAND:			/* GWM_SYM_OK when user presses ENTER in name editor */
		doRename(dv, data);
		return GWM_EVSTATUS_OK;
	default:
		return GWM_EVSTATUS_CONT;
	};
};

static void dvMinSize(DirView *dv, int *width, int *height)
{
	*width = 200;
	*height = 200;
};

static void dvPrefSize(DirView *dv, int *width, int *height)
{
	*width = 640;
	*height = 480;
};

static void dvPosition(DirView *dv, int x, int y, int width, int height)
{
	gwmMoveWindow(dv, x, y);
	gwmResizeWindow(dv, width, height);
	dvRedraw(dv);
};

DirView* dvNew(GWMWindow *parent)
{
	DirView *dv = gwmCreateWindow(parent, "DirView", 0, 0, 0, 0, 0);
	
	DirViewData *data = (DirViewData*) malloc(sizeof(DirViewData));
	data->ents = NULL;
	data->location = NULL;
	data->sbar = NULL;
	data->editing = NULL;
	data->txtEdit = NULL;
	
	dv->getMinSize = dvMinSize;
	dv->getPrefSize = dvPrefSize;
	dv->position = dvPosition;

	gwmPushEventHandler(dv, dvHandler, data);
	dvRedraw(dv);
	return dv;
};

static int listDir(const char *path, DirEntry **out)
{
	DIR *dirp = opendir(path);
	if (dirp == NULL)
	{
		char errbuf[2048];
		sprintf(errbuf, "Cannot open %s: %s", path, strerror(errno));
		gwmMessageBox(NULL, "File manager", errbuf, GWM_MBICON_ERROR | GWM_MBUT_OK);
		return -1;
	};
	
	DirEntry *first = NULL;
	DirEntry *last = NULL;
	
	struct dirent *ent;
	while ((ent = readdir(dirp)) != NULL)
	{
		if (strcmp(ent->d_name, ".") == 0 || strcmp(ent->d_name, "..") == 0) continue;
		
		char *fullpath = (char*) malloc(strlen(path) + strlen(ent->d_name) + 2);
		sprintf(fullpath, "%s/%s", path, ent->d_name);
		
		DirEntry *ment = (DirEntry*) malloc(sizeof(DirEntry));
		memset(ment, 0, sizeof(DirEntry));
		ment->next = NULL;
		
		if (last == NULL)
		{
			ment->prev = NULL;
			first = last = ment;
		}
		else
		{
			ment->prev = last;
			last->next = ment;
			last = ment;
		};
		
		FSMimeType *mime = fsGetType(fullpath);
		ment->icon = gwmGetFileIcon(mime->iconName, GWM_FICON_LARGE);
		ment->mime = mime;
		ment->name = strdup(ent->d_name);
		ment->path = fullpath;
		ment->selected = 0;
	};
	
	closedir(dirp);
	*out = first;
	return 0;
};

int dvGoTo(DirView *dv, const char *path)
{
	DirViewData *data = (DirViewData*) gwmGetData(dv, dvHandler);
	
	char *rpath = realpath(path, NULL);
	if (rpath == NULL)
	{
		char errbuf[2048];
		sprintf(errbuf, "Cannot resolve %s: %s", path, strerror(errno));
		return -1;
	};
	
	DirEntry *newlist;
	if (listDir(rpath, &newlist) != 0)
	{
		free(rpath);
		return -1;
	};
	
	free(data->location);
	data->location = rpath;
	
	while (data->ents != NULL)
	{
		DirEntry *ent = data->ents;
		data->ents = ent->next;
		
		free(ent->name);
		free(ent->path);
		free(ent);
	};
	
	data->ents = newlist;
	
	char *slashPos = strrchr(rpath, '/');
	if (slashPos == NULL)
	{
		gwmSetWindowCaption(topWindow, "/ - File manager");
	}
	else
	{
		char *caption = (char*) malloc(strlen(slashPos+1) + strlen(" - File manager") + 1);
		sprintf(caption, "%s - File manager", slashPos+1);
		gwmSetWindowCaption(topWindow, caption);
		free(caption);
	};
	
	GWMEvent ev;
	memset(&ev, 0, sizeof(GWMEvent));
	ev.type = DV_EVENT_CHDIR;
	gwmPostEvent(&ev, dv);
	
	if (data->sbar != NULL) gwmSetScaleValue(data->sbar, 0.0);
	
	dvRedraw(dv);
	return 0;
};

const char* dvGetLocation(DirView *dv)
{
	DirViewData *data = (DirViewData*) gwmGetData(dv, dvHandler);
	return data->location;
};

void dvAttachScrollbar(DirView *dv, GWMScrollbar *sbar)
{
	DirViewData *data = (DirViewData*) gwmGetData(dv, dvHandler);
	data->sbar = sbar;
	gwmPushEventHandler(sbar, dvScrollHandler, dv);
	dvRedraw(dv);
};

void dvRename(DirView *dv)
{
	DirViewData *data = (DirViewData*) gwmGetData(dv, dvHandler);
	
	// find a selected DirEntry
	DirEntry *ent;
	for (ent=data->ents; ent!=NULL; ent=ent->next)
	{
		if (ent->selected) break;
	};
	
	if (ent != NULL)
	{
		// OK, we found an entry to edit
		data->editing = ent;
		
		GWMTextField *edit = gwmNewTextField(dv);
		edit->position(edit, ent->baseX, ent->baseY+66, 100, 100-66);
		gwmSetTextFieldWrap(edit, 1);
		gwmSetTextFieldAlignment(edit, DDI_ALIGN_CENTER);
		gwmSetWindowFlags(edit, GWM_WINDOW_MKFOCUSED);
		gwmWriteTextField(edit, ent->name);
		gwmTextFieldSelectAll(edit);
		
		data->txtEdit = edit;
	};
};

void dvMakeDir(DirView *dv)
{
	DirViewData *data = (DirViewData*) gwmGetData(dv, dvHandler);
	
	// unselect all entries
	DirEntry *ent;
	for (ent=data->ents; ent!=NULL; ent=ent->next)
	{
		ent->selected = 0;
	};
	
	// change to the containing directory
	if (chdir(data->location) != 0)
	{
		gwmMessageBox(topWindow, "New directory", "Failed to change directory", GWM_MBUT_OK | GWM_MBICON_ERROR);
		return;
	};
	
	// choose a suitable name
	int i;
	char namebuf[128];
	for (i=0;;i++)
	{
		if (i == 0) strcpy(namebuf, "New directory");
		else sprintf(namebuf, "New directory (%d)", i);
		
		if (mkdir(namebuf, 0755) == 0)
		{
			break;
		};
		
		if (errno != EEXIST)
		{
			char errbuf[2048];
			sprintf(errbuf, "Cannot create directory: %s", strerror(errno));
			gwmMessageBox(topWindow, "New directory", errbuf, GWM_MBUT_OK | GWM_MBICON_ERROR);
			return;
		};
	};
	
	// create an entry and select it
	ent = (DirEntry*) malloc(sizeof(DirEntry));
	memset(ent, 0, sizeof(DirEntry));
	
	ent->mime = fsGetType(namebuf);
	ent->icon = gwmGetFileIcon(ent->mime->iconName, GWM_FICON_LARGE);
	ent->name = strdup(namebuf);
	ent->path = realpath(namebuf, NULL);
	ent->selected = 1;
	
	// append the entry
	if (data->ents == NULL) data->ents = ent;
	else
	{
		DirEntry *last = data->ents;
		while (last->next != NULL) last = last->next;
		last->next = ent;
		ent->prev = last;
	};
	
	// redraw the directory view so that the tile coordinates are found
	dvRedraw(dv);
	
	// now rename it
	dvRename(dv);
};
