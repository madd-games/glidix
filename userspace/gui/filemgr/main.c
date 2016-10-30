/*
	Glidix GUI

	Copyright (c) 2014-2016, Madd Games.
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

#include <sys/stat.h>
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
#include <pwd.h>
#include <errno.h>
#include <fcntl.h>
#include "filemgr.h"

#define	DEFAULT_WIDTH			500
#define	DEFAULT_HEIGHT			500

GWMWindow *winMain;
GWMWindow *txtPath;
GWMWindow *sbView;
GWMWindow *menubar;

FMFileType *typeDatabase;
DDIPixelFormat screenFormat;
FMDir *currentDir;

FMFileType* ftDir;
FMFileType* ftBinFile;
FMFileType* ftTextFile;
FMFileType* ftExecFile;

GWMMenu *menuEdit;

int selectedIndex = -1;

FMFileType* addFileType(const char *mimeName, const char *humanName, const char *iconPath)
{
	FMFileType *type = (FMFileType*) malloc(sizeof(FMFileType));
	memset(type, 0, sizeof(FMFileType));
	type->mimeName = strdup(mimeName);
	type->humanName = strdup(humanName);
	type->icon = ddiLoadAndConvertPNG(&screenFormat, iconPath, NULL);
	if (type->icon == NULL)
	{
		fprintf(stderr, "Failed to load icon %s\n", iconPath);
		abort();
	};
	
	if (typeDatabase == NULL)
	{
		typeDatabase = type;
	}
	else
	{
		FMFileType *last = typeDatabase;
		while (last->next != NULL) last = last->next;
		last->next = type;
	};
	
	return type;
};

int isTextFile(const char *path, struct stat *st)
{
	if (st->st_size > 6*1024*1024)
	{
		// text files larger than 6MB are improbable
		return 0;
	};
	
	int fd = open(path, O_RDONLY);
	if (fd == -1) return 0;
	
	char buffer[8*1024];
	
	while (1)
	{
		ssize_t sizeNow = read(fd, buffer, 8*1024);
		if (sizeNow == -1)
		{
			close(fd);
			return 0;
		};
		
		if (sizeNow == 0) break;
		
		ssize_t i;
		for (i=0; i<sizeNow; i++)
		{
			if (buffer[i] == 0)
			{
				close(fd);
				return 0;
			};
		};
	};
	
	close(fd);
	return 1;
};

int isExecFile(const char *path, struct stat *st)
{
	if (st->st_size < 18) return 0;
	
	int fd = open(path, O_RDONLY);
	if (fd == -1) return 0;
	
	char buffer[18];
	static char elfMagic[18] = {
					0x7F, 'E', 'L', 'F', 	/* magic */
					2,			/* ELF64 */
					1,			/* little endian */
					1,			/* ELF version 1 */
					0,			/* System V ABI */
					0,			/* ABI version */
					0, 0, 0, 0, 0, 0, 0,	/* padding */
					2, 0			/* executable file */
	};
	
	if (read(fd, buffer, 18) != 18)
	{
		close(fd);
		return 0;
	};
	
	close(fd);
	
	return memcmp(buffer, elfMagic, 18) == 0;
};

FMFileType *fmDetermineType(const char *path)
{
	struct stat st;
	if (stat(path, &st) != 0)
	{
		return ftBinFile;
	};
	
	if (S_ISDIR(st.st_mode))
	{
		return ftDir;
	}
	else
	{
		// TODO: determine type of file
		if (isTextFile(path, &st)) return ftTextFile;
		if (isExecFile(path, &st)) return ftExecFile;
		return ftBinFile;
	};
};

FMDir *fmOpenDir(const char *path, int *errnum)
{
	DIR *dirp = opendir(path);
	if (dirp == NULL)
	{
		if (errnum != NULL) *errnum = errno;
		return NULL;
	};
	
	FMDir *dir = (FMDir*) malloc(sizeof(FMDir));
	dir->path = strdup(path);
	dir->list = NULL;
	dir->numElements = 0;
	
	FMDirEntry **nextPut = &dir->list;
	struct dirent *ent;
	while ((ent = readdir(dirp)) != NULL)
	{
		if (ent->d_name[0] != '.')
		{
			FMDirEntry *fment = (FMDirEntry*) malloc(sizeof(FMDirEntry));
			fment->name = strdup(ent->d_name);
		
			char fullpath[PATH_MAX];
			sprintf(fullpath, "%s/%s", path, ent->d_name);
			fment->type = fmDetermineType(fullpath);
			fment->next = NULL;
			*nextPut = fment;
			nextPut = &fment->next;
			dir->numElements++;
		};
	};
	
	closedir(dirp);
	return dir;
};

