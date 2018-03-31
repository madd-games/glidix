/*
	Glidix Installer

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

#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <string.h>
#include <termios.h>
#include "render.h"

void setCursor(uint8_t x, uint8_t y)
{
	uint8_t seq[4] = "\e#??";
	seq[2] = x;
	seq[3] = y;
	write(1, seq, 4);
};

void setColor(uint8_t col)
{
	uint8_t seq[3] = "\e\"?";
	seq[2] = col;
	write(1, seq, 3);
};

void clearScreen()
{
	struct winsize winsz;
	ioctl(1, TIOCGWINSZ, &winsz);

	setCursor(0, 0);
	setColor(COLOR_BACKGROUND);
	
	char screen[winsz.ws_row*winsz.ws_col];
	memset(screen, ' ', winsz.ws_row*winsz.ws_col);
	
	write(1, screen, winsz.ws_row*winsz.ws_col);
};

void renderWindow(const char *status, const char *caption, int width, int height, int *startX, int *startY)
{
	struct winsize winsz;
	ioctl(1, TIOCGWINSZ, &winsz);
	
	clearScreen();
	
	setCursor(0, 0);
	printf("Glidix Installer");
	
	setCursor(0, winsz.ws_row-1);
	printf("%s", status);
	
	int windowX = (winsz.ws_col - width - 2)/2;
	int windowY = (winsz.ws_row - height - 2)/2;
	
	setCursor((uint8_t)windowX, (uint8_t)windowY);
	
	char titleBar[width+2];
	int captionOffset = (width - strlen(caption))/2;
	memset(titleBar, 205, captionOffset);
	memcpy(&titleBar[captionOffset], caption, strlen(caption));
	memset(&titleBar[captionOffset+strlen(caption)], 205, width + 2 - captionOffset - strlen(caption));
	
	titleBar[0] = 201;
	titleBar[width+1] = 187;
	
	titleBar[captionOffset-1] = 181;
	titleBar[captionOffset+strlen(caption)] = 198;
	
	setColor(COLOR_WINDOW);
	write(1, titleBar, width+2);
	
	char clientBar[width+2];
	memset(clientBar, ' ', width+2);
	clientBar[0] = clientBar[width+1] = 186;
	
	int i;
	for (i=0; i<height; i++)
	{
		setColor(COLOR_WINDOW);
		setCursor((uint8_t)windowX, (uint8_t)(windowY + 1 + i));
		write(1, clientBar, width+2);
		
		setColor(COLOR_SHADOW);
		write(1, " ", 1);
	};
	
	char bottomBar[width+2];
	memset(bottomBar, 205, width+2);
	bottomBar[0] = 200;
	bottomBar[width+1] = 188;
	
	setColor(COLOR_WINDOW);
	setCursor((uint8_t)windowX, (uint8_t)(windowY + 1 + height));
	write(1, bottomBar, width+2);
	
	setColor(COLOR_SHADOW);
	write(1, " ", 1);
	
	char shadowBar[width+2];
	setCursor((uint8_t)(windowX+1), (uint8_t)(windowY + 2 + height));
	memset(shadowBar, 220, width+2);
	write(1, shadowBar, width+2);
	
	setCursor(79, 24);
	*startX = windowX + 1;
	*startY = windowY + 1;
};
