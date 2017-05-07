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

#define	CELL_WIDTH					16
#define	CELL_HEIGHT					16
#define	MAZE_WIDTH					32
#define	MAZE_HEIGHT					32

typedef struct
{
	int x;
	int y;
} Coords;

DDIColor colBackground = {0x00, 0x00, 0x00, 0xFF};
DDIColor colCake = {0xFF, 0x99, 0x33, 0xFF};
DDIColor colHead = {0x33, 0xCC, 0x33, 0xFF};
DDIColor colTail = {0x19, 0x66, 0x19, 0xFF};
DDIColor colTransparent = {0, 0, 0, 0};
DDIColor colGameOver = {0xFF, 0xFF, 0x00, 0xFF};

Coords cakePos;
int snakeLen;
int gameOver;
Coords *snakePos = NULL;		/* array of positions */
Coords vector;
Coords nextVector;

GWMWindow *win;
DDIFont *fntGameOver;
DDISurface *imgGameOver;

volatile int stepWait = 300;
void* timerThread(void *ignore)
{
	while (1)
	{
		poll(NULL, 0, stepWait);
		gwmPostUpdate(win);
	};
	
	return NULL;
};

void resetGame()
{	
	free(snakePos);
	snakeLen = 5;
	
	snakePos = (Coords*) malloc(sizeof(Coords) * 5);
	
	int i;
	for (i=0; i<5; i++)
	{
		snakePos[i].x = MAZE_WIDTH/2;
		snakePos[i].y = MAZE_HEIGHT/2+i;
	};
	
	do
	{
		cakePos.x = rand() % MAZE_WIDTH;
	} while (cakePos.x == MAZE_WIDTH/2);
	
	cakePos.y = rand() % MAZE_HEIGHT;
	
	vector.x = 0;
	vector.y = 0;
	
	nextVector.x = 0;
	nextVector.y = 0;
	gameOver = 0;
};

int isSnakePos(int x, int y)
{
	int i;
	for (i=0; i<snakeLen; i++)
	{
		if ((snakePos[i].x == x) && (snakePos[i].y == y))
		{
			return 1;
		};
	};
	
	return 0;
};

void move()
{
	if (gameOver)
	{
		return;
	};
	
	if ((nextVector.x != 0) || (nextVector.y != 0))
	{
		if ((vector.x * nextVector.x + vector.y * nextVector.y) == 0)
		{
			vector.x = nextVector.x;
			vector.y = nextVector.y;
		};
		
		nextVector.x = 0;
		nextVector.y = 0;
	};
	
	if ((vector.x == 0) && (vector.y == 0))
	{
		// the game hasn't started yet
		return;
	};
	
	int destX = snakePos[0].x + vector.x;
	int destY = snakePos[0].y + vector.y;
	
	if (destX < 0) destX += MAZE_WIDTH;
	if (destX >= MAZE_WIDTH) destX -= MAZE_WIDTH;
	
	if (destY < 0) destY += MAZE_WIDTH;
	if (destY >= MAZE_HEIGHT) destY -= MAZE_HEIGHT;
	
	if (isSnakePos(destX, destY))
	{
		gameOver = 1;
	}
	else if ((destX == cakePos.x) && (destY == cakePos.y))
	{
		Coords *newSnake = (Coords*) malloc(sizeof(Coords)*(snakeLen+1));
		memcpy(&newSnake[1], snakePos, sizeof(Coords)*snakeLen);
		newSnake[0].x = destX;
		newSnake[0].y = destY;
		snakeLen++;
		free(snakePos);
		snakePos = newSnake;
		
		do
		{
			cakePos.x = rand() % MAZE_WIDTH;
			cakePos.y = rand() % MAZE_HEIGHT;
		} while (isSnakePos(cakePos.x, cakePos.y));
	}
	else
	{
		Coords *newSnake = (Coords*) malloc(sizeof(Coords)*snakeLen);
		int i;
		for (i=1; i<snakeLen; i++)
		{
			newSnake[i].x = snakePos[i-1].x;
			newSnake[i].y = snakePos[i-1].y;
		};
		
		newSnake[0].x = destX;
		newSnake[0].y = destY;
		
		free(snakePos);
		snakePos = newSnake;
	};
};

void renderGame()
{
	DDISurface *canvas = gwmGetWindowCanvas(win);
	
	ddiFillRect(canvas, 0, 20, CELL_WIDTH*MAZE_WIDTH, CELL_HEIGHT*MAZE_HEIGHT, &colBackground);
	ddiFillRect(canvas, CELL_WIDTH*cakePos.x, 20+CELL_HEIGHT*cakePos.y, CELL_WIDTH, CELL_HEIGHT, &colCake);
	
	DDIColor *color = &colHead;
	int i;
	for (i=0; i<snakeLen; i++)
	{
		ddiFillRect(canvas, CELL_WIDTH*snakePos[i].x, 20+CELL_HEIGHT*snakePos[i].y, CELL_WIDTH, CELL_HEIGHT, color);
		color = &colTail;
	};
	
	if (gameOver)
	{
		int x = (CELL_WIDTH*MAZE_WIDTH)/2 - imgGameOver->width/2;
		int y = 20 + (CELL_HEIGHT*MAZE_HEIGHT)/2 - imgGameOver->height/2;
		ddiBlit(imgGameOver, 0, 0, canvas, x, y, imgGameOver->width, imgGameOver->height);
	};
	
	gwmPostDirty(win);
};

