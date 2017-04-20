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

#define	TAG_FG				(1 << 0)
#define	TAG_BG				(1 << 1)

GWMTag gwmTagSelection = {
	TAG_FG | TAG_BG,
	{0x00, 0x00, 0x00, 0xFF},	// fg
	{0x00, 0xAA, 0x00, 0xFF}	// bg
};

typedef struct TextSegment_
{
	struct TextSegment_*		next;
	size_t				size;
	GWMTag**			tags;
	size_t				numTags;
} TextSegment;

typedef struct
{
	char*				text;
	size_t				textSize;
	DDIFont*			font;
	DDIColor			txtbackground;
	DDIColor			txtforeground;
	DDIColor			background;
	int				flags;
	DDIPen*				pen;
	int				active;
	int				scrollX;
	int				scrollY;
	TextSegment*			segFirst;
	TextSegment*			segLast;
	off_t				cursorPos;
	off_t				clickPos;
} TextAreaData;

GWMTag* gwmCreateTag()
{
	GWMTag *tag = (GWMTag*) malloc(sizeof(GWMTag));
	tag->flags = 0;
	return tag;
};

void gwmDestroyTag(GWMTag *tag)
{
	free(tag);
};

void gwmSetTagForeground(GWMTag *tag, DDIColor *fg)
{
	if (fg == NULL)
	{
		tag->flags &= ~TAG_FG;
	}
	else
	{
		tag->flags |= TAG_FG;
		memcpy(&tag->fg, fg, sizeof(DDIColor));
	};
};

void gwmSetTagBackground(GWMTag *tag, DDIColor *bg)
{
	if (bg == NULL)
	{
		tag->flags &= ~TAG_BG;
	}
	else
	{
		tag->flags |= TAG_BG;
		memcpy(&tag->bg, bg, sizeof(DDIColor));
	};
};

static void redrawTextArea(GWMWindow *area)
{
	TextAreaData *data = (TextAreaData*) area->data;
	DDISurface *canvas = gwmGetWindowCanvas(area);
	
	static DDIColor normalBorderColor = {0, 0, 0, 0xFF};
	static DDIColor focusBorderColor = {0, 0xAA, 0, 0xFF};
	static DDIColor disabledBackground = {0x77, 0x77, 0x77, 0xFF};
	
	if (data->active)
	{
		ddiFillRect(canvas, 0, 0, canvas->width, canvas->height, &focusBorderColor);
	}
	else
	{
		ddiFillRect(canvas, 0, 0, canvas->width, canvas->height, &normalBorderColor);
	};
	
	if (data->flags & GWM_TEXTAREA_DISABLED)
	{
		ddiFillRect(canvas, 1, 1, canvas->width-2, canvas->height-2, &disabledBackground);
	}
	else
	{
		ddiFillRect(canvas, 1, 1, canvas->width-2, canvas->height-2, &data->background);
	};
	
	if (data->pen != NULL) ddiDeletePen(data->pen);
	data->pen = ddiCreatePen(&canvas->format, data->font, 2, 2, canvas->width-4, canvas->height-4,
				data->scrollX, data->scrollY, NULL);
	
	if (data->pen != NULL)
	{
		if (data->active) ddiSetPenCursor(data->pen, data->cursorPos);
		
		TextSegment *seg;
		char *scan = data->text;
		
		for (seg=data->segFirst; seg!=NULL; seg=seg->next)
		{
			DDIColor *fg = &data->txtforeground;
			DDIColor *bg = &data->txtbackground;
			
			size_t i;
			for (i=0; i<seg->numTags; i++)
			{
				if (seg->tags[i]->flags & TAG_FG)
				{
					fg = &seg->tags[i]->fg;
				};
				
				if (seg->tags[i]->flags & TAG_BG)
				{
					bg = &seg->tags[i]->bg;
				};
			};
			
			ddiSetPenColor(data->pen, fg);
			ddiSetPenBackground(data->pen, bg);
			
			char c = scan[seg->size];
			scan[seg->size] = 0;
			ddiWritePen(data->pen, scan);
			scan[seg->size] = c;
			
			scan += seg->size;
		};
		
		ddiExecutePen(data->pen, canvas);
	};

	gwmPostDirty(area);
};