void fmCloseDir(FMDir *dir)
{
	FMDirEntry *entry = dir->list;
	while (entry != NULL)
	{
		free(entry->name);
		FMDirEntry *next = entry->next;
		free(entry);
		entry = next;
	};
	
	free(dir->path);
	free(dir);
};

void fmSwitchDir(const char *relpath)
{
	char path[PATH_MAX];
	if (realpath(relpath, path) == NULL)
	{
		char errmsg[1024];
		sprintf(errmsg, "Failed to resolve path %s: %s\n", path, strerror(errno));
		if (currentDir == NULL)
		{
			exit(1);
		};
		return;
	};
	
	int errnum;
	FMDir *dir = fmOpenDir(path, &errnum);
	if (dir == NULL)
	{
		char errmsg[1024];
		sprintf(errmsg, "Cannot open directory %s: %s", path, strerror(errnum));
		gwmMessageBox(NULL, "Error opening directory", errmsg, GWM_MBICON_ERROR | GWM_MBUT_OK);
		
		if (currentDir == NULL)
		{
			exit(1);
		}
		else
		{
			return;
		};
	};
	
	chdir(path);
	if (currentDir != NULL) fmCloseDir(currentDir);
	currentDir = dir;
	
	gwmWriteTextField(txtPath, path);
	
	DDISurface *canvas = gwmGetWindowCanvas(winMain);
	int elementsPerLine = canvas->width/100;
	gwmSetScrollbarParams(sbView, 0, canvas->height-40, 100*(currentDir->numElements/elementsPerLine+1), 0);
};

void redraw()
{
	static DDIColor white = {0xFF, 0xFF, 0xFF, 0xFF};
	DDISurface *canvas = gwmGetWindowCanvas(winMain);
	
	int scrollY = gwmGetScrollbarOffset(sbView);
	ddiFillRect(canvas, 0, 40, canvas->width, canvas->height-40, &white);
	
	int elementsPerLine = canvas->width/100;
	int currentIndex = 0;
	
	FMDirEntry *entry;
	for (entry=currentDir->list; entry!=NULL; entry=entry->next)
	{
		int row = currentIndex / elementsPerLine;
		int col = currentIndex % elementsPerLine;
		int selected = (currentIndex == selectedIndex);
		currentIndex++;
		
		int startX = col * 100;
		int startY = row * 100 - scrollY + 40;
		
		if (selected)
		{
			ddiFillRect(canvas, startX, startY, 100, 100, GWM_COLOR_SELECTION);
		};
		
		ddiBlit(entry->type->icon, 0, 0, canvas, startX+18, startY+2, 64, 64);
		
		DDIPen *pen = ddiCreatePen(&canvas->format, gwmGetDefaultFont(), startX+2, startY+68,
						96, 30, 0, 0, NULL);
		ddiSetPenAlignment(pen, DDI_ALIGN_CENTER);
		ddiWritePen(pen, entry->name);
		ddiExecutePen(pen, canvas);
		ddiDeletePen(pen);
	};
	
	gwmPostDirty(winMain);
};

int scrollbarUpdate(void *context)
{
	redraw();
	return 0;
};

