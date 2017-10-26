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

#include <sys/stat.h>
#include <sys/glidix.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <pthread.h>
#include <sys/mman.h>
#include <sys/ioctl.h>
#include <signal.h>
#include <libgwm.h>
#include <errno.h>
#include <dirent.h>
#include <fstools.h>

#define	FC_WIDTH			500
#define	FC_HEIGHT			300

typedef struct
{
	int					mode;
	char*					dir;
	GWMWindow*				txtDir;
	GWMWindow*				txtName;
	GWMWindow*				teListing;
	GWMWindow*				optFilter;
	GWMWindow*				btnAccept;
	GWMWindow*				btnCancel;
	char*					result;
} FileChooserData;

typedef struct
{
	char					path[PATH_MAX];
	DIR*					dirp;
} DirNode;

static const char *dirCols[] = {
	"Name",
	"Type"
};

static char* dirGetColCaption(int index)
{
	return strdup(dirCols[index]);
};

static void* dirOpenNode(const void *path)
{
	DirNode *node = (DirNode*) malloc(sizeof(DirNode));
	if (node == NULL) return NULL;
	
	strcpy(node->path, (const char*) path);
	node->dirp = opendir((const char*) path);
	if (node->dirp == NULL)
	{
		free(node);
		return NULL;
	};
	
	return node;
};

static int dirGetNext(void *context, GWMNodeInfo *info, size_t infoSize)
{
	DirNode *node = (DirNode*) context;
	struct dirent *ent = readdir(node->dirp);
	if (ent == NULL) return -1;
	
	strcpy((char*)info->niPath, ent->d_name);
	
	char pathname[PATH_MAX];
	sprintf(pathname, "%s/%s", node->path, ent->d_name);
	FSMimeType *mime = fsGetType(pathname);
	
	info->niFlags = 0;
	info->niIcon = gwmGetFileIcon(mime->iconName, GWM_FICON_SMALL);
	info->niValues[0] = strdup(ent->d_name);
	info->niValues[1] = strdup(mime->label);
	return 0;
};

static void dirCloseNode(void *context)
{
	DirNode *node = (DirNode*) context;
	closedir(node->dirp);
	free(node);
};

static GWMTreeEnum enumDir = {
	.teSize = 				sizeof(GWMTreeEnum),
	.tePathSize = 				PATH_MAX,
	.teNumCols =				sizeof(dirCols) / sizeof(const char*),
	.teGetColCaption =			dirGetColCaption,
	.teOpenNode =				dirOpenNode,
	.teGetNext =				dirGetNext,
	.teCloseNode =				dirCloseNode
};

GWMWindow* gwmCreateFileChooser(GWMWindow *parent, const char *caption, int mode)
{
	GWMWindow *dialog = gwmCreateModal(caption, GWM_POS_UNSPEC, GWM_POS_UNSPEC, FC_WIDTH, FC_HEIGHT);
	FileChooserData *data = (FileChooserData*) malloc(sizeof(FileChooserData));
	dialog->data = data;
	
	data->mode = mode;

	char *home = getenv("HOME");
	if (home == NULL)
	{
		data->dir = strdup("/");
	}
	else
	{
		data->dir = strdup(home);
	};
	
	return dialog;
};

static void onSelect(void *context)
{
	char buf[PATH_MAX];
	GWMWindow *fc = (GWMWindow*) context;
	FileChooserData *data = (FileChooserData*) fc->data;
	
	if (gwmTreeViewGetSelection(data->teListing, buf) == 0)
	{
		gwmWriteTextField(data->txtName, buf);
	};
};

static int onCancel(void *context)
{
	GWMWindow *fc = (GWMWindow*) context;
	FileChooserData *data = (FileChooserData*) fc->data;
	data->result = NULL;
	return -1;
};

