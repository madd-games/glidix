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

#include <libgwm.h>
#include <stdlib.h>
#include <stdio.h>
#include <string.h>

#define	TEXTFIELD_MIN_WIDTH	30
#define	TEXTFIELD_HEIGHT	20

typedef struct
{
	char			*text;
	size_t			textSize;
	off_t			cursorPos;
	int			focused;
	
	/**
	 * Selection range.
	 */
	off_t			selectStart;
	off_t			selectEnd;
	
	/**
	 * If the mouse button isn't down, this is -1. Otherwise, it's the position
	 * that was initally clicked. Used to define the selection range.
	 */
	off_t			clickPos;
} GWMTextFieldData;

void gwmRedrawTextField(GWMWindow *field)
{
	GWMTextFieldData *data = (GWMTextFieldData*) field->data;
	DDISurface *canvas = gwmGetWindowCanvas(field);
	
	static DDIColor normalBorderColor = {0, 0, 0, 0xFF};
	static DDIColor focusBorderColor = {0, 0xAA, 0, 0xFF};
	DDIColor *color = &normalBorderColor;
	if (data->focused)
	{
		color = &focusBorderColor;
	};

	static DDIColor background = {0xFF, 0xFF, 0xFF, 0xFF};
	ddiFillRect(canvas, 0, 0, canvas->width, canvas->height, color);
	ddiFillRect(canvas, 1, 1, canvas->width-2, canvas->height-2, &background);
	
	char buf[2];
	buf[1] = 0;
	
	off_t i;
	for (i=0; i<data->textSize; i++)
	{
		buf[0] = data->text[i];
		if ((i >= data->selectStart) && (i < data->selectEnd))
		{
			ddiFillRect(canvas, 3+8*i, TEXTFIELD_HEIGHT/2-4, 8, 8, GWM_COLOR_SELECTION);
		};
		ddiDrawText(canvas, 3+8*i, TEXTFIELD_HEIGHT/2-4, buf, NULL, NULL);
	};
	
	int cursorX = 3 + data->cursorPos * 8;
	static DDIColor black = {0, 0, 0, 0xFF};
	if (data->focused)
	{
		ddiFillRect(canvas, cursorX, 3, 1, canvas->height-6, &black);
	};
	
	gwmPostDirty();
};

static void gwmTextFieldDeleteSelection(GWMWindow *field)
{
	GWMTextFieldData *data = (GWMTextFieldData*) field->data;
	char *newBuffer = (char*) malloc(data->textSize - (data->selectEnd - data->selectStart));
	memcpy(newBuffer, data->text, data->selectStart);
	memcpy(&newBuffer[data->selectStart], &data->text[data->selectEnd], data->textSize - data->selectEnd);
	data->textSize -= (data->selectEnd - data->selectStart);
	data->cursorPos = data->selectStart;
	data->selectStart = data->selectEnd = 0;
	free(data->text);
	data->text = newBuffer;
};

void gwmTextFieldBackspace(GWMWindow *field)
{
	GWMTextFieldData *data = (GWMTextFieldData*) field->data;
	if (data->selectStart != data->selectEnd)
	{
		gwmTextFieldDeleteSelection(field);
		gwmRedrawTextField(field);
		return;
	};
	if (data->cursorPos == 0) return;
	
	char *newBuffer = (char*) malloc(data->textSize - 1);
	data->cursorPos--;
	data->textSize--;
	memcpy(newBuffer, data->text, data->cursorPos);
	memcpy(&newBuffer[data->cursorPos], &data->text[data->cursorPos+1], data->textSize - data->cursorPos);
	free(data->text);
	data->text = newBuffer;
	gwmRedrawTextField(field);
};

void gwmTextFieldInsert(GWMWindow *field, const char *str)
{
	GWMTextFieldData *data = (GWMTextFieldData*) field->data;
	if (data->selectStart != data->selectEnd)
	{
		gwmTextFieldDeleteSelection(field);
	};
	char *newBuffer = (char*) malloc(data->textSize + strlen(str));
	memcpy(newBuffer, data->text, data->cursorPos);
	memcpy(&newBuffer[data->cursorPos], str, strlen(str));
	memcpy(&newBuffer[data->cursorPos + strlen(str)], &data->text[data->cursorPos], data->textSize - data->cursorPos);
	free(data->text);
	data->text = newBuffer;
	data->textSize += strlen(str);
	data->cursorPos += strlen(str);
	gwmRedrawTextField(field);
};

