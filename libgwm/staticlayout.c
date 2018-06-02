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
 * STATIC LAYOUT
 * Lays out children by having statically-specified positions of each side of a child.
 */

typedef struct StaticChild_ StaticChild;
struct StaticChild_
{
	StaticChild *next;
	int left, right, top, bottom;

	GWMLayout *layout;
};

typedef struct
{
	StaticChild* firstChild;
	StaticChild* lastChild;
	
	int minWidth, minHeight;
	int prefWidth, prefHeight;
} StaticLayoutData;

static void staticMinSize(GWMLayout *layout, int *outWidth, int *outHeight)
{
	StaticLayoutData *data = (StaticLayoutData*) layout->data;
	*outWidth = data->minWidth;
	*outHeight = data->minHeight;
};

static void staticPrefSize(GWMLayout *layout, int *outWidth, int *outHeight)
{
	StaticLayoutData *data = (StaticLayoutData*) layout->data;
	*outWidth = data->prefWidth;
	*outHeight = data->prefHeight;
};

static void staticRun(GWMLayout *layout, int x, int y, int width, int height)
{
	StaticLayoutData *data = (StaticLayoutData*) layout->data;
	
	StaticChild *child;
	for (child=data->firstChild; child!=NULL; child=child->next)
	{
		int left = child->left;
		if (left < 0) left += width;
		
		int right = child->right;
		if (right < 0) right += width;
		
		int top = child->top;
		if (top < 0) top += height;
		
		int bottom = child->bottom;
		if (bottom < 0) bottom += height;

		child->layout->run(child->layout, left, top, right-left, bottom-top);
	};
};

GWMLayout* gwmCreateStaticLayout(int minWidth, int minHeight, int prefWidth, int prefHeight)
{
	GWMLayout *layout = gwmCreateAbstractLayout();
	StaticLayoutData *data = (StaticLayoutData*) malloc(sizeof(StaticLayoutData));
	
	data->firstChild = data->lastChild = NULL;
	data->minWidth = minWidth;
	data->minHeight = minHeight;
	data->prefWidth = prefWidth;
	data->prefHeight = prefHeight;
	
	layout->data = data;
	layout->getMinSize = staticMinSize;
	layout->getPrefSize = staticPrefSize;
	layout->run = staticRun;
	
	return layout;
};

void gwmDestroyStaticLayout(GWMLayout *layout)
{
	StaticLayoutData *data = (StaticLayoutData*) layout->data;
	
	while (data->firstChild != NULL)
	{
		StaticChild *child = data->firstChild;
		data->firstChild = child->next;
		
		free(child);
	};
	
	free(data);
	gwmDestroyAbstractLayout(layout);
};

void gwmStaticLayoutAddLayout(GWMLayout *st, GWMLayout *child, int left, int top, int right, int bottom)
{
	StaticLayoutData *data = (StaticLayoutData*) st->data;
	StaticChild *ch = (StaticChild*) malloc(sizeof(StaticChild));
	ch->next = NULL;
	ch->left = left;
	ch->top = top;
	ch->right = right;
	ch->bottom = bottom;
	ch->layout = child;
	
	if (data->lastChild == NULL)
	{
		data->firstChild = data->lastChild = ch;
	}
	else
	{
		data->lastChild->next = ch;
		data->lastChild = ch;
	};
};

void gwmStaticLayoutAddWindow(GWMLayout *st, GWMWindow *child, int left, int top, int right, int bottom)
{
	gwmStaticLayoutAddLayout(st, gwmCreateLeafLayout(child), left, top, right, bottom);
};
