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

#define	TEXTFIELD_MIN_WIDTH	100
#define	TEXTFIELD_HEIGHT	20

typedef struct
{
	char			*text;
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
	 * Icon or NULL.
	 */
	DDISurface*		icon;
} GWMTextFieldData;

int gwmTextFieldHandler(GWMEvent *ev, GWMWindow *field, void *context);

void gwmRedrawTextField(GWMWindow *field)
{
	GWMTextFieldData *data = (GWMTextFieldData*) gwmGetData(field, gwmTextFieldHandler);
	DDISurface *canvas = gwmGetWindowCanvas(field);
	
	static DDIColor transparent = {0, 0, 0, 0};
	static DDIColor normalBorderColor = {0, 0, 0, 0xFF};
	static DDIColor focusBorderColor = {0, 0xAA, 0, 0xFF};
	DDIColor *color = &normalBorderColor;
	if ((data->focused) && ((data->flags & GWM_TXT_DISABLED) == 0))
	{
		color = &focusBorderColor;
	};

	static DDIColor disabledBackground = {0x77, 0x77, 0x77, 0xFF};
	DDIColor *background = GWM_COLOR_EDITOR;
	if (data->flags & GWM_TXT_DISABLED)
	{
		background = &disabledBackground;
	};
	
	ddiFillRect(canvas, 0, 0, canvas->width, canvas->height, color);
	ddiFillRect(canvas, 1, 1, canvas->width-2, canvas->height-2, background);

	if (data->pen != NULL) ddiDeletePen(data->pen);
	
	int penX = 3;
	if (data->icon != NULL)
	{
		ddiBlit(data->icon, 0, 0, canvas, 2, 2, 16, 16);
		penX = 20;
	};
	
	data->pen = ddiCreatePen(&canvas->format, gwmGetDefaultFont(), penX, 2, canvas->width-3, canvas->height-3, 0, 0, NULL);
	if (data->pen != NULL)
	{
		ddiSetPenWrap(data->pen, 0);
		if (data->flags & GWM_TXT_MASKED) ddiPenSetMask(data->pen, 1);
		if (data->focused) ddiSetPenCursor(data->pen, data->cursorPos);
		
		if (data->selectStart == data->selectEnd)
		{
			ddiWritePen(data->pen, data->text);
		}
		else
		{
			char *buffer = (char*) malloc(strlen(data->text)+1);
			buffer[0] = 0;
			
			const char *scan = data->text;
			
			// first print the text before selection
			size_t count = data->selectStart;
			while (count--)
			{
				long codepoint = ddiReadUTF8(&scan);
				if (codepoint == 0) break;
				ddiWriteUTF8(&buffer[strlen(buffer)], codepoint);
			};
			
			ddiWritePen(data->pen, buffer);
			
			// now the selection part
			ddiSetPenBackground(data->pen, GWM_COLOR_SELECTION);
			buffer[0] = 0;
			count = data->selectEnd - data->selectStart;
			while (count--)
			{
				long codepoint = ddiReadUTF8(&scan);
				if (codepoint == 0) break;
				ddiWriteUTF8(&buffer[strlen(buffer)], codepoint);
			};
			ddiWritePen(data->pen, buffer);
			
			// write the rest
			ddiSetPenBackground(data->pen, &transparent);
			ddiWritePen(data->pen, scan);
			
			free(buffer);
		};
		
		ddiExecutePen(data->pen, canvas);
	};
	
	gwmPostDirty(field);
};

static void gwmTextFieldDeleteSelection(GWMWindow *field)
{
	GWMTextFieldData *data = (GWMTextFieldData*) gwmGetData(field, gwmTextFieldHandler);
	
	char *newBuffer = (char*) malloc(strlen(data->text)+1);
	newBuffer[0] = 0;
	
	const char *scan = data->text;
	
	// copy the part before selection
	size_t count = data->selectStart;
	while (count--)
	{
		long codepoint = ddiReadUTF8(&scan);
		if (codepoint == 0) break;
		ddiWriteUTF8(&newBuffer[strlen(newBuffer)], codepoint);
	};
	
	// skip selection
	count = data->selectEnd - data->selectStart;
	while (count--)
	{
		ddiReadUTF8(&scan);
	};
	
	// write the part after selection
	while (1)
	{
		long codepoint = ddiReadUTF8(&scan);
		if (codepoint == 0) break;
		ddiWriteUTF8(&newBuffer[strlen(newBuffer)], codepoint);
	};
	
	// update
	data->cursorPos = data->selectStart;
	data->selectStart = data->selectEnd = 0;
	free(data->text);
	data->text = newBuffer;
};