static int areaHandler(GWMEvent *ev, GWMWindow *area)
{
	TextAreaData *data = (TextAreaData*) area->data;
	off_t newCursorPos;
	int disabled = data->flags & GWM_TEXTAREA_DISABLED;
	off_t selPos;
	size_t selSize;
	char buf[2];
	
	switch (ev->type)
	{
	case GWM_EVENT_FOCUS_IN:
		data->active = 1;
		redrawTextArea(area);
		return 0;
	case GWM_EVENT_FOCUS_OUT:
		data->active = 0;
		redrawTextArea(area);
		return 0;
	case GWM_EVENT_DOWN:
		if (ev->scancode == GWM_SC_MOUSE_LEFT)
		{
			newCursorPos = ddiPenCoordsToPos(data->pen, ev->x, ev->y);
			if (newCursorPos < 0) newCursorPos = 0;
			else if (newCursorPos > data->textSize) newCursorPos = data->textSize;
			data->cursorPos = newCursorPos;
			data->clickPos = newCursorPos;
			gwmTagTextArea(area, GWM_TAG_SELECTION, 0, data->textSize, GWM_STACK_REMOVE);
		}
		else if (ev->keycode == GWM_KC_LEFT)
		{
			if (data->cursorPos != 0)
			{
				data->cursorPos--;
			};
			gwmTagTextArea(area, GWM_TAG_SELECTION, 0, data->textSize, GWM_STACK_REMOVE);
		}
		else if (ev->keycode == GWM_KC_RIGHT)
		{
			if (data->cursorPos != (off_t)data->textSize)
			{
				data->cursorPos++;
			};
			gwmTagTextArea(area, GWM_TAG_SELECTION, 0, data->textSize, GWM_STACK_REMOVE);
		}
		else if ((ev->keycode == '\b') && (!disabled))
		{
			gwmGetTextAreaTagRange(area, GWM_TAG_SELECTION, 0, &selPos, &selSize);
			if (selSize != 0)
			{
				gwmTextAreaErase(area, selPos, selSize);
			}
			else
			{
				gwmTextAreaErase(area, data->cursorPos-1, 1);
			};
		}
		else if ((ev->keychar != 0) && (!disabled))
		{
			gwmGetTextAreaTagRange(area, GWM_TAG_SELECTION, 0, &selPos, &selSize);
			if (selSize != 0)
			{
				gwmTextAreaErase(area, selPos, selSize);
			};
			
			sprintf(buf, "%c", (char)ev->keychar);
			gwmTextAreaInsert(area, data->cursorPos, buf);
			data->cursorPos++;
			redrawTextArea(area);
		};
		return 0;
	case GWM_EVENT_UP:
		if (ev->scancode == GWM_SC_MOUSE_LEFT)
		{
			data->clickPos = -1;
		};
		return 0;
	case GWM_EVENT_MOTION:
		if (data->clickPos != -1)
		{
			newCursorPos = ddiPenCoordsToPos(data->pen, ev->x, ev->y);
			if (newCursorPos < 0) newCursorPos = 0;
			else if (newCursorPos > data->textSize) newCursorPos = data->textSize;
			
			if (data->cursorPos != newCursorPos)
			{
				data->cursorPos = newCursorPos;
				gwmTagTextArea(area, GWM_TAG_SELECTION, 0, data->textSize, GWM_STACK_REMOVE);
				if (newCursorPos < data->clickPos)
				{
					gwmTagTextArea(area, GWM_TAG_SELECTION, newCursorPos, data->clickPos - newCursorPos, GWM_STACK_ABOVE);
				}
				else
				{
					gwmTagTextArea(area, GWM_TAG_SELECTION, data->clickPos, newCursorPos - data->clickPos, GWM_STACK_ABOVE);
				};
			};
		};
		return 0;
	default:
		return gwmDefaultHandler(ev, area);
	};
};

GWMWindow* gwmCreateTextArea(GWMWindow *parent, int x, int y, int width, int height, int flags)
{
	GWMWindow *area = gwmCreateWindow(parent, "GWMTextArea", x, y, width, height, 0);
	if (area == NULL) return NULL;
	
	static DDIColor background = {0, 0, 0, 0};
	static DDIColor foreground = {0, 0, 0, 0xFF};
	static DDIColor white = {0xFF, 0xFF, 0xFF, 0xFF};
	
	TextAreaData *data = (TextAreaData*) malloc(sizeof(TextAreaData));
	data->text = strdup("");
	data->textSize = 0;
	data->font = gwmGetDefaultFont();
	memcpy(&data->txtbackground, &background, sizeof(DDIColor));
	memcpy(&data->txtforeground, &foreground, sizeof(DDIColor));
	memcpy(&data->background, &white, sizeof(DDIColor));
	data->flags = flags;
	data->pen = NULL;
	data->active = 0;
	data->scrollX = data->scrollY = 0;
	
	TextSegment *seg = (TextSegment*) malloc(sizeof(TextSegment));
	seg->next = NULL;
	seg->size = 0;
	seg->tags = NULL;
	seg->numTags = 0;
	
	data->segFirst = data->segLast = seg;
	data->cursorPos = 0;
	data->clickPos = -1;
	
	area->data = data;
	gwmSetEventHandler(area, areaHandler);
	gwmSetWindowCursor(area, GWM_CURSOR_TEXT);
	redrawTextArea(area);
	return area;
};

