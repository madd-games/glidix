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

#define	TILE_SIZE				64

GWMWindow *win;

int grid[4*4];
DDISurface* numberSurfaces[16];

int isSolved()
{
	int i;
	for (i=0; i<16; i++)
	{
		if (grid[i] != (i+1))
		{
			return 0;
		};
	};
	
	return 1;
};

void redraw()
{
	DDISurface *canvas = gwmGetWindowCanvas(win);
	
	static DDIColor colUnsolved = {0xFF, 0x00, 0x00, 0xFF};
	static DDIColor colSolved = {0x00, 0xFF, 0x00, 0xFF};
	static DDIColor colSpace = {0x00, 0x00, 0x00, 0xFF};
	
	DDIColor *col = &colUnsolved;
	if (isSolved())
	{
		col = &colSolved;
	};
	
	int x, y;
	for (x=0; x<4; x++)
	{
		for (y=0; y<4; y++)
		{
			if (grid[y*4+x] == 16)
			{
				ddiFillRect(canvas, TILE_SIZE*x, 20+TILE_SIZE*y, TILE_SIZE, TILE_SIZE, &colSpace);
			}
			else
			{
				ddiFillRect(canvas, TILE_SIZE*x, 20+TILE_SIZE*y, TILE_SIZE, TILE_SIZE, col);
				DDISurface *src = numberSurfaces[grid[y*4+x]-1];
				ddiBlit(src, 0, 0,
					canvas, TILE_SIZE*x + (TILE_SIZE/2-src->width/2),
					20+TILE_SIZE*y + (TILE_SIZE/2-src->height),
					src->width, src->height);
			};
		};
	};
	
	gwmPostDirty(win);
};

void clickTile(int x, int y)
{
	if ((x < 0) || (x > 3) || (y < 0) || (y > 3))
	{
		return;
	};
	
	int index = y * 4 + x;
	if (grid[index] == 16)
	{
		return;
	};
	
	// see if we can move left
	if (x != 0)
	{
		if (grid[index-1] == 16)
		{
			grid[index-1] = grid[index];
			grid[index] = 16;
			return;
		};
	};

	// see if we can move right
	if (x != 3)
	{
		if (grid[index+1] == 16)
		{
			grid[index+1] = grid[index];
			grid[index] = 16;
			return;
		};
	};
	
	// see if we can move up
	if (y != 0)
	{
		if (grid[index-4] == 16)
		{
			grid[index-4] = grid[index];
			grid[index] = 16;
			return;
		};
	};
	
	// see if we can move down
	if (y != 3)
	{
		if (grid[index+4] == 16)
		{
			grid[index+4] = grid[index];
			grid[index] = 16;
			return;
		};
	};
};

int mysticHandler(GWMEvent *ev, GWMWindow *win, void *context)
{
	switch (ev->type)
	{
	case GWM_EVENT_CLOSE:
		return GWM_EVSTATUS_BREAK;
	case GWM_EVENT_UP:
		if (ev->keycode == GWM_KC_MOUSE_LEFT)
		{
			clickTile(ev->x/TILE_SIZE, (ev->y-20)/TILE_SIZE);
			redraw();
		};
		return GWM_EVSTATUS_OK;
	default:
		return GWM_EVSTATUS_CONT;
	};
};

int onScramble(void *ignore)
{
	srand(time(NULL));
	
	int count;
	for (count=0; count<100; count++)
	{
		int spaceAt;
		for (spaceAt=0; spaceAt<16; spaceAt++)
		{
			if (grid[spaceAt] == 16) break;
		};
		
		int spaceY = spaceAt/4;
		int spaceX = spaceAt%4;
		
		int destList[4];
		int destFound = 0;
		
		// can the space move left?
		if (spaceX != 0)
		{
			destList[destFound++] = spaceAt - 1;
		};
		
		// can the space move right?
		if (spaceX != 3)
		{
			destList[destFound++] = spaceAt + 1;
		};
		
		// can the space move up?
		if (spaceY != 0)
		{
			destList[destFound++] = spaceAt - 4;
		};
		
		// can teh space move down?
		if (spaceY != 3)
		{
			destList[destFound++] = spaceAt + 4;
		};
		
		// generate a random move
		int dest = destList[rand() % destFound];
		
		grid[spaceAt] = grid[dest];
		grid[dest] = 16;
	};
	
	redraw();
	gwmSetWindowFlags(win, GWM_WINDOW_MKFOCUSED);
	return 0;
};

int onExit(void *ignore)
{
	return -1;
};

int main()
{
	if (gwmInit() != 0)
	{
		fprintf(stderr, "Failed to initialize GWM!\n");
		return 1;
	};
	
	win = gwmCreateWindow(NULL, "Mystic Square",
				GWM_POS_UNSPEC, GWM_POS_UNSPEC,
				4*TILE_SIZE, 20+4*TILE_SIZE,
				GWM_WINDOW_NOTASKBAR | GWM_WINDOW_HIDDEN);
	
	
	DDISurface *canvas = gwmGetWindowCanvas(win);
	DDISurface *icon = ddiLoadAndConvertPNG(&canvas->format, "/usr/share/images/mystic.png", NULL);
	
	if (icon != NULL)
	{
		gwmSetWindowIcon(win, icon);
	};
	
	int i;
	for (i=0; i<16; i++)
	{
		grid[i] = i+1;
		
		char text[16];
		sprintf(text, "%d", i+1);
		
		static DDIColor black = {0x00, 0x00, 0x00, 0xFF};
		numberSurfaces[i] = ddiRenderText(&canvas->format, gwmGetDefaultFont(), &black, text, NULL);
	};
	
	GWMWindow *menubar = gwmCreateMenubar(win);
	
	GWMMenu *menuGame = gwmCreateMenu();
	gwmMenuAddEntry(menuGame, "Scramble", onScramble, NULL);
	gwmMenuAddEntry(menuGame, "Exit", onExit, NULL);
	
	gwmMenubarAdd(menubar, "Game", menuGame);
	gwmMenubarAdjust(menubar);
	
	redraw();
	
	gwmPushEventHandler(win, mysticHandler, NULL);
	gwmSetWindowFlags(win, GWM_WINDOW_MKFOCUSED);
	gwmMainLoop();
	gwmQuit();
	return 0;
};
