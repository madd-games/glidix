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

#ifndef PLACES_H_
#define PLACES_H_

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
 * Defines a bookmark tile.
 */
typedef struct Bookmark_
{
	/**
	 * Links.
	 */
	struct Bookmark_*		prev;
	struct Bookmark_*		next;
	
	/**
	 * Label to display.
	 */
	char*				label;
	
	/**
	 * Absolute path to where this bookmark should lead.
	 */
	char*				path;
	
	/**
	 * Icon (16x16) to display by the bookmark.
	 */
	DDISurface*			icon;
	
	/**
	 * Y position that this bookmark has been drawn on.
	 */
	int				baseY;
} Bookmark;

/**
 * Defines a bookmark category.
 */
typedef struct Category_
{
	/**
	 * Links.
	 */
	struct Category_*		prev;
	struct Category_*		next;
	
	/**
	 * Label to display.
	 */
	char*				label;
	
	/**
	 * Bookmark list head.
	 */
	Bookmark*			bookmarks;
} Category;

/**
 * Places widget data.
 */
typedef struct
{
	/**
	 * List of categories.
	 */
	Category*			cats;
} PlacesData;

typedef GWMWindow Places;

/**
 * Create a new places widget.
 */
Places* plNew(GWMWindow *parent);

/**
 * Create a new category.
 */
Category* plNewCat(Places *pl, const char *label);

/**
 * Add a bookmark.
 */
void plAddBookmark(Category *cat, DDISurface *icon, const char *label, const char *path);

/**
 * Redraw places widget.
 */
void plRedraw(Places *pl);

#endif