void gwmDestroyTextArea(GWMWindow *area)
{
	TextAreaData *data = (TextAreaData*) area->data;
	free(data->text);
	free(data);
	gwmDestroyWindow(area);
};

void gwmAppendTextArea(GWMWindow *area, const char *text)
{
	TextAreaData *data = (TextAreaData*) area->data;
	
	size_t newSize = data->textSize + strlen(text);
	data->text = realloc(data->text, newSize+1);
	strcpy(&data->text[data->textSize], text);
	data->text[newSize] = 0;
	data->textSize = newSize;
	
	data->segLast->size += strlen(text);
	
	redrawTextArea(area);
};

static off_t getTagInSegment(TextSegment *seg, GWMTag *tag)
{
	off_t i;
	for (i=0; i<seg->numTags; i++)
	{
		if (seg->tags[i] == tag) return i;
	};
	
	return -1;
};

static void splitSegment(TextSegment *seg, size_t newSize)
{
	TextSegment *new = (TextSegment*) malloc(sizeof(TextSegment));
	new->next = seg->next;
	new->size = seg->size - newSize;
	new->tags = (GWMTag**) malloc(sizeof(void*) * seg->numTags);
	memcpy(new->tags, seg->tags, sizeof(void*) * seg->numTags);
	new->numTags = seg->numTags;
	
	seg->size = newSize;
	seg->next = new;
};

static void addTagToSegment(TextSegment *seg, GWMTag *tag, int stacking)
{
	off_t currentPos = getTagInSegment(seg, tag);
	if (currentPos != -1)
	{
		memcpy(&seg->tags[currentPos], &seg->tags[currentPos+1], (seg->numTags-1) * sizeof(void*));
		seg->numTags--;
		seg->tags = (GWMTag**) realloc(seg->tags, seg->numTags * sizeof(void*));
	};
	
	if (stacking == GWM_STACK_ABOVE)
	{
		size_t index = seg->numTags++;
		seg->tags = (GWMTag**) realloc(seg->tags, seg->numTags * sizeof(void*));
		seg->tags[index] = tag;
	}
	else if (stacking == GWM_STACK_BELOW)
	{
		GWMTag **newList = (GWMTag**) malloc((seg->numTags+1) * sizeof(void*));
		memcpy(&newList[1], seg->tags, sizeof(void*) * seg->numTags);
		newList[0] = tag;
		seg->numTags++;
		free(seg->tags);
		seg->tags = newList;
	};
	
	// otherwise GWM_STACK_REMOVE, and we already removed above
};

void gwmTagTextArea(GWMWindow *area, GWMTag *tag, size_t pos, size_t len, int stacking)
{
	TextAreaData *data = (TextAreaData*) area->data;
	TextSegment *scan = data->segFirst;
	while (scan != NULL)
	{
		if (scan->size <= pos)
		{
			pos -= scan->size;
			scan = scan->next;
		}
		else
		{
			break;
		};
	};
	
	if (scan == NULL) return;
	
	if (pos > 0)
	{
		splitSegment(scan, pos);
		scan = scan->next;
	};
	
	while (len != 0)
	{
		if (scan == NULL) break;
		
		if (len < scan->size)
		{
			splitSegment(scan, len);
		};
		
		addTagToSegment(scan, tag, stacking);
		
		len -= scan->size;
		scan = scan->next;
	};
	
	redrawTextArea(area);
};

void gwmTextAreaInsert(GWMWindow *area, off_t pos, const char *text)
{
	TextAreaData *data = (TextAreaData*) area->data;
	if (pos < 0) pos = 0;
	if (pos > (off_t)data->textSize) pos = (off_t)data->textSize;
	
	char *newText = (char*) malloc(data->textSize + strlen(text) + 1);
	memcpy(newText, data->text, pos);
	strcpy(&newText[pos], text);
	memcpy(&newText[pos + strlen(text)], &data->text[pos], data->textSize - pos);
	data->textSize += strlen(text);
	newText[data->textSize] = 0;
	
	free(data->text);
	data->text = newText;
	
	// extend the segment that contains this text
	TextSegment *scan = data->segFirst;
	while (scan != NULL)
	{
		if (scan->size < pos)
		{
			pos -= scan->size;
			scan = scan->next;
		}
		else
		{
			break;
		};
	};
	
	if (scan == NULL)
	{
		redrawTextArea(area);
		return;
	};
	
	scan->size += strlen(text);
	
	redrawTextArea(area);
};

