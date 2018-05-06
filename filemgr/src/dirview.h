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
	 * Base X/Y of the tile.
	 */
	int						baseX, baseY;
} DirEntry;

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

#endif