void onDoubleClick()
{
	FMDirEntry *entry;
	FMDirEntry *selectedEntry = NULL;
	int currentIndex = 0;
	for (entry=currentDir->list; entry!=NULL; entry=entry->next)
	{
		if (currentIndex == selectedIndex)
		{
			selectedEntry = entry;
			break;
		};
		
		currentIndex++;
	};
	
	if (selectedEntry == NULL) return;
	
	if (strcmp(selectedEntry->type->mimeName, "inode/directory") == 0)
	{
		selectedIndex = -1;
		char newPath[PATH_MAX];
		sprintf(newPath, "%s/%s", currentDir->path, selectedEntry->name);
		fmSwitchDir(newPath);
		redraw();
	}
	else if (strcmp(selectedEntry->type->mimeName, "application/x-executable") == 0)
	{
		char execPath[PATH_MAX];
		sprintf(execPath, "%s/%s", currentDir->path, selectedEntry->name);
		
		pid_t pid = fork();
		if (pid == -1)
		{
			char errmsg[1024];
			sprintf(errmsg, "fork failed: %s", strerror(errno));
			gwmMessageBox(NULL, "Run Executable", errmsg, GWM_MBICON_ERROR | GWM_MBUT_OK);
			return;
		}
		else if (pid == 0)
		{
			// i am the child
			if (fork() == 0)
			{
				chdir(currentDir->path);
				execl(execPath, execPath, NULL);
			};
			
			exit(1);
		}
		else
		{
			waitpid(pid, NULL, 0);
		};
	};
};

int pathUpdate(void *param)
{
	size_t len = gwmGetTextFieldSize(txtPath);
	char newPath[len+1];
	newPath[len] = 0;
	gwmReadTextField(txtPath, newPath, 0, len);
	fmSwitchDir(newPath);
	redraw();
	return 0;
};

int fmNewDir(void *param)
{
	char *dirname = gwmGetInput("New Directory", "Enter the name of the new directory:", "New Directory");
	if (dirname == NULL) return 0;
	
	if (strlen(dirname) > 127)
	{
		free(dirname);
		gwmMessageBox(NULL, "New Directory", "The name you entered is too long (over 127 characters)",
				GWM_MBICON_ERROR | GWM_MBUT_OK);
		return 0;
	};
	
	if (strchr(dirname, '/') != NULL)
	{
		char errmsg[1024];
		sprintf(errmsg, "The directory name '%s' is invalid because it contains a slash (/)", dirname);
		free(dirname);
		gwmMessageBox(NULL, "New Directory", errmsg, GWM_MBICON_ERROR | GWM_MBUT_OK);
		return 0;
	};
	
	if (mkdir(dirname, 0755) != 0)
	{
		char errmsg[1024];
		sprintf(errmsg, "Cannot create directory '%s': %s", dirname, strerror(errno));
		free(dirname);
		gwmMessageBox(NULL, "New Directory", errmsg, GWM_MBICON_ERROR | GWM_MBUT_OK);
		return 0;
	};
	
	free(dirname);
	fmSwitchDir(".");
	redraw();
	return 0;
};

int fmNewFile(void *param)
{
	char *name = gwmGetInput("New File", "Enter the name of the new file:", "New Empty File");
	if (name == NULL) return 0;
	
	if (strlen(name) > 127)
	{
		free(name);
		gwmMessageBox(NULL, "New File", "The name you entered is too long (over 127 characters)",
				GWM_MBICON_ERROR | GWM_MBUT_OK);
		return 0;
	};
	
	if (strchr(name, '/') != NULL)
	{
		char errmsg[1024];
		sprintf(errmsg, "The file name '%s' is invalid because it contains a slash (/)", name);
		free(name);
		gwmMessageBox(NULL, "New File", errmsg, GWM_MBICON_ERROR | GWM_MBUT_OK);
		return 0;
	};
	
	int fd = open(name, O_WRONLY | O_CREAT | O_EXCL);
	if (fd == -1)
	{
		char errmsg[1024];
		sprintf(errmsg, "Cannot create file '%s': %s", name, strerror(errno));
		free(name);
		gwmMessageBox(NULL, "New File", errmsg, GWM_MBICON_ERROR | GWM_MBUT_OK);
		return 0;
	};
	
	close(fd);
	free(name);
	fmSwitchDir(".");
	redraw();
	return 0;
};

int fmOpenTerminal(void *param)
{
	if (fork() == 0)
	{
		if (fork() == 0)
		{
			chdir(currentDir->path);
			execl("/usr/bin/terminal", "terminal", NULL);
		};
		
		exit(0);
	};
	
	return 0;
};

