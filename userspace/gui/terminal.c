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

#include <sys/glidix.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <libddi.h>
#include <pthread.h>
#include <fcntl.h>
#include <unistd.h>
#include <signal.h>
#include "gui.h"

typedef struct
{
	char c;
	uint8_t	attr;
} ConsoleCell;

extern const struct bitmap_font font_8x16;
extern const unsigned char __font_8x16_bitmap__[];
ConsoleCell grid[80*25];
int cursorX=0, cursorY=0;
uint8_t currentAttr = 0x07;
pthread_spinlock_t consoleLock;
pthread_t ctrlThread;
extern const char font[16*256];
int running = 1;

int fdMaster;

DDIColor consoleColors[16] = {
	{0x00, 0x00, 0x00, 0xFF},		/* 0 */
	{0x00, 0x00, 0xAA, 0xFF},		/* 1 */
	{0x00, 0xAA, 0x00, 0xFF},		/* 2 */
	{0x00, 0xAA, 0xAA, 0xFF},		/* 3 */
	{0xAA, 0x00, 0x00, 0xFF},		/* 4 */
	{0xAA, 0x00, 0xAA, 0xFF},		/* 5 */
	{0xAA, 0x55, 0x00, 0xFF},		/* 6 */
	{0xAA, 0xAA, 0xAA, 0xFF},		/* 7 */
	{0x55, 0x55, 0x55, 0xFF},		/* 8 */
	{0x55, 0x55, 0xFF, 0xFF},		/* 9 */
	{0x55, 0xFF, 0x55, 0xFF},		/* A */
	{0x55, 0xFF, 0xFF, 0xFF},		/* B */
	{0xFF, 0x55, 0x55, 0xFF},		/* C */
	{0xFF, 0x55, 0xFF, 0xFF},		/* D */
	{0xFF, 0xFF, 0x55, 0xFF},		/* E */
	{0xFF, 0xFF, 0xFF, 0xFF},		/* F */
};

void clearConsole()
{
	cursorX = 0;
	cursorY = 0;
	
	int i;
	for (i=0; i<80*25; i++)
	{
		grid[i].c = 0;
		grid[i].attr = 0x07;
	};
};

void scroll()
{
	int i;
	for (i=0; i<80*24; i++)
	{
		grid[i].c = grid[i+80].c;
		grid[i].attr = grid[i+80].attr;
	};
	
	for (i=80*24; i<80*25; i++)
	{
		grid[i].c = 0;
		grid[i].attr = currentAttr;
	};
	
	cursorY--;
};

void writeConsole(const char *buf, size_t sz)
{
	while (sz--)
	{
		char c = *buf++;

		if (c == '\b')
		{
			if ((cursorX == 0) && (cursorY == 0))
			{
				continue;
			};
			
			if (cursorX == 0)
			{
				cursorX = 79;
				cursorY--;
			}
			else
			{
				cursorX--;
			};
			
			grid[cursorY * 80 + cursorX].c = 0;
		}
		else if (c == '\n')
		{
			cursorY++;
			cursorX = 0;
		}
		else
		{
			grid[cursorY * 80 + cursorX].c = c;
			grid[cursorY * 80 + cursorX].attr = currentAttr;
			cursorX++;
			if (cursorX == 80)
			{
				cursorX = 0;
				cursorY++;
			};
		};
	
		if (cursorY == 25) scroll();
	};
};

void renderConsole(DDISurface *surface)
{
	int x, y;
	for (x=0; x<80; x++)
	{
		for (y=0; y<25; y++)
		{
			ConsoleCell *cell = &grid[y * 80 + x];
			DDIColor *background = &consoleColors[cell->attr >> 4];
			DDIColor *foreground = &consoleColors[cell->attr & 0xF];
			
			unsigned int index = (unsigned char) cell->c;
			const char *bmp = &font[index * 16];
			ddiFillRect(surface, x*9, y*16, 9, 16, background);
			ddiExpandBitmap(surface, x*9, y*16, DDI_BITMAP_8x16, bmp, foreground);
		};
	};
	
	// cursor
	ddiFillRect(surface, cursorX*9, cursorY*16+14, 9, 2, &consoleColors[7]);
};

void *ctrlThreadFunc(void *ignore)
{
	(void)ignore;

	pid_t pid = fork();
	if (pid == -1)
	{
		perror("fork");
		close(fdMaster);
		gwmQuit();
		exit(1);
	}
	else if (pid == 0)
	{
		setsid();
		int fd = open(ptsname(fdMaster), O_RDWR);
		if (fd == -1)
		{
			exit(1);
		};
		
		close(0);
		close(1);
		close(2);
		
		dup2(fd, 0);
		dup2(fd, 1);
		dup2(fd, 2);
		
		execl("/bin/sh", "sh", NULL);
		exit(1);
	};
	
	while (1)
	{
		char buffer[4096];
		ssize_t sz = read(fdMaster, buffer, 4096);
		if (sz > 0L)
		{
			pthread_spin_lock(&consoleLock);
			writeConsole(buffer, sz);
			pthread_spin_unlock(&consoleLock);
			gwmPostUpdate(NULL);
		};
		
		if (!running)
		{
			gwmPostUpdate(NULL);
		};
	};
	
	while (1) pause();
	
	return NULL;
};

void onSigChld(int sig)
{
	running = 0;
};

int main()
{	
	gwmInit();
	GWMWindow *wnd = gwmCreateWindow(NULL, "Terminal", 10, 10, 720, 400, GWM_WINDOW_MKFOCUSED);
	
	DDISurface *surface = gwmGetWindowCanvas(wnd);
	clearConsole();
	renderConsole(surface);
	gwmPostDirty();

	fdMaster = getpt();
	if (fdMaster == -1)
	{
		perror("getpt");
		gwmQuit();
		return 1;
	};
	
	grantpt(fdMaster);
	unlockpt(fdMaster);
	
	signal(SIGCHLD, onSigChld);
	pthread_create(&ctrlThread, NULL, ctrlThreadFunc, NULL);
	
	while (1)
	{
		GWMEvent ev;
		gwmWaitEvent(&ev);
		
		if (ev.type == GWM_EVENT_CLOSE)
		{
			break;
		}
		else if (ev.type == GWM_EVENT_UPDATE)
		{
			pthread_spin_lock(&consoleLock);
			renderConsole(surface);
			pthread_spin_unlock(&consoleLock);
			gwmPostDirty();
			
			if (!running) break;
		}
		else if (ev.type == GWM_EVENT_DOWN)
		{
			if (ev.keymod & GWM_KM_CTRL)
			{
				char put = 0;
				if (ev.keycode == 'c')
				{
					put = 0x83;	// CC_VINTR
				}
				else if (ev.keycode == 'k')
				{
					put = 0x84;	// CC_VKILL
				};
				
				if (put != 0)
				{
					write(fdMaster, &put, 1);
				};
			}
			else
			{
				char c = (char) ev.keychar;
				if (c != 0)
				{
					write(fdMaster, &c, 1);
				};
			};
		};
	};
	
	gwmQuit();
	
	return 0;
};
