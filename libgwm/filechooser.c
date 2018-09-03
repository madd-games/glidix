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
#include <assert.h>

#define	FC_WIDTH			500
#define	FC_HEIGHT			300

#define	FC_COL_NAME			1

/**
 * Describes a file type filter.
 */
typedef struct FCFilter_
{
	struct FCFilter_*		next;
	uint64_t			id;
	char*				label;
	char*				filtspec;
	char*				ext;
} FCFilter;

typedef struct
{
	/**
	 * File chooser mode (GWM_FILE_OPEN or GWM_FILE_SAVE).
	 */
	int				mode;
	
	/**
	 * Layout.
	 */
	GWMLayout*			mainBox;
	GWMLayout*			grid;
	
	/**
	 * All the controls.
	 */
	GWMCombo*			comPath;
	GWMDataCtrl*			dcFiles;
	GWMLabel*			lblFileName;
	GWMTextField*			txtFileName;
	GWMButton*			btnAccept;
	GWMLabel*			lblFileType;
	GWMOptionMenu*			optFileType;
	GWMButton*			btnCancel;
	GWMCheckbox*			cbExt;
	
	/**
	 * Current location.
	 */
	char*				location;
	
	/**
	 * Result.
	 */
	char*				result;
	
	/**
	 * Head of filter list.
	 */
	FCFilter*			filts;
	uint64_t			nextFiltID;
} FCData;

static int filtMatch(FCData *data, uint64_t filtID, const char *filename, FSMimeType *mime)
{
	if (mime != NULL)
	{
		if (strcmp(mime->mimename, "inode/directory") == 0)
		{
			// always show directories
			return 1;
		};
	};
	
	FCFilter *filt;
	for (filt=data->filts; filt!=NULL; filt=filt->next)
	{
		if (filt->id == filtID) break;
	};
	
	assert(filt != NULL);
	
	const char *scan = filt->filtspec;
	while (*scan != 0)
	{
		char spec[128];
		char *put = spec;
		
		while (*scan != ';' && *scan != 0)
		{
			*put++ = *scan++;
		};
		
		*put = 0;
		if (*scan == ';') scan++;
		
		char *starPos = strchr(spec, '*');
		if (starPos == NULL)
		{
			if (strcmp(spec, filename) == 0)
			{
				return 1;
			};
		}
		else
		{
			int a = memcmp(filename, spec, starPos-spec);
			
			char *endSpec = starPos+1;
			if (strlen(endSpec) <= strlen(filename))
			{
				int b = strcmp(endSpec, &filename[strlen(filename)-strlen(endSpec)]);
				
				if (a == 0 && b == 0)
				{
					return 1;
				};
			};
		};
	};
	
	return 0;
};

static void fcListDir(GWMFileChooser *fc, FCData *data, const char *path)
{
	DIR *dirp = opendir(path);
	if (dirp == NULL)
	{
		gwmMessageBox(fc, "Error", strerror(errno), GWM_MBICON_ERROR | GWM_MBUT_OK);
		return;
	};
	
	GWMDataNode *node;
	while ((node = gwmGetDataNode(data->dcFiles, NULL, 0)) != NULL)
	{
		gwmDeleteDataNode(data->dcFiles, node);
	};
	
	struct dirent *ent;
	while ((ent = readdir(dirp)) != NULL)
	{
		if ((strcmp(ent->d_name, ".") == 0) || (strcmp(ent->d_name, "..") == 0))
		{
			continue;
		};
		
		char *fullpath;
		asprintf(&fullpath, "%s/%s", path, ent->d_name);
		
		uint64_t filtID = gwmReadOptionMenu(data->optFileType);
		
		FSMimeType *mime = fsGetType(fullpath);
		DDISurface *icon = gwmGetFileIcon(mime->iconName, GWM_FICON_SMALL);
		
		if (filtMatch(data, filtID, ent->d_name, mime))
		{
			GWMDataNode *node = gwmAddDataNode(data->dcFiles, GWM_DATA_ADD_BOTTOM_CHILD, NULL);
			gwmSetDataString(data->dcFiles, node, FC_COL_NAME, ent->d_name);
			gwmSetDataNodeIcon(data->dcFiles, node, icon);
			
			char *rpath = realpath(fullpath, NULL);
			if (rpath == NULL)
			{
				gwmMessageBox(NULL, "realpath", strerror(errno), GWM_MBICON_ERROR | GWM_MBUT_OK);
				abort();
			};
			gwmSetDataNodeDesc(data->dcFiles, node, rpath);
		};
		
		free(fullpath);
	};
	
	closedir(dirp);
	
	char *rpath = realpath(path, NULL);
	if (rpath != NULL)
	{
		gwmWriteCombo(data->comPath, rpath);
	}
	else
	{
		gwmMessageBox(NULL, "realpath", strerror(errno), GWM_MBICON_ERROR | GWM_MBUT_OK);
		abort();
	};
	
	free(data->location);
	data->location = rpath;
};