static int onAccept(void *context)
{
	GWMWindow *fc = (GWMWindow*) context;
	FileChooserData *data = (FileChooserData*) fc->data;
	
	size_t nameSize = gwmGetTextFieldSize(data->txtName);
	char filename[nameSize+1];
	gwmReadTextField(data->txtName, filename, 0, nameSize);
	
	if (strchr(filename, '/') != NULL)
	{
		gwmMessageBox(NULL, "Error", "A filename may not to contain the '/' character", GWM_MBICON_ERROR | GWM_MBUT_OK);
		return 0;
	};
	
	char pathname[PATH_MAX];
	if (strlen(data->dir)+1+strlen(filename) >= PATH_MAX)
	{
		gwmMessageBox(NULL, "Error", "Path too long!", GWM_MBICON_ERROR | GWM_MBUT_OK);
		return 0;
	};
	
	sprintf(pathname, "%s/%s", data->dir, filename);
	struct stat st;
	int exists = (stat(pathname, &st) == 0);
	
	if (exists && S_ISDIR(st.st_mode))
	{
		char normalPath[PATH_MAX];
		if (realpath(pathname, normalPath) == NULL)
		{
			strcpy(normalPath, pathname);
		};
		
		gwmTreeViewUpdate(data->teListing, normalPath);
		gwmWriteTextField(data->txtDir, normalPath);
		free(data->dir);
		data->dir = strdup(normalPath);
		return 0;
	};
	
	if (data->mode == GWM_FILE_OPEN)
	{
		if (!exists)
		{
			gwmMessageBox(NULL, "Error", "The specified file does not exist or access was denied.",
					GWM_MBICON_ERROR | GWM_MBUT_OK);
			return 0;
		};
		
		data->result = strdup(pathname);
		return -1;
	}
	else
	{
		int ok = 1;
		if (exists)
		{
			int selection = gwmMessageBox(NULL, "Save File",
						"The specified file already exists! Do you want to replace it?",
						GWM_MBICON_WARN | GWM_MBUT_YESNO);
			ok = (selection == GWM_SYM_YES);
		};
		
		if (ok)
		{
			data->result = strdup(pathname);
			return -1;
		};
	};
	
	return 0;
};

char* gwmRunFileChooser(GWMWindow *fc)
{
	FileChooserData *data = (FileChooserData*) fc->data;
	
	data->txtDir = gwmCreateTextField(fc, data->dir, 2, 5, FC_WIDTH-4, 0);
	data->teListing = gwmCreateTreeView(fc, 2, 30, FC_WIDTH-4, FC_HEIGHT-96, &enumDir, data->dir, 0);
	gwmTreeViewSetSelectCallback(data->teListing, onSelect, fc);
	gwmTreeViewSetActivateCallback(data->teListing, onAccept, fc);
	data->txtName = gwmCreateTextField(fc, "", 70, FC_HEIGHT-60, FC_WIDTH-154, 0);
	data->optFilter = gwmCreateOptionMenu(fc, 0, "Filters", 70, FC_HEIGHT-28, FC_WIDTH-154, GWM_OPTMENU_DISABLED);
	
	const char *acceptLabel;
	if (data->mode == GWM_FILE_OPEN)
	{
		acceptLabel = "Open";
	}
	else
	{
		acceptLabel = "Save";
	};
	
	data->btnAccept = gwmCreateButton(fc, acceptLabel, FC_WIDTH-82, FC_HEIGHT-64, 80, 0);
	gwmSetButtonCallback(data->btnAccept, onAccept, fc);
	data->btnCancel = gwmCreateButton(fc, "Cancel", FC_WIDTH-82, FC_HEIGHT-32, 80, 0);
	gwmSetButtonCallback(data->btnCancel, onCancel, fc);
	
	DDISurface *canvas = gwmGetWindowCanvas(fc);
	DDIPen *pen = ddiCreatePen(&canvas->format, gwmGetDefaultFont(), 2, FC_HEIGHT-58, FC_WIDTH, 32, 0, 0, NULL);
	if (pen != NULL)
	{
		ddiWritePen(pen, "File name:");
		ddiExecutePen(pen, canvas);
		ddiDeletePen(pen);
	};
	
	pen = ddiCreatePen(&canvas->format, gwmGetDefaultFont(), 2, FC_HEIGHT-26, FC_WIDTH, 32, 0, 0, NULL);
	if (pen != NULL)
	{
		ddiWritePen(pen, "File types:");
		ddiExecutePen(pen, canvas);
		ddiDeletePen(pen);
	};
	
	gwmPostDirty(fc);
	gwmRunModal(fc, GWM_WINDOW_MKFOCUSED | GWM_WINDOW_NOTASKBAR | GWM_WINDOW_NOICON | GWM_WINDOW_NOSYSMENU);
	char *result = data->result;
	
	gwmDestroyTreeView(data->teListing);
	gwmDestroyTextField(data->txtDir);
	gwmDestroyTextField(data->txtName);
	gwmDestroyOptionMenu(data->optFilter);
	gwmDestroyButton(data->btnAccept);
	gwmDestroyButton(data->btnCancel);
	gwmDestroyWindow(fc);
	free(data);
	
	return result;
};