void gwmTextFieldBackspace(GWMWindow *field)
{
	GWMTextFieldData *data = (GWMTextFieldData*) gwmGetData(field, gwmTextFieldHandler);
	if (data->selectStart != data->selectEnd)
	{
		gwmTextFieldDeleteSelection(field);
		gwmRedrawTextField(field);
		return;
	};
	
	if (data->cursorPos == 0) return;

	char *newBuffer = (char*) malloc(strlen(data->text) + 1);
	newBuffer[0] = 0;
	
	const char *scan = data->text;
	
	// copy the data before the cursor except the last one
	// (the case where cursorPos == 0 is already handled above)
	size_t count = data->cursorPos - 1;
	while (count--)
	{
		long codepoint = ddiReadUTF8(&scan);
		if (codepoint == 0) break;
		ddiWriteUTF8(&newBuffer[strlen(newBuffer)], codepoint);
	};
	
	// discard one character
	ddiReadUTF8(&scan);
	
	// write out the rest
	while (1)
	{
		long codepoint = ddiReadUTF8(&scan);
		if (codepoint == 0) break;
		ddiWriteUTF8(&newBuffer[strlen(newBuffer)], codepoint);
	};
	
	free(data->text);
	data->text = newBuffer;
	data->cursorPos--;
	
	gwmRedrawTextField(field);
};

void gwmTextFieldInsert(GWMWindow *field, const char *str)
{
	GWMTextFieldData *data = (GWMTextFieldData*) gwmGetData(field, gwmTextFieldHandler);
	if (data->selectStart != data->selectEnd)
	{
		gwmTextFieldDeleteSelection(field);
	};
	
	char *newBuffer = (char*) malloc(strlen(data->text) + strlen(str) + 1);
	newBuffer[0] = 0;
	
	const char *scan = data->text;
	
	// copy up to the cursor first
	size_t count = data->cursorPos;
	while (count--)
	{
		long codepoint = ddiReadUTF8(&scan);
		if (codepoint == 0) break;
		ddiWriteUTF8(&newBuffer[strlen(newBuffer)], codepoint);
	};
	
	// now copy the string, incrementing the cursor as necessary
	while (1)
	{
		long codepoint = ddiReadUTF8(&str);
		if (codepoint == 0) break;
		ddiWriteUTF8(&newBuffer[strlen(newBuffer)], codepoint);
		data->cursorPos++;
	};
	
	// finish off
	strcat(newBuffer, scan);
	
	free(data->text);
	data->text = newBuffer;
	
	gwmRedrawTextField(field);
};

void gwmTextFieldSelectWord(GWMWindow *field)
{
	GWMTextFieldData *data = (GWMTextFieldData*) gwmGetData(field, gwmTextFieldHandler);
	
	const char *scan = data->text;
	int lastClass = -1;
	off_t lastPos;
	
	// read up to the cursor position, classifying characters on the way, remembering each position where
	// the classification changed.
	size_t count = data->cursorPos;
	off_t curPos = 0;
	while (count--)
	{
		long codepoint = ddiReadUTF8(&scan);
		int cls = gwmClassifyChar(codepoint);
		
		if (cls != lastClass)
		{
			lastClass = cls;
			lastPos = curPos;
		};
		
		curPos++;
	};
	
	// figure out where to stop
	off_t endPos = curPos;
	while (1)
	{
		long codepoint = ddiReadUTF8(&scan);
		if (codepoint == 0) break;
		int cls = gwmClassifyChar(codepoint);
		
		if (lastClass == -1)
		{
			lastClass = cls;
			lastPos = endPos;
		};
		
		if (cls != lastClass) break;
		endPos++;
	};
	
	// results
	if (lastClass == -1)
	{
		data->selectStart = data->selectEnd = 0;
	}
	else
	{
		data->selectStart = lastPos;
		data->selectEnd = endPos;
	};
	
	gwmRedrawTextField(field);
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
	GWMTextFieldData *data = (GWMTextFieldData*) gwmGetData(field, gwmTextFieldHandler);
	
	if (data->selectStart != data->selectEnd)
	{
		char *temp = (char*) malloc(strlen(data->text) + 1);
		temp[0] = 0;
		
		const char *scan = data->text;
		
		// skip the text before selection
		size_t count = data->selectStart;
		while (count--)
		{
			ddiReadUTF8(&scan);
		};
		
		// now copy selection into buffer
		count = data->selectEnd - data->selectStart;
		while (count--)
		{
			long codepoint = ddiReadUTF8(&scan);
			if (codepoint == 0) break;
			ddiWriteUTF8(&temp[strlen(temp)], codepoint);
		};
		
		// store in the clipboard
		gwmClipboardPutText(temp, strlen(temp));
		free(temp);
	};
	
	gwmTextFieldDeleteSelection(field);
	gwmRedrawTextField(field);
	gwmSetWindowFlags(field, GWM_WINDOW_MKFOCUSED);

	return 0;
};

