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

#include <libgwm.h>
#include <libddi.h>
#include <stdlib.h>
#include <stdio.h>
#include <string.h>

/**
 * LEAF LAYOUT
 * Represents the leafs of a layout tree, i.e. the windows and the spacers, nested in other layouts.
 */

static void leafMinSize(GWMLayout *layout, int *outWidth, int *outHeight)
{
	GWMWindow *win = (GWMWindow*) layout->data;
	
	if (win == NULL)
	{
		*outWidth = 0;
		*outHeight = 0;
	}
	else if (win->layout != NULL)
	{
		win->layout->getMinSize(win->layout, outWidth, outHeight);
	}
	else if (win->getMinSize != NULL)
	{
		win->getMinSize(win, outWidth, outHeight);
	}
	else
	{
		*outWidth = win->canvas->width;
		*outHeight = win->canvas->height;
	};
};

static void leafPrefSize(GWMLayout *layout, int *outWidth, int *outHeight)
{
	GWMWindow *win = (GWMWindow*) layout->data;
	
	if (win == NULL)
	{
		*outWidth = 0;
		*outHeight = 0;
	}
	else if (win->layout != NULL)
	{
		win->layout->getPrefSize(win->layout, outWidth, outHeight);
	}
	else if (win->getPrefSize != NULL)
	{
		win->getPrefSize(win, outWidth, outHeight);
	}
	else
	{
		*outWidth = win->canvas->width;
		*outHeight = win->canvas->height;
	};
};

static void leafRun(GWMLayout *layout, int x, int y, int width, int height)
{
	GWMWindow *win = (GWMWindow*) layout->data;
	
	if (win == NULL)
	{
		// NOP
	}
	else if (win->layout != NULL)
	{
		gwmMoveWindow(win, x, y);
		gwmLayout(win, width, height);
	}
	else if (win->position != NULL)
	{
		win->position(win, x, y, width, height);
	}
	else
	{
		gwmMoveWindow(win, x, y);
	};
};

GWMLayout* gwmCreateLeafLayout(GWMWindow *win)
{
	GWMLayout *layout = gwmCreateAbstractLayout();
	layout->data = win;
	layout->getMinSize = leafMinSize;
	layout->getPrefSize = leafPrefSize;
	layout->run = leafRun;
	return layout;
};

void gwmDestroyLeafLayout(GWMLayout *leaf)
{
	gwmDestroyAbstractLayout(leaf);
};
