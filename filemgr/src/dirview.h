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

#ifndef DIRVIEW_H_
#define DIRVIEW_H_

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
#include <fstools.h>

/**
 * Describes a directory entry as understood by the file manager.
 */
typedef struct DirEntry_
{
	/**
	 * Links.
	 */
	struct DirEntry_*				prev;
	struct DirEntry_*				next;
	
	/**
	 * The entry's icon (64x64).
	 */
	DDISurface*					icon;
	
	/**
	 * MIME type.
	 */
	FSMimeType*					mime;
	
	/**
	 * Name of the entry.
	 */
	char*						name;
	
	/**
	 * Full absolute path to the entry.
	 */
	char*						path;
	
	/**
	 * Is the entry selected?
	 */
	int						selected;
	
	/**
	 * Is the entry a symbolic link?
	 */
	int						symlink;
	
	/**
	 * Base X/Y of the tile.
	 */
	int						baseX, baseY;
	
	/**
	 * Callback to open this entry; default action is taken if NULL.
	 * Return 0 if we should continue opening other selected files, -1 otherwise.
	 */
	int (*open)(GWMWindow *dv, struct DirEntry_*);
} DirEntry;

enum
{
	FILEOP_MOVE,
	FILEOP_COPY,
	FILEOP_MKDIR,
	FILEOP_RMDIR,
};

typedef struct FileOpObject_
{
	struct FileOpObject_*				next;
	
	/**
	 * Which operation to perform.
	 */
	int						op;
	
	/**
	 * Source path if applicable, or NULL.
	 */
	char*						src;
	
	/**
	 * Destination path. This stores the path for FILEOP_RMDIR.
	 */
	char*						dest;
	
	/**
	 * File size if applicable, or zero.
	 */
	size_t						size;
	
	/**
	 * File mode (if applicable).
	 */
	mode_t						mode;
} FileOpObject;

/**
 * A node on the history stack.
 */
typedef struct HistoryNode_
{
	struct HistoryNode_*				down;
	char*						location;
} HistoryNode;

/**
 * Directory view data.
 */
typedef struct
{
	/**
	 * Current list of entries.
	 */
	DirEntry*					ents;
	
	/**
	 * Current location.
	 */
	char*						location;
	
	/**
	 * Attached scrollbar.
	 */
	GWMScrollbar*					sbar;
	
	/**
	 * Total height needed to fit all icons.
	 */
	int						totalHeight;
	
	/**
	 * Scroll position.
	 */
	int						scroll;
	
	/**
	 * Current directory entry being edited (renamed) and the text field editing it.
	 */
	DirEntry*					editing;
	GWMTextField*					txtEdit;
	
	/**
	 * "back" and "forward" stacks.
	 */
	HistoryNode*					back;
	HistoryNode*					forward;
} DirViewData;

/**
 * Directory view class.
 */
typedef GWMWindow DirView;

/**
 * Create a new directory view.
 */
DirView* dvNew(GWMWindow *parent);

/**
 * Go to the specified directory. On success, this will change directory and emit an update event.
 * On error, it will display a message box. This will also return 0 on success and -1 on error.
 */
int dvGoTo(DirView *dv, const char *path);

/**
 * Get the absolute path of where the directory view is currently showing.
 */
const char* dvGetLocation(DirView *dv);

/**
 * Set the scrollbar attached to the directory view.
 */
void dvAttachScrollbar(DirView *dv, GWMScrollbar *sbar);

/**
 * Rename the currently-selected entry in the directory if any.
 */
void dvRename(DirView *dv);

/**
 * Create a new directory.
 */
void dvMakeDir(DirView *dv);

/**
 * Create a new file with the specified content.
 */
void dvMakeFile(DirView *dv, const void *content, size_t size);

/**
 * Open a terminal in the current directory.
 */
void dvTerminal(DirView *dv);

/**
 * Go back.
 */
void dvBack(DirView *dv);

/**
 * Go forward.
 */
void dvForward(DirView *dv);

/**
 * Show the properties dialog about the currently-selected element.
 */
void dvProps(DirView *dv);

/**
 * Cut the currently selected file(s).
 */
void dvCut(DirView *dv);

/**
 * Copy the currently selected file(s).
 */
void dvCopy(DirView *dv);

/**
 * Paste.
 */
void dvPaste(DirView *dv);

#endif