static int doFileChoice(GWMFileChooser *fc, FCData *data, const char *path)
{
	if (data->mode == GWM_FILE_OPEN)
	{
		struct stat st;
		if (stat(path, &st) != 0)
		{
			gwmMessageBox(fc, "Error", "The specified file does not exist or could not be opened.",
					GWM_MBICON_ERROR | GWM_MBUT_OK);
			return GWM_EVSTATUS_CONT;
		};
		
		if (S_ISDIR(st.st_mode))
		{
			gwmMessageBox(fc, "Error", "The specified name refers to a directory.", GWM_MBICON_ERROR | GWM_MBUT_OK);
			return GWM_EVSTATUS_CONT;
		};
		
		data->result = strdup(path);
		return GWM_EVSTATUS_BREAK;
	}
	else
	{	
		struct stat st;
		if (stat(path, &st) == 0)
		{
			if (S_ISDIR(st.st_mode))
			{
				gwmMessageBox(fc, "Error", "The specified name refers to a directory.", GWM_MBICON_ERROR | GWM_MBUT_OK);
				return GWM_EVSTATUS_CONT;
			};
			
			if (gwmMessageBox(fc, "File exists", "The specified file already exists. Do you want to overwrite it?",
						GWM_MBICON_WARN | GWM_MBUT_YESNO) != GWM_SYM_YES)
			{
				return GWM_EVSTATUS_CONT;
			};
		};
		
		data->result = strdup(path);
		return GWM_EVSTATUS_BREAK;
	};
};

static int fcHandler(GWMEvent *ev, GWMFileChooser *fc, void *context)
{
	FCData *data = (FCData*) context;
	GWMCommandEvent *cmdev = (GWMCommandEvent*) ev;
	GWMDataEvent *dev = (GWMDataEvent*) ev;
	GWMDataNode *node;
	struct stat st;
	const char *path;
	const char *name;
	
	switch (ev->type)
	{
	case GWM_EVENT_OPTION_SET:
		if (ev->win == data->comPath->id) fcListDir(fc, data, gwmReadCombo(data->comPath));
		else fcListDir(fc, data, data->location);
		return GWM_EVSTATUS_OK;
	case GWM_EVENT_COMMAND:
		if (cmdev->symbol == GWM_SYM_OK && ev->win == data->comPath->id)
		{
			fcListDir(fc, data, gwmReadCombo(data->comPath));
			return GWM_EVSTATUS_OK;
		}
		else if (cmdev->symbol == GWM_SYM_CANCEL)
		{
			data->result = NULL;
			return GWM_EVSTATUS_BREAK;
		}
		else if (cmdev->symbol == GWM_SYM_OK)
		{
			name = gwmReadTextField(data->txtFileName);
			
			if (name[0] == 0)
			{
				return GWM_EVSTATUS_OK;
			};
			
			if (strchr(name, '/') != NULL)
			{
				gwmMessageBox(fc, "Error", "File names cannot include the slash (/) character.",
						GWM_MBICON_ERROR | GWM_MBUT_OK);
				return GWM_EVSTATUS_OK;
			};
			
			char *finalPath;
			const char *suffix = "";

			if (data->cbExt != NULL)
			{
				if (gwmGetCheckboxState(data->cbExt))
				{
					uint64_t filtID = gwmReadOptionMenu(data->optFileType);
				
					if (!filtMatch(data, filtID, name, NULL))
					{
						FCFilter *filt;
						for (filt=data->filts; filt!=NULL; filt=filt->next)
						{
							if (filt->id == filtID) break;
						};
	
						assert(filt != NULL);	
				
						suffix = filt->ext;
					};
				};
			};

			asprintf(&finalPath, "%s/%s%s", data->location, name, suffix);
			int result = doFileChoice(fc, data, finalPath);
			free(finalPath);
			return result;
		};
		return GWM_EVSTATUS_CONT;
	case GWM_EVENT_DATA_SELECT_CHANGED:
		node = gwmGetDataSelection(data->dcFiles, 0);
		if (node != NULL)
		{
			gwmWriteTextField(data->txtFileName, gwmGetDataString(data->dcFiles, node, FC_COL_NAME));
		};
		return GWM_EVSTATUS_OK;
	case GWM_EVENT_DATA_DELETING:
		free(gwmGetDataNodeDesc(data->dcFiles, dev->node));
		return GWM_EVSTATUS_CONT;
	case GWM_EVENT_DATA_ACTIVATED:
		path = (const char*) gwmGetDataNodeDesc(data->dcFiles, dev->node);
		if (stat(path, &st) != 0)
		{
			gwmMessageBox(fc, "Error", strerror(errno), GWM_MBICON_ERROR | GWM_MBUT_OK);
			return GWM_EVSTATUS_OK;
		};
		if (S_ISDIR(st.st_mode))
		{
			char *goPath = strdup(path);
			fcListDir(fc, data, goPath);
			free(goPath);
		}
		else
		{
			return doFileChoice(fc, data, path);
		};
		return GWM_EVSTATUS_CONT;
	default:
		return GWM_EVSTATUS_CONT;
	};
};