int gwmTextFieldHandler(GWMEvent *ev, GWMWindow *field)
{
	GWMTextFieldData *data = (GWMTextFieldData*) field->data;
	off_t newCursorPos;
	char buf[2];
	switch (ev->type)
	{
	case GWM_EVENT_FOCUS_IN:
		data->focused = 1;
		gwmRedrawTextField(field);
		return 0;
	case GWM_EVENT_FOCUS_OUT:
		data->focused = 0;
		gwmRedrawTextField(field);
		return 0;
	case GWM_EVENT_DOWN:
		if (ev->scancode == GWM_SC_MOUSE_LEFT)
		{
			newCursorPos = (ev->x - 3) / 8;
			if (newCursorPos < 0) data->cursorPos = 0;
			else if (newCursorPos > data->textSize) data->cursorPos = data->textSize;
			else data->cursorPos = newCursorPos;
			data->clickPos = data->cursorPos;
			data->selectStart = 0;
			data->selectEnd = 0;
			gwmRedrawTextField(field);
		}
		else if (ev->keycode == GWM_KC_LEFT)
		{
			data->selectStart = data->selectEnd = 0;
			if (data->cursorPos != 0)
			{
				data->cursorPos--;
				gwmRedrawTextField(field);
			};
		}
		else if (ev->keycode == GWM_KC_RIGHT)
		{
			data->selectStart = data->selectEnd = 0;
			if (data->cursorPos != (off_t)data->textSize)
			{
				data->cursorPos++;
				gwmRedrawTextField(field);
			};
		}
		else if (ev->keycode == '\n')
		{
		}
		else if (ev->keycode == '\b')
		{
			gwmTextFieldBackspace(field);
		}
		else if (ev->keychar != 0)
		{
			sprintf(buf, "%c", ev->keychar);
			gwmTextFieldInsert(field, buf);
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
			newCursorPos = (ev->x - 3) / 8;
			if (newCursorPos < 0) newCursorPos = 0;
			else if (newCursorPos > data->textSize) newCursorPos = data->textSize;
			data->cursorPos = newCursorPos;
			if (newCursorPos < data->clickPos)
			{
				data->selectStart = newCursorPos;
				data->selectEnd = data->clickPos;
			}
			else
			{
				data->selectStart = data->clickPos;
				data->selectEnd = newCursorPos;
			};
			gwmRedrawTextField(field);
		};
		return 0;
	default:
		return gwmDefaultHandler(ev, field);
	};
};

GWMWindow *gwmCreateTextField(GWMWindow *parent, const char *text, int x, int y, int width, int flags)
{
	if (width < TEXTFIELD_MIN_WIDTH)
	{
		width = TEXTFIELD_MIN_WIDTH;
	};
	
	GWMWindow *field = (GWMWindow*) gwmCreateWindow(parent, "GWMTextField", x, y, width, TEXTFIELD_HEIGHT, 0);
	if (field == NULL) return NULL;
	
	GWMTextFieldData *data = (GWMTextFieldData*) malloc(sizeof(GWMTextFieldData));
	field->data = data;
	
	data->text = strdup(text);
	data->textSize = strlen(text);
	data->cursorPos = 0;
	data->focused = 0;
	
	data->selectStart = data->selectEnd = 0;
	data->clickPos = -1;
	
	gwmRedrawTextField(field);
	gwmSetEventHandler(field, gwmTextFieldHandler);
};

void gwmDestroyTextField(GWMWindow *field)
{
	GWMTextFieldData *data = (GWMTextFieldData*) field->data;
	free(data->text);
	free(data);
	gwmDestroyWindow(field);
};

size_t gwmGetTextFieldSize(GWMWindow *field)
{
	GWMTextFieldData *data = (GWMTextFieldData*) field->data;
	return data->textSize;
};

size_t gwmReadTextField(GWMWindow *field, char *buffer, off_t startPos, off_t endPos)
{
	GWMTextFieldData *data = (GWMTextFieldData*) field->data;
	off_t pos;
	size_t count = 0;
	for (pos=startPos; pos<endPos; pos++)
	{
		if ((pos < 0) || (pos > data->textSize))
		{
			break;
		};
		
		*buffer++ = data->text[pos];
	};
	
	*buffer = 0;
	return count;
};