static void deleteSegment(TextAreaData *data, TextSegment *prev, TextSegment *seg)
{
	if (seg->next == NULL)
	{
		seg->size = 0;
		return;
	};
	
	if (prev == NULL)
	{
		data->segFirst = seg->next;
	}
	else
	{
		prev->next = seg->next;
	};
	
	free(seg->tags);
	free(seg);
};

void gwmTextAreaErase(GWMWindow *area, off_t pos, size_t len)
{
	TextAreaData *data = (TextAreaData*) area->data;
	if (pos < 0) pos = 0;
	if (pos > (off_t)data->textSize) pos = (off_t)data->textSize;
	if ((pos + len) > data->textSize) len = data->textSize - pos;
	
	char *newText = (char*) malloc(data->textSize - len + 1);
	memcpy(newText, data->text, pos);
	memcpy(&newText[pos], &data->text[pos + len], data->textSize - pos - len);
	newText[data->textSize - len] = 0;
	
	data->textSize -= len;
	free(data->text);
	data->text = newText;
	
	// move the cursor
	if (data->cursorPos >= (pos + len))
	{
		data->cursorPos -= len;
	}
	else if (data->cursorPos >= pos)
	{
		data->cursorPos = pos;
	};
	
	// remove segment or parts of segment on the way
	TextSegment *scan = data->segFirst;
	TextSegment *prev = NULL;
	while (scan != NULL)
	{
		if (scan->size <= pos)
		{
			pos -= scan->size;
			prev = scan;
			scan = scan->next;
		}
		else
		{
			break;
		};
	};
	
	if (scan == NULL)
	{
		redrawTextArea(area);
		return;
	};
	
	if (pos > 0)
	{
		splitSegment(scan, pos);
		prev = scan;
		scan = scan->next;
	};
	
	while (len != 0)
	{
		if (scan == NULL) break;
		
		if (len < scan->size)
		{
			splitSegment(scan, len);
		};
		
		TextSegment *next = scan->next;
		len -= scan->size;
		
		deleteSegment(data, prev, scan);
		scan = next;
	};
	
	redrawTextArea(area);
};

void gwmGetTextAreaTagRange(GWMWindow *area, GWMTag *tag, off_t after, off_t *outPos, size_t *outLen)
{
	TextAreaData *data = (TextAreaData*) area->data;
	TextSegment *scan = data->segFirst;
	*outLen = 0;
	
	off_t pos = 0;
	while (scan != NULL)
	{
		if (scan->size <= after)
		{
			after -= scan->size;
			pos += scan->size;
			scan = scan->next;
		}
		else
		{
			break;
		};
	};
	
	if (scan == NULL) return;
	
	while (scan != NULL)
	{
		if (getTagInSegment(scan, tag) != -1)
		{
			*outPos = pos;
			
			while (scan != NULL)
			{
				if (getTagInSegment(scan, tag) == -1)
				{
					return;
				};
				
				(*outLen) += scan->size;
				scan = scan->next;
			};
			
			return;
		};
		
		pos += scan->size;
		scan = scan->next;
	};
};

void gwmSetTextAreaStyle(GWMWindow *area, DDIFont *font, DDIColor *background, DDIColor *txtbg, DDIColor *txtfg)
{
	TextAreaData *data = (TextAreaData*) area->data;
	if (font != NULL) data->font = font;
	if (background != NULL) memcpy(&data->background, background, sizeof(DDIColor));
	if (txtbg != NULL) memcpy(&data->txtbackground, txtbg, sizeof(DDIColor));
	if (txtfg != NULL) memcpy(&data->txtforeground, txtfg, sizeof(DDIColor));
	redrawTextArea(area);
};

void gwmSetTextAreaFlags(GWMWindow *area, int flags)
{
	TextAreaData *data = (TextAreaData*) area->data;
	data->flags = flags;
	redrawTextArea(area);
};

size_t gwmGetTextAreaLen(GWMWindow *area)
{
	TextAreaData *data = (TextAreaData*) area->data;
	return data->textSize;
};

void gwmReadTextArea(GWMWindow *area, off_t pos, size_t len, char *buffer)
{
	TextAreaData *data = (TextAreaData*) area->data;
	if (pos < 0) pos = 0;
	if (pos > (off_t)data->textSize) pos = (off_t)data->textSize;
	if ((pos + len) > data->textSize) len = data->textSize - pos;
	memcpy(buffer, &data->text[pos], len);
	buffer[len] = 0;
};