int txtCopy(void *context)
{
	GWMWindow *field = (GWMWindow*) context;
	GWMTextFieldData *data = (GWMTextFieldData*) gwmGetData(field, gwmTextFieldHandler);
	
	if (data->selectStart != data->selectEnd)
	{
		char *temp = (char*) malloc(strlen(data->text) + 1);
		temp[0] = 0;
		
		const char *scan = data->text;
		
		// skip the text before selection
		size_t count = data->selectStart;
		while (count--)
		{
			ddiReadUTF8(&scan);
		};
		
		// now copy selection into buffer
		count = data->selectEnd - data->selectStart;
		while (count--)
		{
			long codepoint = ddiReadUTF8(&scan);
			if (codepoint == 0) break;
			ddiWriteUTF8(&temp[strlen(temp)], codepoint);
		};
		
		// store in the clipboard
		gwmClipboardPutText(temp, strlen(temp));
		free(temp);
	};
	
	gwmSetWindowFlags(field, GWM_WINDOW_MKFOCUSED);
	return 0;
};

int txtSelectAll(void *context)
{
	GWMWindow *field = (GWMWindow*) context;
	GWMTextFieldData *data = (GWMTextFieldData*) gwmGetData(field, gwmTextFieldHandler);
	data->selectStart = 0;
	data->selectEnd = ddiCountUTF8(data->text);
	data->cursorPos = data->selectEnd;
	gwmRedrawTextField(field);
	gwmSetWindowFlags(field, GWM_WINDOW_MKFOCUSED);
	return 0;
};