int snakeHandler(GWMEvent *ev, GWMWindow *win)
{
	switch (ev->type)
	{
	case GWM_EVENT_UPDATE:
		move();
		renderGame();
		return 0;
	case GWM_EVENT_DOWN:
		switch (ev->keycode)
		{
		case GWM_KC_LEFT:
			nextVector.x = -1;
			nextVector.y = 0;
			break;
		case GWM_KC_RIGHT:
			nextVector.x = 1;
			nextVector.y = 0;
			break;
		case GWM_KC_UP:
			nextVector.x = 0;
			nextVector.y = -1;
			break;
		case GWM_KC_DOWN:
			nextVector.x = 0;
			nextVector.y = 1;
			break;
		case ' ':
			nextVector.x = 0;
			nextVector.y = 0;
			vector.x = 0;
			vector.y = 0;
			break;
		case GWM_KC_CTRL:
			stepWait = 100;
			break;
		};
		return 0;
	case GWM_EVENT_UP:
		if (ev->keycode == GWM_KC_CTRL)
		{
			stepWait = 300;
		};
		return 0;
	case GWM_EVENT_CLOSE:
		return -1;
	default:
		return gwmDefaultHandler(ev, win);
	};
};

int onReset(void *ignore)
{
	resetGame();
	renderGame();
	gwmSetWindowFlags(win, GWM_WINDOW_MKFOCUSED);
	return 0;
};

int onExit(void *ignore)
{
	return -1;
};

void setupMenus()
{
	GWMWindow *menubar = gwmCreateMenubar(win);
	
	GWMMenu *menuGame = gwmCreateMenu();
	gwmMenuAddEntry(menuGame, "Reset", onReset, NULL);
	gwmMenuAddEntry(menuGame, "Exit", onExit, NULL);
	
	gwmMenubarAdd(menubar, "Game", menuGame);
	gwmMenubarAdjust(menubar);
};

int main()
{
	srand(time(NULL));
	resetGame();
	
	if (gwmInit() != 0)
	{
		fprintf(stderr, "Failed to initialize GWM!\n");
		return 1;
	};
	
	fntGameOver = ddiLoadFont("DejaVu Sans", 50, DDI_STYLE_BOLD, NULL);
	if (fntGameOver == NULL)
	{
		fprintf(stderr, "Failed to load the 'GAME OVER' font!\n");
		return 1;
	};
	
	win = gwmCreateWindow(NULL, "Snake",
			GWM_POS_UNSPEC, GWM_POS_UNSPEC,
			CELL_WIDTH*MAZE_WIDTH, 20+CELL_HEIGHT*MAZE_HEIGHT,
			GWM_WINDOW_NOTASKBAR | GWM_WINDOW_HIDDEN);
			
	if (win == NULL)
	{
		fprintf(stderr, "Failed to create window!\n");
		return 1;
	};
	
	setupMenus();
	
	DDISurface *canvas = gwmGetWindowCanvas(win);
	DDISurface *icon = ddiLoadAndConvertPNG(&canvas->format, "/usr/share/images/snake.png", NULL);
	if (icon != NULL)
	{
		gwmSetWindowIcon(win, icon);
	};
	
	DDIPen *pen = ddiCreatePen(&canvas->format, fntGameOver, 0, 0,
			CELL_WIDTH*MAZE_WIDTH, CELL_HEIGHT*MAZE_HEIGHT, 0, 0, NULL);
	ddiSetPenWrap(pen, 0);
	ddiSetPenColor(pen, &colGameOver);
	ddiWritePen(pen, "GAME OVER");
	
	int width, height;
	ddiGetPenSize(pen, &width, &height);
	
	imgGameOver = ddiCreateSurface(&canvas->format, width, height, NULL, 0);
	ddiFillRect(imgGameOver, 0, 0, width, height, &colTransparent);
	ddiExecutePen(pen, imgGameOver);
	ddiDeletePen(pen);
	
	renderGame();
	
	pthread_t thread;
	pthread_create(&thread, NULL, timerThread, NULL);
	
	gwmSetEventHandler(win, snakeHandler);
	gwmSetWindowFlags(win, GWM_WINDOW_MKFOCUSED);
	gwmMainLoop();
	gwmQuit();
	return 0;
};