int filemgrHandler(GWMEvent *ev, GWMWindow *win)
{
	DDISurface *canvas = gwmGetWindowCanvas(win);
	int scrollY = gwmGetScrollbarOffset(sbView);
	int elementsPerLine = canvas->width / 100;
	int gridX, gridY;
	
	switch (ev->type)
	{
	case GWM_EVENT_RESIZE_REQUEST:
		gwmMoveWindow(win, ev->x, ev->y);
		gwmResizeWindow(win, ev->width, ev->height);
		gwmResizeTextField(txtPath, ev->width);
		gwmMoveWindow(sbView, ev->width-8, 40);
		gwmSetScrollbarLen(sbView, ev->height-40);
		gwmMenubarAdjust(menubar);
		redraw();
		return 0;
	case GWM_EVENT_DOWN:
		if (ev->scancode == GWM_SC_MOUSE_LEFT)
		{
			gridX = ev->x/100;
			gridY = (ev->y+scrollY-40)/100;
			selectedIndex = gridY * elementsPerLine + gridX;
			redraw();
		};
		return 0;
	case GWM_EVENT_DOUBLECLICK:
		onDoubleClick();
		return 0;
	default:
		return gwmDefaultHandler(ev, win);
	};
};

int main(int argc, char *argv[])
{	
	if (gwmInit() != 0)
	{
		fprintf(stderr, "Failed to initialize GWM!\n");
		return 1;
	};
	
	winMain = gwmCreateWindow(NULL, "File Manager", GWM_POS_UNSPEC, GWM_POS_UNSPEC, DEFAULT_WIDTH, DEFAULT_HEIGHT,
					GWM_WINDOW_NOTASKBAR | GWM_WINDOW_HIDDEN);
	
	DDISurface *canvas = gwmGetWindowCanvas(winMain);
	memcpy(&screenFormat, &canvas->format, sizeof(DDIPixelFormat));
	DDISurface *iconFileMgr = ddiLoadAndConvertPNG(&canvas->format, "/usr/share/images/fileman.png", NULL);
	gwmSetWindowIcon(winMain, iconFileMgr);
	
	ftDir = addFileType("inode/directory", "Directory", "/usr/share/images/dir.png");
	ftBinFile = addFileType("application/octet-stream", "Binary File", "/usr/share/images/binfile.png");
	ftTextFile = addFileType("text/plain", "Text File", "/usr/share/images/textfile.png");
	ftExecFile = addFileType("application/x-executable", "Executable File", "/usr/share/images/exe.png");
	
	txtPath = gwmCreateTextField(winMain, "?", 0, 20, DEFAULT_WIDTH, 0);
	gwmSetTextFieldAcceptCallback(txtPath, pathUpdate, NULL);
	sbView = gwmCreateScrollbar(winMain, DEFAULT_WIDTH-8, 40, DEFAULT_HEIGHT-40, 0, 10, 10, GWM_SCROLLBAR_VERT);
	gwmSetScrollbarCallback(sbView, scrollbarUpdate, NULL);
	
	menuEdit = gwmCreateMenu();
	gwmMenuAddEntry(menuEdit, "New directory...", fmNewDir, NULL);
	gwmMenuAddEntry(menuEdit, "New empty file...", fmNewFile, NULL);
	gwmMenuAddSeparator(menuEdit);
	gwmMenuAddEntry(menuEdit, "Open terminal", fmOpenTerminal, NULL);
	
	menubar = gwmCreateMenubar(winMain);
	gwmMenubarAdd(menubar, "Edit", menuEdit);
	gwmMenubarAdjust(menubar);
	
	if (argc == 2)
	{
		fmSwitchDir(argv[1]);
	}
	else
	{
		struct passwd *pwent = getpwuid(getuid());
		if (pwent != NULL)
		{
			fmSwitchDir(pwent->pw_dir);
		};
	};
	
	redraw();
	
	gwmSetWindowFlags(winMain, GWM_WINDOW_MKFOCUSED | GWM_WINDOW_RESIZEABLE);
	gwmSetEventHandler(winMain, filemgrHandler);
	gwmMainLoop();
	gwmQuit();
	return 0;
};