int gwmTextFieldHandler(GWMEvent *ev, GWMWindow *field, void *context)
{
	GWMTextFieldData *data = (GWMTextFieldData*) gwmGetData(field, gwmTextFieldHandler);
	off_t newCursorPos;
	int disabled = data->flags & GWM_TXT_DISABLED;
	char buf[9];
	
	switch (ev->type)
	{
	case GWM_EVENT_FOCUS_IN:
		data->focused = 1;
		gwmRedrawTextField(field);
		return GWM_EVSTATUS_OK;
	case GWM_EVENT_FOCUS_OUT:
		data->focused = 0;
		gwmRedrawTextField(field);
		return GWM_EVSTATUS_OK;
	case GWM_EVENT_DOUBLECLICK:
		gwmTextFieldSelectWord(field);
		gwmRedrawTextField(field);
		return GWM_EVSTATUS_OK;
	case GWM_EVENT_DOWN:
		if (ev->keycode == GWM_KC_MOUSE_LEFT)
		{
			newCursorPos = ddiPenCoordsToPos(data->pen, ev->x, ev->y);
			data->cursorPos = newCursorPos;
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
			if (data->cursorPos != (off_t)ddiCountUTF8(data->text))
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
		else if (ev->keycode == '\r')
		{
			GWMCommandEvent cmdev;
			memset(&cmdev, 0, sizeof(GWMCommandEvent));
			cmdev.header.type = GWM_EVENT_COMMAND;
			cmdev.symbol = GWM_SYM_OK;
			return gwmPostEvent((GWMEvent*) &cmdev, field);
		}
		else if ((ev->keycode == '\b') && (!disabled))
		{
			gwmTextFieldBackspace(field);
		}
		else if ((ev->keychar != 0) && (!disabled))
		{
			//sprintf(buf, "%c", (char)ev->keychar);
			ddiWriteUTF8(buf, ev->keychar);
			gwmTextFieldInsert(field, buf);
		};
		return GWM_EVSTATUS_OK;
	case GWM_EVENT_UP:
		if (ev->keycode == GWM_KC_MOUSE_LEFT)
		{
			data->clickPos = -1;
		}
		else if (ev->keycode == GWM_KC_MOUSE_RIGHT)
		{
			gwmOpenMenu(data->menu, field, ev->x, ev->y);
		};
		return GWM_EVSTATUS_OK;
	case GWM_EVENT_MOTION:
		if (data->clickPos != -1)
		{
			newCursorPos = ddiPenCoordsToPos(data->pen, ev->x, ev->y);
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
		return GWM_EVSTATUS_OK;
	default:
		return GWM_EVSTATUS_CONT;
	};
};

static void txtGetSize(GWMWindow *field, int *width, int *height)
{
	*width = TEXTFIELD_MIN_WIDTH;
	*height = TEXTFIELD_HEIGHT;
};

static void txtPosition(GWMWindow *field, int x, int y, int width, int height)
{
	y += (height - TEXTFIELD_HEIGHT) / 2;
	gwmMoveWindow(field, x, y);
	gwmResizeWindow(field, width, TEXTFIELD_HEIGHT);
	gwmRedrawTextField(field);
};

GWMWindow *gwmCreateTextField(GWMWindow *parent, const char *text, int x, int y, int width, int flags)
{
	GWMWindow *field = gwmCreateWindow(parent, "GWMTextField", x, y, width, TEXTFIELD_HEIGHT, 0);
	if (field == NULL) return NULL;
	
	GWMTextFieldData *data = (GWMTextFieldData*) malloc(sizeof(GWMTextFieldData));
	
	data->text = strdup(text);
	data->cursorPos = 0;
	data->focused = 0;
	data->flags = flags;
	
	data->selectStart = data->selectEnd = 0;
	data->clickPos = -1;
	data->pen = NULL;
	data->icon = NULL;
	
	field->getMinSize = field->getPrefSize = txtGetSize;
	field->position = txtPosition;
	
	data->menu = gwmCreateMenu();
	gwmMenuAddEntry(data->menu, "Cut", txtCut, field);
	gwmMenuAddEntry(data->menu, "Copy", txtCopy, field);
	gwmMenuAddEntry(data->menu, "Paste", txtPaste, field);
	gwmMenuAddSeparator(data->menu);
	gwmMenuAddEntry(data->menu, "Select All", txtSelectAll, field);
	
	gwmPushEventHandler(field, gwmTextFieldHandler, data);
	gwmRedrawTextField(field);
	gwmSetWindowCursor(field, GWM_CURSOR_TEXT);
	
	return field;
};

GWMWindow* gwmNewTextField(GWMWindow *parent)
{
	return gwmCreateTextField(parent, "", 0, 0, 0, 0);
};

void gwmDestroyTextField(GWMWindow *field)
{
	GWMTextFieldData *data = (GWMTextFieldData*) gwmGetData(field, gwmTextFieldHandler);
	free(data->text);
	free(data);
	gwmDestroyMenu(data->menu);
	gwmDestroyWindow(field);
};

size_t gwmGetTextFieldSize(GWMWindow *field)
{
	GWMTextFieldData *data = (GWMTextFieldData*) gwmGetData(field, gwmTextFieldHandler);
	return strlen(data->text);
};

const char* gwmReadTextField(GWMWindow *field)
{
	GWMTextFieldData *data = (GWMTextFieldData*) gwmGetData(field, gwmTextFieldHandler);
	return data->text;
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
	GWMTextFieldData *data = (GWMTextFieldData*) gwmGetData(field, gwmTextFieldHandler);
	free(data->text);
	data->text = strdup(newText);
	data->selectStart = data->selectEnd = 0;
	data->cursorPos = 0;
	gwmRedrawTextField(field);
};

void gwmTextFieldSelectAll(GWMWindow *field)
{
	GWMTextFieldData *data = (GWMTextFieldData*) gwmGetData(field, gwmTextFieldHandler);
	data->selectStart = 0;
	data->selectEnd = ddiCountUTF8(data->text);
	data->cursorPos = data->selectEnd;
	gwmRedrawTextField(field);
};

void gwmSetTextFieldFlags(GWMWindow *field, int flags)
{
	GWMTextFieldData *data = (GWMTextFieldData*) gwmGetData(field, gwmTextFieldHandler);
	data->flags = flags;
	gwmRedrawTextField(field);
};

void gwmSetTextFieldIcon(GWMTextField *field, DDISurface *icon)
{
	GWMTextFieldData *data = (GWMTextFieldData*) gwmGetData(field, gwmTextFieldHandler);
	data->icon = icon;
	gwmRedrawTextField(field);
};
