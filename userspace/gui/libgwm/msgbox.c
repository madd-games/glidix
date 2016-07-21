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

static DDISurface *imgIcons = NULL;

static int buttonCounts[] = {
	1,
	2,
	2,
	3
};

typedef const char *ButtonList[3];
static ButtonList buttonLabels[] = {
	{"OK"},
	{"Yes", "No"},
	{"OK", "Cancel"},
	{"Yes", "No", "Cancel"}
};

static int gwmOnMsgButton(void *param)
{
	return -1;
};

int gwmButtonHandler(GWMEvent *ev, GWMWindow *button);
int gwmMessageBox(GWMWindow *parent, const char *caption, const char *text, int flags)
{
	int iconIndex = flags & 0xF;
	int buttonSet = (flags >> 4) & 0xF;
	
	if (buttonSet > 3)
	{
		return -1;
	};
	
	if (iconIndex > 4)
	{
		return -1;
	};
	
	DDIPixelFormat screenFormat;
	gwmGetScreenFormat(&screenFormat);
	
	DDIPen *pen = ddiCreatePen(&screenFormat, gwmGetDefaultFont(), 36, 2, 200, 20, 0, 0, NULL);
	ddiWritePen(pen, text);
	
	int width, height;
	ddiGetPenSize(pen, &width, &height);

	int minWidth = 2 + buttonCounts[buttonSet] * 62;
	if (width < minWidth)
	{
		width = minWidth;
	};
	
	if (height < 32)
	{
		ddiSetPenPosition(pen, 36, 18-height/2);
		height = 32;
	};
	
	int screenWidth, screenHeight;
	gwmScreenSize(&screenWidth, &screenHeight);
	
	GWMWindow *dialog = gwmCreateWindow(NULL, caption, screenWidth/2-width/2, screenHeight/2-33,
						width + 38, height + 34, GWM_WINDOW_NOTASKBAR | GWM_WINDOW_HIDDEN);
	if (dialog == NULL)
	{
		return -1;
	};
	
	DDISurface *canvas = gwmGetWindowCanvas(dialog);
	if (imgIcons == NULL)
	{
		const char *error;
		imgIcons = ddiLoadAndConvertPNG(&canvas->format, "/usr/share/images/mbicons.png", &error);
		if (imgIcons == NULL)
		{
			fprintf(stderr, "Failed to load message box icon image (/usr/share/images/mbicons.png): %s\n", error);
			abort();
		};
	};
	
	if (iconIndex != 0)
	{
		ddiBlit(imgIcons, 32*(iconIndex-1), 0, canvas, 2, (height/2)-14, 32, 32);
	};
	
	ddiExecutePen(pen, canvas);
	ddiDeletePen(pen);
	
	int numButtons = buttonCounts[buttonSet];
	int startX = ((width+38) / 2) - (numButtons * 31);
	
	GWMWindow *buttons[3];
	int i;
	for (i=0; i<numButtons; i++)
	{
		buttons[i] = gwmCreateButton(dialog, buttonLabels[buttonSet][i], startX+62*i, height+2, 60, 0);
		gwmSetButtonCallback(buttons[i], gwmOnMsgButton, NULL);
	};
	
	gwmSetWindowFlags(dialog, GWM_WINDOW_MKFOCUSED | GWM_WINDOW_NOTASKBAR | GWM_WINDOW_NOSYSMENU);
	
	while (1)
	{
		GWMEvent ev;
		gwmWaitEvent(&ev);
		
		for (i=0; i<numButtons; i++)
		{
			if (ev.win == buttons[i]->id)
			{
				if (gwmButtonHandler(&ev, buttons[i]) != 0)
				{
					int result = i;
					gwmDestroyWindow(dialog);
					for (i=0; i<numButtons; i++)
					{
						gwmDestroyWindow(buttons[i]);
					};
					return result;
				};
			};
		};
	};
	
	return 0;
};
