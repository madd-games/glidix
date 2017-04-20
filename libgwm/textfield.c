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
	int			flags;
	
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
	
	/**
	 * Pen used to draw the text.
	 */
	DDIPen*			pen;
	
	/**
	 * The right-click menu.
	 */
	GWMMenu*		menu;
	
	/**
	 * Accept callback.
	 */
	GWMTextFieldCallback	acceptCallback;
	void*			acceptParam;
} GWMTextFieldData;

void gwmRedrawTextField(GWMWindow *field)
{
	GWMTextFieldData *data = (GWMTextFieldData*) field->data;
	DDISurface *canvas = gwmGetWindowCanvas(field);
	
	static DDIColor transparent = {0, 0, 0, 0};
	static DDIColor normalBorderColor = {0, 0, 0, 0xFF};
	static DDIColor focusBorderColor = {0, 0xAA, 0, 0xFF};
	DDIColor *color = &normalBorderColor;
	if ((data->focused) && ((data->flags & GWM_TXT_DISABLED) == 0))
	{
		color = &focusBorderColor;
	};

	static DDIColor normalBackground = {0xFF, 0xFF, 0xFF, 0xFF};
	static DDIColor disabledBackground = {0x77, 0x77, 0x77, 0xFF};
	DDIColor *background = &normalBackground;
	if (data->flags & GWM_TXT_DISABLED)
	{
		background = &disabledBackground;
	};
	
	ddiFillRect(canvas, 0, 0, canvas->width, canvas->height, color);
	ddiFillRect(canvas, 1, 1, canvas->width-2, canvas->height-2, background);

	if (data->pen != NULL) ddiDeletePen(data->pen);
	data->pen = ddiCreatePen(&canvas->format, gwmGetDefaultFont(), 3, 2, canvas->width-3, canvas->height-3, 0, 0, NULL);
	if (data->pen != NULL)
	{
		ddiSetPenWrap(data->pen, 0);
		if (data->flags & GWM_TXT_MASKED) ddiPenSetMask(data->pen, 1);
		if (data->focused) ddiSetPenCursor(data->pen, data->cursorPos);

		char *buffer = malloc(data->textSize+1);
		if (data->selectStart == data->selectEnd)
		{
			memcpy(buffer, data->text, data->textSize);
			buffer[data->textSize] = 0;
			ddiWritePen(data->pen, buffer);
		}
		else
		{
			memcpy(buffer, data->text, data->selectStart);
			buffer[data->selectStart] = 0;
			ddiWritePen(data->pen, buffer);
			
			memcpy(buffer, &data->text[data->selectStart], data->selectEnd-data->selectStart);
			buffer[data->selectEnd-data->selectStart] = 0;
			ddiSetPenBackground(data->pen, GWM_COLOR_SELECTION);
			ddiWritePen(data->pen, buffer);
			ddiSetPenBackground(data->pen, &transparent);
			
			memcpy(buffer, &data->text[data->selectEnd], data->textSize-data->selectEnd);
			buffer[data->textSize-data->selectEnd] = 0;
			ddiWritePen(data->pen, buffer);
		};
		
		free(buffer);
		ddiExecutePen(data->pen, canvas);
	};
	
	gwmPostDirty(field);
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

void gwmTextFieldSelectWord(GWMWindow *field)
{
	GWMTextFieldData *data = (GWMTextFieldData*) field->data;
	if (data->textSize == 0) return;
	
	data->selectStart = data->cursorPos;
	data->selectEnd = data->cursorPos;
	
	int charClass;
	if (data->cursorPos == data->textSize)
	{
		charClass = gwmClassifyChar(data->text[data->cursorPos-1]);
		data->selectEnd--;
		data->selectStart--;
	}
	else
	{
		charClass = gwmClassifyChar(data->text[data->cursorPos]);
	};
	
	while (data->selectStart != 0)
	{
		if (gwmClassifyChar(data->text[data->selectStart-1]) != charClass)
		{
			break;
		};
		
		data->selectStart--;
	};
	
	while (data->selectEnd != (data->textSize-1))
	{
		if (gwmClassifyChar(data->text[data->selectEnd+1]) != charClass)
		{
			break;
		};
		
		data->selectEnd++;
	};
	
	data->selectEnd++;
	data->cursorPos = data->selectEnd;
};

int txtPaste(void *context)
{
	GWMWindow *field = (GWMWindow*) context;
	size_t size;
	char *text = gwmClipboardGetText(&size);
	if (text != NULL)
	{
		gwmTextFieldInsert(field, text);
		free(text);
	};

	gwmSetWindowFlags(field, GWM_WINDOW_MKFOCUSED);
	return 0;
};

int txtCut(void *context)
{
	GWMWindow *field = (GWMWindow*) context;
	GWMTextFieldData *data = (GWMTextFieldData*) field->data;
	
	if (data->selectStart != data->selectEnd)
	{
		gwmClipboardPutText(&data->text[data->selectStart], data->selectEnd - data->selectStart);
	};
	
	gwmTextFieldDeleteSelection(field);
	gwmRedrawTextField(field);
	gwmSetWindowFlags(field, GWM_WINDOW_MKFOCUSED);
	return 0;
};

int txtCopy(void *context)
{
	GWMWindow *field = (GWMWindow*) context;
	GWMTextFieldData *data = (GWMTextFieldData*) field->data;
	
	if (data->selectStart != data->selectEnd)
	{
		gwmClipboardPutText(&data->text[data->selectStart], data->selectEnd - data->selectStart);
	};

	gwmSetWindowFlags(field, GWM_WINDOW_MKFOCUSED);
	return 0;
};

int txtSelectAll(void *context)
{
	GWMWindow *field = (GWMWindow*) context;
	GWMTextFieldData *data = (GWMTextFieldData*) field->data;
	data->selectStart = 0;
	data->selectEnd = data->textSize;
	data->cursorPos = data->textSize;
	gwmRedrawTextField(field);
	gwmSetWindowFlags(field, GWM_WINDOW_MKFOCUSED);
	return 0;
};

int gwmTextFieldHandler(GWMEvent *ev, GWMWindow *field)
{
	GWMTextFieldData *data = (GWMTextFieldData*) field->data;
	off_t newCursorPos;
	int disabled = data->flags & GWM_TXT_DISABLED;
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
	case GWM_EVENT_DOUBLECLICK:
		gwmTextFieldSelectWord(field);
		gwmRedrawTextField(field);
		return 0;
	case GWM_EVENT_DOWN:
		if (ev->scancode == GWM_SC_MOUSE_LEFT)
		{
			newCursorPos = ddiPenCoordsToPos(data->pen, ev->x, ev->y);
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
		else if (ev->keymod & GWM_KM_CTRL)
		{
			if (ev->keycode == 'c')
			{
				txtCopy(field);
			}
			else if ((ev->keycode == 'x') && (!disabled))
			{
				txtCut(field);
			}
			else if ((ev->keycode == 'v') && (!disabled))
			{
				txtPaste(field);
			}
			else if (ev->keycode == 'a')
			{
				txtSelectAll(field);
			};
		}
		else if (ev->keycode == '\n')
		{
			if (data->acceptCallback != NULL)
			{
				return data->acceptCallback(data->acceptParam);
			};
		}
		else if ((ev->keycode == '\b') && (!disabled))
		{
			gwmTextFieldBackspace(field);
		}
		else if ((ev->keychar != 0) && (!disabled))
		{
			sprintf(buf, "%c", (char)ev->keychar);
			gwmTextFieldInsert(field, buf);
		};
		return 0;
	case GWM_EVENT_UP:
		if (ev->scancode == GWM_SC_MOUSE_LEFT)
		{
			data->clickPos = -1;
		}
		else if (ev->scancode == GWM_SC_MOUSE_RIGHT)
		{
			gwmOpenMenu(data->menu, field, ev->x, ev->y);
		};
		return 0;
	case GWM_EVENT_MOTION:
		if (data->clickPos != -1)
		{
			newCursorPos = ddiPenCoordsToPos(data->pen, ev->x, ev->y);
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
	
	GWMWindow *field = gwmCreateWindow(parent, "GWMTextField", x, y, width, TEXTFIELD_HEIGHT, 0);
	if (field == NULL) return NULL;
	
	GWMTextFieldData *data = (GWMTextFieldData*) malloc(sizeof(GWMTextFieldData));
	field->data = data;
	
	data->text = strdup(text);
	data->textSize = strlen(text);
	data->cursorPos = 0;
	data->focused = 0;
	data->flags = flags;
	
	data->selectStart = data->selectEnd = 0;
	data->clickPos = -1;
	data->pen = NULL;
	
	data->acceptCallback = NULL;
	
	data->menu = gwmCreateMenu();
	gwmMenuAddEntry(data->menu, "Cut", txtCut, field);
	gwmMenuAddEntry(data->menu, "Copy", txtCopy, field);
	gwmMenuAddEntry(data->menu, "Paste", txtPaste, field);
	gwmMenuAddSeparator(data->menu);
	gwmMenuAddEntry(data->menu, "Select All", txtSelectAll, field);
	
	gwmSetEventHandler(field, gwmTextFieldHandler);
	gwmRedrawTextField(field);
	gwmSetWindowCursor(field, GWM_CURSOR_TEXT);
	
	return field;
};

void gwmDestroyTextField(GWMWindow *field)
{
	GWMTextFieldData *data = (GWMTextFieldData*) field->data;
	free(data->text);
	free(data);
	gwmDestroyMenu(data->menu);
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
		if ((pos < 0) || (pos >= (off_t)data->textSize))
		{
			break;
		};
		
		*buffer++ = data->text[pos];
		count++;
	};
	
	*buffer = 0;
	return count;
};

void gwmResizeTextField(GWMWindow *field, int width)
{
	if (width < TEXTFIELD_MIN_WIDTH)
	{
		width = TEXTFIELD_MIN_WIDTH;
	};
	gwmResizeWindow(field, width, TEXTFIELD_HEIGHT);
	gwmRedrawTextField(field);
};

void gwmWriteTextField(GWMWindow *field, const char *newText)
{
	GWMTextFieldData *data = (GWMTextFieldData*) field->data;
	free(data->text);
	data->text = strdup(newText);
	data->textSize = strlen(newText);
	data->selectStart = data->selectEnd = 0;
	data->cursorPos = 0;
	gwmRedrawTextField(field);
};

void gwmSetTextFieldAcceptCallback(GWMWindow *field, GWMTextFieldCallback callback, void *param)
{
	GWMTextFieldData *data = (GWMTextFieldData*) field->data;
	data->acceptCallback = callback;
	data->acceptParam = param;
};

void gwmTextFieldSelectAll(GWMWindow *field)
{
	GWMTextFieldData *data = (GWMTextFieldData*) field->data;
	data->selectStart = 0;
	data->selectEnd = data->textSize;
	data->cursorPos = data->textSize;
	gwmRedrawTextField(field);
};