GWMFileChooser* gwmCreateFileChooser(GWMWindow *parent, const char *caption, int mode)
{
	GWMFileChooser *fc = gwmCreateModal(caption, GWM_POS_UNSPEC, GWM_POS_UNSPEC, 0, 0);
	if (fc == NULL) return NULL;
	
	FCData *data = (FCData*) malloc(sizeof(FCData));
	data->mode = mode;
	
	data->mainBox = gwmCreateBoxLayout(GWM_BOX_VERTICAL);
	gwmSetWindowLayout(fc, data->mainBox);
	
	data->comPath = gwmNewCombo(fc);
	gwmBoxLayoutAddWindow(data->mainBox, data->comPath, 0, 2, GWM_BOX_FILL | GWM_BOX_ALL);
	
	char *wd = getcwd(NULL, 0);

	gwmAddComboOption(data->comPath, wd);	
	gwmAddComboOption(data->comPath, "/");

	free(wd);
	data->location = strdup(".");
	
	data->filts = NULL;
	data->nextFiltID = 0;
	
	data->dcFiles = gwmNewDataCtrl(fc);
	gwmBoxLayoutAddWindow(data->mainBox, data->dcFiles, 1, 2, GWM_BOX_FILL | GWM_BOX_ALL);
	
	GWMDataColumn *col = gwmAddDataColumn(data->dcFiles, "Name", GWM_DATA_STRING, FC_COL_NAME);
	gwmSetDataColumnWidth(data->dcFiles, col, FC_WIDTH);
	
	data->grid = gwmCreateGridLayout(6);
	gwmBoxLayoutAddLayout(data->mainBox, data->grid, 0, 2, GWM_BOX_FILL | GWM_BOX_ALL);
	
	data->lblFileName = gwmCreateLabel(fc, "File name:", 0);
	gwmGridLayoutAddWindow(data->grid, data->lblFileName, 1, 1, GWM_GRID_FILL, GWM_GRID_CENTER);
	
	data->txtFileName = gwmNewTextField(fc);
	gwmGridLayoutAddWindow(data->grid, data->txtFileName, 4, 1, GWM_GRID_FILL, GWM_GRID_CENTER);
	
	const char *acceptLabel;
	if (mode == GWM_FILE_OPEN)
	{
		acceptLabel = "Open";
	}
	else
	{
		acceptLabel = "Save";
	};
	
	data->btnAccept = gwmCreateButtonWithLabel(fc, GWM_SYM_OK, acceptLabel);
	gwmGridLayoutAddWindow(data->grid, data->btnAccept, 1, 1, GWM_GRID_FILL, GWM_GRID_CENTER);
	
	data->lblFileType = gwmCreateLabel(fc, "File type:", 0);
	gwmGridLayoutAddWindow(data->grid, data->lblFileType, 1, 1, GWM_GRID_FILL, GWM_GRID_CENTER);
	
	data->optFileType = gwmNewOptionMenu(fc);
	gwmGridLayoutAddWindow(data->grid, data->optFileType, 4, 1, GWM_GRID_FILL, GWM_GRID_CENTER);
	
	data->btnCancel = gwmCreateButtonWithLabel(fc, GWM_SYM_CANCEL, NULL);
	gwmGridLayoutAddWindow(data->grid, data->btnCancel, 1, 1, GWM_GRID_FILL, GWM_GRID_CENTER);
	
	gwmPushEventHandler(fc, fcHandler, data);
	
	data->cbExt = NULL;
	return fc;
};

