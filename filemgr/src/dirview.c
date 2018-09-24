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
#include <sys/call.h>
#include <sys/storage.h>
#include <sys/fsinfo.h>
#include <fcntl.h>

#include "dirview.h"
#include "filemgr.h"
#include "props.h"
#include "task.h"

extern GWMWindow *topWindow;

DDISurface *iconUnmountHDD;
DDISurface *iconMountHDD;
DDISurface *iconUnmountCD;
DDISurface *iconMountCD;
DDISurface *iconSymlink;

static int dvHandler(GWMEvent *ev, DirView *dv, void *context);
int dvGoToEx(DirView *dv, const char *path, int stack);

static void dvRedraw(DirView *dv)
{
	DirViewData *data = (DirViewData*) gwmGetData(dv, dvHandler);
	DDISurface *canvas = gwmGetWindowCanvas(dv);
	
	int entsPerLine = canvas->width/100;
	if (entsPerLine == 0) return;

	static DDIColor transparent = {0, 0, 0, 0};
	if (desktopMode) ddiFillRect(canvas, 0, 0, canvas->width, canvas->height, &transparent);
	else ddiFillRect(canvas, 0, 0, canvas->width, canvas->height, GWM_COLOR_EDITOR);
	
	int entIndex = 0;
	int totalHeight = 0;
	
	DirEntry *ent;
	for (ent=data->ents; ent!=NULL; ent=ent->next)
	{
		int y = (entIndex / entsPerLine) * 100 - data->scroll;
		int x = (entIndex % entsPerLine) * 100;
		
		ent->baseX = x;
		ent->baseY = y;
		
		totalHeight = y + 100 + data->scroll;
		
		if (y > -100 && y < canvas->height)
		{
			if (ent->selected)
			{
				ddiFillRect(canvas, x, y, 100, 100, GWM_COLOR_SELECTION);
			};
		
			ddiBlit(ent->icon, 0, 0, canvas, x+18, y+2, 64, 64);
			if (ent->symlink)
			{
				ddiBlit(iconSymlink, 0, 0, canvas, x+18, y+50, 16, 16);
			};
			
			DDIPen *pen = ddiCreatePen(&canvas->format, gwmGetDefaultFont(), x+2, y+68, 98, 100-68-2, 0, 0, NULL);
			assert(pen != NULL);
			ddiSetPenAlignment(pen, DDI_ALIGN_CENTER);
			static DDIColor white = {0xFF, 0xFF, 0xFF, 0xFF};
			if (desktopMode) ddiSetPenColor(pen, &white);
			ddiWritePen(pen, ent->name);
			ddiExecutePen(pen, canvas);
			ddiDeletePen(pen);
		};
		
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
	if (ent->open != NULL)
	{
		return ent->open(dv, ent);
	}
	else if (strcmp(ent->mime->mimename, "inode/directory") == 0)
	{
		if (desktopMode)
		{
			if (fork() == 0)
			{
				chdir(ent->path);
				execl("/usr/bin/filemgr", "filemgr", NULL);
				_exit(1);
			};
		}
		else
		{
			dvGoTo(dv, ent->path);
		};
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
			if (__syscall(__SYS_mv, AT_FDCWD, data->editing->name, AT_FDCWD, newname, __MV_EXCL) != 0)
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

static int dvCommandHandler(GWMCommandEvent *ev, DirView *dv, DirViewData *data)
{
	switch (ev->symbol)
	{
	case GWM_SYM_OK:
		doRename(dv, data);
		return GWM_EVSTATUS_OK;
	default:
		return GWM_EVSTATUS_CONT;
	};
};

static int dvHandler(GWMEvent *ev, DirView *dv, void *context)
{
	DirViewData *data = (DirViewData*) context;
	struct stat st;
	
	switch (ev->type)
	{
	case GWM_EVENT_RETHEME:
		dvRedraw(dv);
		return GWM_EVSTATUS_OK;
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
		else if (ev->keycode == GWM_KC_MOUSE_RIGHT)
		{
			gwmOpenMenu(menuEdit, dv, ev->x, ev->y);
		}
		else if (ev->keycode == GWM_KC_RETURN)
		{
			dvOpen(dv);
		};
		return GWM_EVSTATUS_CONT;
	case GWM_EVENT_DOUBLECLICK:
		dvOpen(dv);
		return GWM_EVSTATUS_OK;
	case GWM_EVENT_EDIT_END:
		doRename(dv, data);
		return GWM_EVSTATUS_OK;
	case GWM_EVENT_COMMAND:
		return dvCommandHandler((GWMCommandEvent*) ev, dv, data);
	case DV_EVENT_CHECK:
		if (stat(data->location, &st) == 0)
		{
			if (st.st_mtime > data->refreshTime)
			{
				dvGoTo(dv, data->location);
			};
		};
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
	DDIPixelFormat format;
	gwmGetScreenFormat(&format);
	
	iconUnmountHDD = ddiLoadAndConvertPNG(&format, "/usr/share/images/hdd-umnt.png", NULL);
	iconMountHDD = ddiLoadAndConvertPNG(&format, "/usr/share/images/hdd-mnt.png", NULL);
	iconUnmountCD = ddiLoadAndConvertPNG(&format, "/usr/share/images/cd-umnt.png", NULL);
	iconMountCD = ddiLoadAndConvertPNG(&format, "/usr/share/images/cd-mnt.png", NULL);
	iconSymlink = ddiLoadAndConvertPNG(&format, "/usr/share/images/symlink.png", NULL);
	
	assert(iconUnmountHDD != NULL);
	assert(iconMountHDD != NULL);
	assert(iconUnmountCD != NULL);
	assert(iconMountCD != NULL);
	assert(iconSymlink != NULL);
	
	DirView *dv = gwmCreateWindow(parent, "DirView", 0, 0, 0, 0, 0);
	
	DirViewData *data = (DirViewData*) malloc(sizeof(DirViewData));
	data->ents = NULL;
	data->location = NULL;
	data->sbar = NULL;
	data->editing = NULL;
	data->txtEdit = NULL;
	data->scroll = 0;
	data->back = NULL;
	data->forward = NULL;
	
	dv->getMinSize = dvMinSize;
	dv->getPrefSize = dvPrefSize;
	dv->position = dvPosition;

	gwmPushEventHandler(dv, dvHandler, data);
	dvRedraw(dv);
	
	gwmCreateTimerEx(dv, 500, DV_EVENT_CHECK);
	return dv;
};

static int isMounted(const char *diskpath)
{
	struct fsinfo info[256];
	size_t count = _glidix_fsinfo(info, 256);
	
	size_t i;
	for (i=0; i<count; i++)
	{
		if (strcmp(info[i].fs_image, diskpath) == 0) return 1;
	};
	
	return 0;
};

static int openDisk(DirView *dv, DirEntry *ent)
{
	char *dirpath = NULL;
	
	struct fsinfo info[256];
	size_t count = _glidix_fsinfo(info, 256);
	
	size_t i;
	for (i=0; i<count; i++)
	{
		if (strcmp(info[i].fs_image, ent->path) == 0)
		{
			dirpath = info[i].fs_mntpoint;
			break;
		};
	};
	
	if (dirpath == NULL)
	{
		char path[256];
		char error[1024];
		
		int outpipe[2];
		int errpipe[2];
		
		if (pipe(outpipe) != 0)
		{
			gwmMessageBox(topWindow, "File manager", "FATAL: failed to create output pipe", GWM_MBICON_ERROR | GWM_MBUT_OK);
			return -1;
		};
		
		if (pipe(errpipe) != 0)
		{
			gwmMessageBox(topWindow, "File manager", "FATAL: failed to create error pipe", GWM_MBICON_ERROR | GWM_MBUT_OK);
			return -1;
		};

		pid_t pid = fork();
		if (pid == -1)
		{
			gwmMessageBox(topWindow, "File manager", "fork() failed!", GWM_MBICON_ERROR | GWM_MBUT_OK);
			return -1;
		}
		else if (pid == 0)
		{
			close(outpipe[0]);
			close(errpipe[0]);
			
			close(1);
			close(2);
			
			dup(outpipe[1]);
			dup(errpipe[1]);
			
			close(outpipe[1]);
			close(errpipe[1]);
			
			execl("/usr/bin/mediamnt", "mediamnt", ent->path, NULL);
			perror("exec mediamnt");
			_exit(1);
		}
		else
		{
			close(outpipe[1]);
			close(errpipe[1]);
			
			char *put = path;
			while (1)
			{
				ssize_t sz = read(outpipe[0], put, 256);
				if (sz > 0)
				{
					put += sz;
				}
				else
				{
					*put = 0;
					break;
				};
			};
			
			memset(error, 0, 256);
			read(errpipe[0], error, 256);
			
			close(outpipe[0]);
			close(errpipe[0]);
			
			int status;
			waitpid(pid, &status, 0);
			
			if (!WIFEXITED(status) || WEXITSTATUS(status) != 0)
			{
				gwmMessageBox(topWindow, "File manager", error, GWM_MBICON_ERROR | GWM_MBUT_OK);
				return -1;
			};
			
			char *endline = strrchr(path, '\n');
			if (endline != NULL) *endline = 0;
			dirpath = path;
		};
	};
	
	dvGoTo(dv, dirpath);
	return -1;
};

static DirEntry* makeDiskEntry(const char *diskpath)
{
	DirEntry *ment = (DirEntry*) malloc(sizeof(DirEntry));
	memset(ment, 0, sizeof(DirEntry));
	ment->next = NULL;
	
	SDIdentity id;
	if (pathctl(diskpath, IOCTL_SDI_IDENTITY, &id) != 0)
	{
		id.flags = 0;
		strcpy(id.name, diskpath);
	};
	
	if (id.flags & SD_EJECTABLE)
	{
		if (isMounted(diskpath)) ment->icon = iconMountCD;
		else ment->icon = iconUnmountCD;
	}
	else
	{
		if (isMounted(diskpath)) ment->icon = iconMountHDD;
		else ment->icon = iconUnmountHDD;
	};
	
	FSMimeType *mime = fsGetType(diskpath);
	ment->mime = mime;
	ment->name = strdup(id.name);
	ment->path = strdup(diskpath);
	ment->selected = 0;
	ment->open = openDisk;
	
	return ment;
};

static int listComputer(DirEntry **out)
{
	DirEntry *first = NULL;
	DirEntry *last = NULL;
	
	DIR *dirp = opendir("/dev");
	if (dirp == NULL)
	{
		char errbuf[2048];
		sprintf(errbuf, "Cannot scan /dev: %s", strerror(errno));
		gwmMessageBox(NULL, "File manager", errbuf, GWM_MBICON_ERROR | GWM_MBUT_OK);
		return -1;
	};
	
	struct dirent *ent;
	while ((ent = readdir(dirp)) != NULL)
	{
		if (strlen(ent->d_name) == 3 && memcmp(ent->d_name, "sd", 2) == 0)
		{
			char firstPart[16];
			sprintf(firstPart, "/dev/%s0", ent->d_name);
			
			struct stat st;
			if (lstat(firstPart, &st) != 0)
			{
				char diskpath[16];
				sprintf(diskpath, "/dev/%s", ent->d_name);
				
				DirEntry *dent = makeDiskEntry(diskpath);
				if (last != NULL)
				{
					last->next = dent;
					last = dent;
				}
				else
				{
					first = last = dent;
				};
			}
			else
			{
				int i;
				for (i=0;;i++)
				{
					char diskpath[16];
					sprintf(diskpath, "/dev/%s%d", ent->d_name, i);
					
					if (lstat(diskpath, &st) != 0) break;
					
					DirEntry *dent = makeDiskEntry(diskpath);
					if (last != NULL)
					{
						last->next = dent;
						last = dent;
					}
					else
					{
						first = last = dent;
					};
				};
			};
		};
	};
	
	closedir(dirp);
	*out = first;
	return 0;
};

static int listDir(const char *path, DirEntry **out)
{
	if (strcmp(path, "://computer") == 0)
	{
		return listComputer(out);
	};
	
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
		
		struct stat st;
		lstat(fullpath, &st);
		
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
		ment->symlink = S_ISLNK(st.st_mode);
	};
	
	closedir(dirp);
	*out = first;
	return 0;
};

int dvGoTo(DirView *dv, const char *path)
{
	return dvGoToEx(dv, path, 1);
};

int dvGoToEx(DirView *dv, const char *path, int stack)
{
	DirViewData *data = (DirViewData*) gwmGetData(dv, dvHandler);
	
	char *rpath;
	if (memcmp(path, "://", 3) == 0)
	{
		rpath = strdup(path);
	}
	else
	{
		rpath = realpath(path, NULL);
	};
	
	if (rpath == NULL)
	{
		char errbuf[2048];
		sprintf(errbuf, "Cannot resolve %s: %s", path, strerror(errno));
		gwmMessageBox(topWindow, "File manager", errbuf, GWM_MBICON_ERROR | GWM_MBUT_OK);
		return -1;
	};
	
	DirEntry *newlist;
	if (listDir(rpath, &newlist) != 0)
	{
		free(rpath);
		return -1;
	};
	
	if (stack && data->location != NULL)
	{
		while (data->forward != NULL)
		{
			HistoryNode *node = data->forward;
			data->forward = node->down;
			
			free(node->location);
			free(node);
		};
		
		HistoryNode *node = (HistoryNode*) malloc(sizeof(HistoryNode));
		node->location = data->location;
		node->down = data->back;
		data->back = node;
	}
	else
	{
		free(data->location);
	};
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
	data->refreshTime = time(NULL);
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

void dvMakeFile(DirView *dv, const void *content, size_t size)
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
		gwmMessageBox(topWindow, "New file", "Failed to change directory", GWM_MBUT_OK | GWM_MBICON_ERROR);
		return;
	};
	
	// choose a suitable name
	int i;
	char namebuf[128];
	for (i=0;;i++)
	{
		if (i == 0) strcpy(namebuf, "New file");
		else sprintf(namebuf, "New file (%d)", i);
		
		int fd = open(namebuf, O_WRONLY | O_CREAT | O_EXCL, 0644);
		if (fd == -1)
		{
			if (errno != EEXIST)
			{
				char errbuf[2048];
				sprintf(errbuf, "Cannot create file: %s", strerror(errno));
				gwmMessageBox(topWindow, "New file", errbuf, GWM_MBUT_OK | GWM_MBICON_ERROR);
				return;
			};
		}
		else
		{
			if (size != 0)
			{
				write(fd, content, size);
			};

			close(fd);
			break;
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

void dvTerminal(DirView *dv)
{
	DirViewData *data = (DirViewData*) gwmGetData(dv, dvHandler);
	
	// change to the visible directory
	if (chdir(data->location) != 0)
	{
		gwmMessageBox(topWindow, "New file", "Failed to change directory", GWM_MBUT_OK | GWM_MBICON_ERROR);
		return;
	};
	
	pid_t pid = fork();
	if (pid == -1)
	{
		gwmMessageBox(topWindow, "Terminal", "Cannot start new process", GWM_MBUT_OK | GWM_MBICON_ERROR);
		return;
	}
	else if (pid == 0)
	{
		execl("/usr/bin/terminal", "terminal", NULL);
		_exit(1);
	};
};

void dvBack(DirView *dv)
{
	DirViewData *data = (DirViewData*) gwmGetData(dv, dvHandler);
	if (data->back == NULL) return;
	
	// take the node off
	HistoryNode *node = data->back;
	data->back = node->down;
	
	// move onto "forward" stack
	char *location = node->location;
	node->down = data->forward;
	node->location = strdup(data->location);
	data->forward = node;
	
	// move there
	dvGoToEx(dv, location, 0);
	free(location);
};

void dvForward(DirView *dv)
{
	DirViewData *data = (DirViewData*) gwmGetData(dv, dvHandler);
	if (data->forward == NULL) return;
	
	// take the node off
	HistoryNode *node = data->forward;
	data->forward = node->down;
	
	// move onto "back" stack
	char *location = node->location;
	node->down = data->back;
	node->location = strdup(data->location);
	data->back = node;
	
	// move there
	dvGoToEx(dv, location, 0);
	free(location);
};

void dvProps(DirView *dv)
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
		propShow(ent->path, ent->mime);
	}
	else
	{
		FSMimeType *mime = fsGetType(data->location);
		propShow(data->location, mime);
	};
};

static void doClipboardOp(DirView *dv, DirViewData *data, const char *op)
{
	char *cbspec = strdup(op);

	// find all selected entries and append them	
	DirEntry *ent;
	for (ent=data->ents; ent!=NULL; ent=ent->next)
	{
		if (ent->selected)
		{
			if (strchr(ent->path, '\n') != NULL)
			{
				gwmMessageBox(dv, "Error",
					"Some of the selected files contain invalid characters in their names.",
					GWM_MBICON_ERROR | GWM_MBUT_OK);
				free(cbspec);
				return;
			};
			
			char *newspec;
			asprintf(&newspec, "%s\n%s", cbspec, ent->path);
			free(cbspec);
			cbspec = newspec;
		};
	};
	
	// see if we actually have any files select
	if (strchr(cbspec, '\n') == NULL)
	{
		return;
	};
	
	// done
	gwmClipboardPutText(cbspec, strlen(cbspec));
	free(cbspec);
};

void dvCut(DirView *dv)
{
	DirViewData *data = (DirViewData*) gwmGetData(dv, dvHandler);
	doClipboardOp(dv, data, "*CUT*");
};

void dvCopy(DirView *dv)
{
	DirViewData *data = (DirViewData*) gwmGetData(dv, dvHandler);
	doClipboardOp(dv, data, "*COPY*");
};

static void addPathObjects(const char *destdir, int defaultOp, const char *path, FileOpObject **firstPtr, FileOpObject **lastPtr, int *numPtr, TaskData *data)
{
	if (path[0] != '/') return;

	struct stat st;
	if (lstat(path, &st) != 0)
	{
		return;
	};
	
	const char *basename = path;
	char *slashPos = strrchr(basename, '/');
	if (slashPos != NULL) basename = slashPos+1;

	if (S_ISDIR(st.st_mode))
	{
		FileOpObject *obj = (FileOpObject*) malloc(sizeof(FileOpObject));
		memset(obj, 0, sizeof(FileOpObject));
		obj->op = FILEOP_MKDIR;
		obj->src = strdup(path);
		asprintf(&obj->dest, "%s/%s", destdir, basename);
		obj->size = st.st_size;
		if (S_ISLNK(st.st_mode)) obj->size = 0;
		obj->mode = st.st_mode & 0xFFF;
		
		if (*firstPtr == NULL)
		{
			*firstPtr = *lastPtr = obj;
		}
		else
		{
			(*lastPtr)->next = obj;
			*lastPtr = obj;
		};
		
		(*numPtr)++;
		
		char txtbuf[256];
		sprintf(txtbuf, "Counting objects... %d", *numPtr);
		taskSetText(data, txtbuf);
		
		DIR *dirp = opendir(path);
		if (dirp == NULL) return;
		
		struct dirent *ent;
		while ((ent = readdir(dirp)) != NULL)
		{
			if (strcmp(ent->d_name, ".") == 0) continue;
			if (strcmp(ent->d_name, "..") == 0) continue;
			
			char *childpath;
			asprintf(&childpath, "%s/%s", path, ent->d_name);
			
			addPathObjects(obj->dest, defaultOp, childpath, firstPtr, lastPtr, numPtr, data);
			free(childpath);
		};
		
		closedir(dirp);
		
		if (defaultOp == FILEOP_MOVE)
		{
			FileOpObject *obj = (FileOpObject*) malloc(sizeof(FileOpObject));
			memset(obj, 0, sizeof(FileOpObject));
			obj->op = FILEOP_RMDIR;
			obj->src = strdup(path);
			obj->dest = strdup(path);
			obj->size = st.st_size;
			if (S_ISLNK(st.st_mode)) obj->size = 0;
			obj->mode = st.st_mode & 0xFFF;
			
			if (*firstPtr == NULL)
			{
				*firstPtr = *lastPtr = obj;
			}
			else
			{
				(*lastPtr)->next = obj;
				*lastPtr = obj;
			};
			
			(*numPtr)++;
			
			char txtbuf[256];
			sprintf(txtbuf, "Counting objects... %d", *numPtr);
			taskSetText(data, txtbuf);
		};
	}
	else if (S_ISLNK(st.st_mode) || S_ISREG(st.st_mode))
	{	
		FileOpObject *obj = (FileOpObject*) malloc(sizeof(FileOpObject));
		memset(obj, 0, sizeof(FileOpObject));
		obj->op = defaultOp;
		obj->src = strdup(path);
		asprintf(&obj->dest, "%s/%s", destdir, basename);
		obj->size = st.st_size;
		if (S_ISLNK(st.st_mode)) obj->size = 0;
		obj->mode = st.st_mode & 0xFFF;
		
		if (*firstPtr == NULL)
		{
			*firstPtr = *lastPtr = obj;
		}
		else
		{
			(*lastPtr)->next = obj;
			*lastPtr = obj;
		};
		
		(*numPtr)++;
		
		char txtbuf[256];
		sprintf(txtbuf, "Counting objects... %d", *numPtr);
		taskSetText(data, txtbuf);
	};
};

typedef struct
{
	const char*					destdir;
	char*						cbspec;
} PasteData;

static void updateProgress(TaskData *data, const char *op, int counter, int numObj, size_t dataDone, size_t dataTotal)
{
	char textbuf[256];
	sprintf(textbuf, "%s file %d/%d...", op, counter, numObj);
	
	taskSetText(data, textbuf);
	taskSetProgress(data, (float) dataDone / (float) dataTotal);
};

static int doCopy(TaskData *data, FileOpObject *obj, size_t *dataDone, size_t dataTotal, int objCounter, int numObj, const char *opname)
{
	struct stat st;
	if (lstat(obj->src, &st) == 0)
	{
		if (S_ISLNK(st.st_mode))
		{
			char *dest = realpath(obj->src, NULL);
			if (dest == NULL) dest = strdup("<dangling>");
			
			if (symlink(dest, obj->dest) != 0)
			{
				char *errmsg;
				asprintf(&errmsg, "Cannot create symbolic link %s: %s", obj->dest, strerror(errno));
				taskMessageBox(data, "Error", errmsg, GWM_MBICON_ERROR | GWM_MBUT_OK);
				free(errmsg);
				free(dest);
				return -1;
			};
			
			free(dest);
			return 0;
		};
	};
	
	int src = open(obj->src, O_RDONLY);
	if (src == -1)
	{
		char *errmsg;
		asprintf(&errmsg, "Cannot read %s: %s", obj->src, strerror(errno));
		taskMessageBox(data, "Error", errmsg, GWM_MBICON_ERROR | GWM_MBUT_OK);
		free(errmsg);
		return -1;
	};
	
	int dest = open(obj->dest, O_WRONLY | O_CREAT | O_EXCL, obj->mode);
	if (dest == -1)
	{
		if (errno == EEXIST)
		{
			char *questmsg;
			asprintf(&questmsg, "The file %s already exists. Would you like "
					"to replace it?", obj->dest);
			if (taskMessageBox(data, "File exists", questmsg,
				GWM_MBICON_WARN | GWM_MBUT_YESNO) == GWM_SYM_YES)
			{
				free(questmsg);
				dest = open(obj->dest, O_WRONLY | O_CREAT | O_TRUNC,
							obj->mode);
				if (dest == -1)
				{
					char *errmsg;
					asprintf(&errmsg, "Cannot write %s: %s", obj->dest, strerror(errno));
					taskMessageBox(data, "Error", errmsg, GWM_MBICON_ERROR | GWM_MBUT_OK);
					free(errmsg);
					return -1;
				};
			}
			else
			{
				free(questmsg);
				return 0;
			};
		}
		else
		{
			char *errmsg;
			asprintf(&errmsg, "Cannot write %s: %s", obj->dest, strerror(errno));
			taskMessageBox(data, "Error", errmsg, GWM_MBICON_ERROR | GWM_MBUT_OK);
			free(errmsg);
			return -1;
		};
	};
	
	char buffer[64*1024];
	ssize_t sz;
	
	while ((sz = read(src, buffer, 64*1024)) != 0)
	{
		if (sz == -1)
		{
			char *errmsg;
			asprintf(&errmsg, "Read failed: %s", strerror(errno));
			taskMessageBox(data, "Error", errmsg, GWM_MBICON_ERROR | GWM_MBUT_OK);
			free(errmsg);
			return -1;
		};
		
		ssize_t wrsz;
		if ((wrsz = write(dest, buffer, (size_t) sz)) != (size_t) sz)
		{
			if (wrsz != -1) errno = ENOSPC;
			char *errmsg;
			asprintf(&errmsg, "Write failed: %s", strerror(errno));
			taskMessageBox(data, "Error", errmsg, GWM_MBICON_ERROR | GWM_MBUT_OK);
			free(errmsg);
			return -1;
		};
		
		(*dataDone) += (size_t) sz;
		updateProgress(data, opname, objCounter, numObj, *dataDone, dataTotal);
	};
	
	close(src);
	close(dest);
	
	return 0;
};

static void pasteTask(TaskData *data, void *context)
{
	PasteData *pdata = (PasteData*) context;
	char *cbspec = pdata->cbspec;
	char *saveptr;
	
	char *cmd = strtok_r(cbspec, "\n", &saveptr);
	if (cmd == NULL) return;
	
	int op;
	const char *opname;
	if (strcmp(cmd, "*CUT*") == 0)
	{
		op = FILEOP_MOVE;
		opname = "Moving";
	}
	else if (strcmp(cmd, "*COPY*") == 0)
	{
		op = FILEOP_COPY;
		opname = "Copying";
	}
	else
	{
		return;
	};
	
	FileOpObject *firstObj = NULL;
	FileOpObject *lastObj = NULL;
	int numObj = 0;
	
	taskSetText(data, "Counting objects...");
	
	char *path;
	for (path=strtok_r(NULL, "\n", &saveptr); path!=NULL; path=strtok_r(NULL, "\n", &saveptr))
	{
		addPathObjects(pdata->destdir, op, path, &firstObj, &lastObj, &numObj, data);
	};
	
	size_t dataDone = 0;
	size_t dataTotal = 0;
	
	FileOpObject *obj;
	for (obj=firstObj; obj!=NULL; obj=obj->next)
	{
		dataTotal += obj->size;
	};
	
	// do it
	int objCounter = 0;
	for (obj=firstObj; obj!=NULL; obj=obj->next)
	{
		objCounter++;
		updateProgress(data, opname, objCounter, numObj, dataDone, dataTotal);
		
		if (obj->op == FILEOP_MOVE)
		{
			if (strcmp(obj->src, obj->dest) == 0)
			{
				// NOP
				dataDone += obj->size;
				continue;
			};
			
			if (__syscall(__SYS_mv, AT_FDCWD, obj->src, AT_FDCWD, obj->dest, __MV_EXCL) != 0)
			{
				if (errno == EXDEV)
				{
					// cannot do cross-device moves directly; must copy
					if (doCopy(data, obj, &dataDone, dataTotal, objCounter, numObj, opname) == -1) break;
					unlink(obj->src);
				}
				else if (errno == EEXIST)
				{
					char *questmsg;
					asprintf(&questmsg, "The file %s already exists. Would you like "
							"to replace it?", obj->dest);
					if (taskMessageBox(data, "File exists", questmsg,
						GWM_MBICON_WARN | GWM_MBUT_YESNO) == GWM_SYM_YES)
					{
						free(questmsg);
						if (__syscall(__SYS_mv, AT_FDCWD, obj->src, AT_FDCWD, obj->dest, 0) != 0)
						{
							char *errmsg;
							asprintf(&errmsg, "Move failed: %s", strerror(errno));
							taskMessageBox(data, "Error", errmsg, GWM_MBICON_ERROR | GWM_MBUT_OK);
							free(errmsg);
							break;
						};
					}
					else
					{
						free(questmsg);
						continue;
					};
				}
				else
				{
					char *errmsg;
					asprintf(&errmsg, "Move failed: %s", strerror(errno));
					taskMessageBox(data, "Error", errmsg, GWM_MBICON_ERROR | GWM_MBUT_OK);
					free(errmsg);
					break;
				};
			}
			else
			{
				dataDone += obj->size;
			};
		}
		else if (obj->op == FILEOP_COPY)
		{
			if (strcmp(obj->src, obj->dest) == 0)
			{
				int i;
				char *proposed = NULL;
				
				for (i=1;;i++)
				{
					free(proposed);
					asprintf(&proposed, "%s (%d)", obj->dest, i);
					
					struct stat st;
					if (stat(proposed, &st) != 0) break;
				};
				
				free(obj->dest);
				obj->dest = proposed;
			};
			
			if (doCopy(data, obj, &dataDone, dataTotal, objCounter, numObj, opname) == -1) break;
		}
		else if (obj->op == FILEOP_MKDIR)
		{
			if (mkdir(obj->dest, 0755) != 0)
			{
				char *errmsg;
				asprintf(&errmsg, "Failed to create directory %s: %s", obj->dest, strerror(errno));
				taskMessageBox(data, "Error", errmsg, GWM_MBICON_ERROR | GWM_MBUT_OK);
				free(errmsg);
				break;
			};
		}
		else if (obj->op == FILEOP_RMDIR)
		{
			if (rmdir(obj->dest) != 0)
			{
				char *errmsg;
				asprintf(&errmsg, "Failed to delete directory %s: %s", obj->dest, strerror(errno));
				taskMessageBox(data, "Error", errmsg, GWM_MBICON_ERROR | GWM_MBUT_OK);
				free(errmsg);
				break;
			};
		};
	};
	
	while (firstObj != NULL)
	{
		FileOpObject *obj = firstObj;
		firstObj = obj->next;
		
		free(obj->src);
		free(obj->dest);
		free(obj);
	};
	
	// sync disks
	taskSetText(data, "Syncing disks...");
	sync();
};

void dvPaste(DirView *dv)
{
	DirViewData *data = (DirViewData*) gwmGetData(dv, dvHandler);
	
	size_t cbsize;
	char *cbspec = gwmClipboardGetText(&cbsize);
	if (cbspec == NULL) return;
	
	PasteData pdata;
	pdata.destdir = data->location;
	pdata.cbspec = cbspec;
	
	taskRun("File operation", pasteTask, &pdata);
	free(cbspec);
	
	dvGoTo(dv, dvGetLocation(dv));
};

void dvRemove(DirView *dv)
{
	// TODO: trash. this will also take care of removing directories (since we can move an entire directory
	// to the trash in one go)

	if (gwmMessageBox(dv, "Remove files", "Are you sure you want to PERMANENTLY delete the selected file(s)?",
				GWM_MBICON_WARN | GWM_MBUT_YESNO) != GWM_SYM_YES)
	{
		return;
	};
	
	DirViewData *data = (DirViewData*) gwmGetData(dv, dvHandler);
	
	DirEntry *ent;
	for (ent=data->ents; ent!=NULL; ent=ent->next)
	{
		if (ent->selected)
		{
			if (unlink(ent->path) != 0)
			{
				char *errmsg;
				asprintf(&errmsg, "Cannot delete %s: %s", ent->name, strerror(errno));
				gwmMessageBox(dv, "Remove files", errmsg, GWM_MBICON_ERROR | GWM_MBUT_OK);
				free(errmsg);
			};
		};
	};
	
	dvGoTo(dv, data->location);
};