void gwmSetFileChooserName(GWMFileChooser *fc, const char *filename)
{
	FCData *data = (FCData*) gwmGetData(fc, fcHandler);
	if (data == NULL) return;
	
	gwmWriteTextField(data->txtFileName, filename);
};

void gwmAddFileChooserFilter(GWMFileChooser *fc, const char *label, const char *filtspec, const char *ext)
{
	FCData *data = (FCData*) gwmGetData(fc, fcHandler);
	if (data == NULL) return;

	FCFilter *filt = (FCFilter*) malloc(sizeof(FCFilter));
	filt->next = NULL;
	filt->id = data->nextFiltID++;
	filt->label = strdup(label);
	filt->filtspec = strdup(filtspec);
	filt->ext = strdup(ext);
	
	gwmAddOptionMenu(data->optFileType, filt->id, label);
	
	if (data->filts == NULL)
	{
		data->filts = filt;
		gwmSetOptionMenu(data->optFileType, filt->id, label);
	}
	else
	{
		FCFilter *last = data->filts;
		while (last->next != NULL) last = last->next;
		last->next = filt;
	};
};

char* gwmRunFileChooser(GWMFileChooser *fc)
{
	FCData *data = (FCData*) gwmGetData(fc, fcHandler);
	if (data == NULL) return NULL;
	
	if (data->filts == NULL)
	{
		gwmAddFileChooserFilter(fc, "All files", "*", "");
	};
	
	if (data->mode == GWM_FILE_SAVE)
	{
		// check if any filter has a filename extension
		int found = 0;
		
		FCFilter *filt;
		for (filt=data->filts; filt!=NULL; filt=filt->next)
		{
			if (filt->ext[0] != 0) found = 1;
		};
		
		if (found)
		{
			// then create the "append extension?" checkbox
			data->cbExt = gwmNewCheckbox(fc);
			gwmGridLayoutAddWindow(data->grid, data->cbExt, 6, 1, GWM_GRID_FILL, GWM_GRID_CENTER);
			
			gwmSetCheckboxLabel(data->cbExt, "Automatically set filename extension");
			gwmSetCheckboxState(data->cbExt, GWM_CB_ON);
		};
	};
	
	gwmLayout(fc, FC_WIDTH, FC_HEIGHT);
	gwmTextFieldSelectAll(data->txtFileName);
	gwmFocus(data->txtFileName);
	gwmRunModal(fc, GWM_WINDOW_NOTASKBAR | GWM_WINDOW_NOSYSMENU);
	gwmSetWindowFlags(fc, GWM_WINDOW_HIDDEN | GWM_WINDOW_NOTASKBAR);
	
	gwmDestroyButton(data->btnCancel);
	gwmDestroyOptionMenu(data->optFileType);
	gwmDestroyLabel(data->lblFileType);
	gwmDestroyButton(data->btnAccept);
	gwmDestroyTextField(data->txtFileName);
	gwmDestroyLabel(data->lblFileName);
	gwmDestroyDataCtrl(data->dcFiles);
	gwmDestroyCombo(data->comPath);
	
	if (data->cbExt != NULL) gwmDestroyCheckbox(data->cbExt);
	
	gwmDestroyBoxLayout(data->mainBox);
	gwmDestroyGridLayout(data->grid);
	
	char *result = data->result;
	free(data);
	return result;
};
