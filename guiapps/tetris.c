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

#include <assert.h>
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

#define	CELL_WIDTH			20
#define	CELL_HEIGHT			20

#define	GRID_COLS			10
#define	GRID_ROWS			20

#define	NUM_PIECES			7

typedef struct
{
	int x, y;
} Coords;

typedef struct
{
	DDIColor color;
	Coords blockPos[4];
} Piece;

typedef struct
{
	DDIColor color;
	int occupied;
} Cell;

enum
{
	SYM_RESET = GWM_SYM_USER,
};

Piece *currentPiece;
int pieceX, pieceY;
int pieceRot[4];

int score;
int gameOver;
Cell grid[GRID_COLS * GRID_ROWS];

DDIFont *fntGameOver;
DDISurface *imgGameOver;
DDISurface *icon;
GWMLabel *lblStatus;
GWMWindow *tetris;

const char *tetrisHelp = "\
GLIDIX TETRIS\n\
=============\n\
\n\
Game Rules\n\
----------\n\
There are shapes, made up of 4 blocks each, moving from the top of the screen to the bottom, in a grid. Once the shape touches the bottom of the screen, or some other shape, it stops and settles. You can move the shapes as they fall. If you make an entire row filled with blocks, that row disappears, and all blocks above it move down. The more rows you delete, the more score you get: and if the shapes stack to the very top, you lose!\n\
\n\
Controls\n\
--------\n\
Left/right arrow key - move current shape left or right\n\
Up arrow key - rotate the shape (there must be enough space to rotate it!)\n\
";

Piece pieces[NUM_PIECES] = {
	/* "S" shape */
	{
		{0x00, 0xFF, 0x00, 0xFF},
		{
			{-1, 0},
			{0, 0},
			{0, -1},
			{1, -1},
		}
	},
	
	/* the long one */
	{
		{0xFF, 0x00, 0x00, 0xFF},
		{
			{-1, 0},
			{0, 0},
			{1, 0},
			{2, 0},
		},
	},
	
	/* the square */
	{
		{0x00, 0xFF, 0xFF, 0xFF},
		{
			{0, 0},
			{0, 1},
			{1, 0},
			{1, 1},
		},
	},
	
	/* "T" shape */
	{
		{0x7F, 0x7F, 0x00, 0xFF},
		{
			{0, 0},
			{-1, 0},
			{1, 0},
			{0, -1},
		},
	},
	
	/* "L" shape */
	{
		{0x00, 0x00, 0xFF, 0xFF},
		{
			{0, 0},
			{0, -1},
			{0, -2},
			{1, 0},
		},
	},
	
	/* inverted "L" shape */
	{
		{0xFF, 0x00, 0xFF, 0xFF},
		{
			{0, 0},
			{0, -1},
			{0, -2},
			{-1, 0},
		},
	},

	/* inverted "S" shape */
	{
		{0x11, 0xAA, 0xFF, 0xFF},
		{
			{1, 0},
			{0, 0},
			{0, -1},
			{-1, -1},
		}
	},
};

void resetGame();

void getPiecePos(Coords *positions)
{
	int i;
	for (i=0; i<4; i++)
	{
		positions[i].x = currentPiece->blockPos[i].x * pieceRot[0] + currentPiece->blockPos[i].y * pieceRot[1] + pieceX;
		positions[i].y = currentPiece->blockPos[i].x * pieceRot[2] + currentPiece->blockPos[i].y * pieceRot[3] + pieceY;
	};
};

int isOccupied(int x, int y)
{
	if (x < 0 || x >= GRID_COLS || y < 0 || y >= GRID_ROWS) return 1;
	return grid[y * GRID_COLS + x].occupied;
};

void nextPiece()
{
	int pieceIndex = rand() % NUM_PIECES;
	currentPiece = &pieces[pieceIndex];
	
	pieceRot[0] = pieceRot[3] = 1;
	pieceRot[1] = pieceRot[2] = 0;
	
	int lowestY = currentPiece->blockPos[0].y;
	int i;
	for (i=0; i<4; i++)
	{
		if (currentPiece->blockPos[i].y < lowestY) lowestY = currentPiece->blockPos[i].y;
	};
	
	pieceY = -lowestY;
	
	int lowestX = currentPiece->blockPos[0].x;
	int highestX = currentPiece->blockPos[0].x;
	
	for (i=0; i<4; i++)
	{
		if (currentPiece->blockPos[i].x < lowestX) lowestX = currentPiece->blockPos[i].x;
		if (currentPiece->blockPos[i].x > highestX) highestX = currentPiece->blockPos[i].x;
	};

	int baseX = -lowestX;
	int width = GRID_COLS - (highestX - lowestX) - baseX;
	
	pieceX = baseX + rand() % width;

	Coords positions[4];
	getPiecePos(positions);
	
	int occupied = 0;
	for (i=0; i<4; i++)
	{
		if (isOccupied(positions[i].x, positions[i].y))
		{
			occupied = 1;
			break;
		};
	};
	
	if (occupied)
	{
		gameOver = 1;
		gwmHighScore("tetris", score);
	};
};

int tetrisCommand(GWMCommandEvent *ev)
{
	switch (ev->symbol)
	{
	case SYM_RESET:
		resetGame();
		return GWM_EVSTATUS_OK;
	case GWM_SYM_ABOUT:
		{
			GWMAboutDialog *about = gwmNewAboutDialog(NULL);
			gwmSetAboutIcon(about, icon, 0, 0, 16, 16);
			gwmSetAboutCaption(about, "Tetris");
			gwmSetAboutDesc(about, "A game about arranging shapes.");
			gwmSetAboutLicense(about, GWM_LICENSE);
			gwmRunAbout(about);
		};
		return GWM_EVSTATUS_OK;
	case GWM_SYM_HELP:
		gwmTextDialog(NULL, "How to play Tetris", tetrisHelp);
		return GWM_EVSTATUS_OK;
	case GWM_SYM_HIGHSCORE:
		gwmHighScore("tetris", -1);
		return GWM_EVSTATUS_OK;
	default:
		return GWM_EVSTATUS_CONT;
	};
};

int myHandler(GWMEvent *ev, GWMWindow *win, void *context)
{
	switch (ev->type)
	{
	case GWM_EVENT_COMMAND:
		return tetrisCommand((GWMCommandEvent*) ev);
	default:
		return GWM_EVSTATUS_CONT;
	};
};

static void redrawTetris(GWMWindow *tetris)
{
	DDISurface *canvas = gwmGetWindowCanvas(tetris);
	
	Coords positions[4];
	getPiecePos(positions);
	
	int x, y;
	for (y=0; y<GRID_ROWS; y++)
	{
		for (x=0; x<GRID_COLS; x++)
		{
			ddiFillRect(canvas, x*CELL_WIDTH, y*CELL_HEIGHT, CELL_WIDTH, CELL_HEIGHT, &grid[y * GRID_COLS + x].color);
		};
	};
	
	int i;
	for (i=0; i<4; i++)
	{
		ddiFillRect(canvas, positions[i].x * CELL_WIDTH, positions[i].y * CELL_HEIGHT, CELL_WIDTH, CELL_HEIGHT,
				&currentPiece->color);
	};
	
	if (gameOver)
	{
		ddiBlit(imgGameOver, 0, 0, canvas, (canvas->width-imgGameOver->width)/2, (canvas->height-imgGameOver->height)/2,
				imgGameOver->width, imgGameOver->height);
	};
	
	gwmPostDirty(tetris);
};

void movePiece(int dx)
{
	Coords positions[4];
	getPiecePos(positions);
	
	int i;
	int occupied = 0;
	for (i=0; i<4; i++)
	{
		if (isOccupied(positions[i].x+dx, positions[i].y))
		{
			occupied = 1;
			break;
		};
	};
	
	if (!occupied)
	{
		pieceX += dx;
	};
};

void rotate()
{
	int oldRot[4];
	memcpy(oldRot, pieceRot, sizeof(pieceRot));
	
	pieceRot[0] = -oldRot[2];
	pieceRot[1] = -oldRot[3];
	pieceRot[2] = oldRot[0];
	pieceRot[3] = oldRot[1];
	
	Coords positions[4];
	getPiecePos(positions);
	
	int i;
	int occupied = 0;
	for (i=0; i<4; i++)
	{
		if (isOccupied(positions[i].x, positions[i].y))
		{
			occupied = 1;
			break;
		};
	};
	
	if (occupied)
	{
		memcpy(pieceRot, oldRot, sizeof(pieceRot));
	};
};

void checkRows()
{
	static DDIColor black = {0x00, 0x00, 0x00, 0xFF};
	
	int y;
	for (y=GRID_ROWS-1; y>=0; y--)
	{
		int full = 1;
		int x;
		for (x=0; x<GRID_COLS; x++)
		{
			if (!grid[y * GRID_COLS + x].occupied) full = 0;
		};
		
		if (full)
		{
			int my;
			for (my=y; my>=0; my--)
			{
				if (my == 0)
				{
					for (x=0; x<GRID_COLS; x++)
					{
						grid[my * GRID_COLS + x].occupied = 0;
						memcpy(&grid[my * GRID_COLS + x].color, &black, sizeof(DDIColor));
					};
				}
				else
				{
					memcpy(&grid[my * GRID_COLS], &grid[(my-1) * GRID_COLS], sizeof(Cell) * GRID_COLS);
				};
			};
			
			y++;
			score++;
		};
	};
	
	char txtbuf[64];
	sprintf(txtbuf, "Score: %d", score);
	gwmSetLabelText(lblStatus, txtbuf);
};

int tetrisHandler(GWMEvent *ev, GWMWindow *win, void *context)
{
	switch (ev->type)
	{
	case GWM_EVENT_DOWN:
		if (ev->keycode == GWM_KC_LEFT)
		{
			movePiece(-1);
		}
		else if (ev->keycode == GWM_KC_RIGHT)
		{
			movePiece(1);
		}
		else if (ev->keycode == GWM_KC_UP)
		{
			rotate();
		};
		redrawTetris(win);
		return GWM_EVSTATUS_OK;
	case GWM_EVENT_UPDATE:
		{
			if (!gameOver)
			{
				Coords positions[4];
				getPiecePos(positions);
			
				int i;
				int occupied = 0;
				for (i=0; i<4; i++)
				{
					if (isOccupied(positions[i].x, positions[i].y+1))
					{
						occupied = 1;
						break;
					};
				};
			
				if (!occupied)
				{
					pieceY++;
				}
				else
				{
					for (i=0; i<4; i++)
					{
						memcpy(&grid[positions[i].y * GRID_COLS + positions[i].x].color, &currentPiece->color, sizeof(DDIColor));
						grid[positions[i].y * GRID_COLS + positions[i].x].occupied = 1;
					};
				
					checkRows();
					nextPiece();
				};
			};
		};
		redrawTetris(win);
		return GWM_EVSTATUS_OK;
	default:
		return GWM_EVSTATUS_CONT;
	};
};

static void sizeTetris(GWMWindow *tetris, int *width, int *height)
{
	*width = CELL_WIDTH * GRID_COLS;
	*height = CELL_HEIGHT * GRID_ROWS;
};

static void positionTetris(GWMWindow *tetris, int x, int y, int width, int height)
{
	gwmMoveWindow(tetris, x, y);
	gwmResizeWindow(tetris, width, height);
	redrawTetris(tetris);
};

void resetGame()
{
	gameOver = 0;
	score = 0;
	
	static DDIColor black = {0x00, 0x00, 0x00, 0xFF};
	int i;
	for (i=0; i<GRID_ROWS*GRID_COLS; i++)
	{
		memcpy(&grid[i].color, &black, sizeof(DDIColor));
		grid[i].occupied = 0;
	};
	
	nextPiece();
	if (tetris != NULL) gwmFocus(tetris);
};

int main()
{
	srand(time(NULL));
	if (gwmInit() != 0)
	{
		fprintf(stderr, "gwmInit() failed\n");
		return 1;
	};
	
	GWMWindow *topWindow = gwmCreateWindow(
		NULL,
		"Tetris",
		GWM_POS_UNSPEC, GWM_POS_UNSPEC,
		0, 0,
		GWM_WINDOW_HIDDEN | GWM_WINDOW_NOTASKBAR
	);
	assert(topWindow != NULL);

	fntGameOver = ddiLoadFont("Roboto Mono", 25, DDI_STYLE_BOLD, NULL);
	assert(fntGameOver != NULL);
	
	DDISurface *canvas = gwmGetWindowCanvas(topWindow);
	icon = ddiLoadAndConvertPNG(&canvas->format, "/usr/share/images/tetris.png", NULL);
	if (icon != NULL) gwmSetWindowIcon(topWindow, icon);
	
	static DDIColor colGameOver = {0xFF, 0xFF, 0x00, 0xFF};
	imgGameOver = ddiRenderText(&canvas->format, fntGameOver, &colGameOver, "GAME OVER", NULL);
	assert(imgGameOver != NULL);
	
	gwmPushEventHandler(topWindow, myHandler, NULL);
	
	GWMLayout *boxLayout = gwmCreateBoxLayout(GWM_BOX_VERTICAL);
	
	GWMWindowTemplate wt;
	wt.wtComps = GWM_WTC_MENUBAR | GWM_WTC_STATUSBAR;
	wt.wtWindow = topWindow;
	wt.wtBody = boxLayout;
	gwmCreateTemplate(&wt);
	
	GWMWindow *menubar = wt.wtMenubar;
	
	GWMMenu *menuGame = gwmCreateMenu();
	gwmMenubarAdd(menubar, "Game", menuGame);
	gwmMenuAddCommand(menuGame, SYM_RESET, "Reset", NULL);
	gwmMenuAddCommand(menuGame, GWM_SYM_HIGHSCORE, NULL, NULL);
	gwmMenuAddCommand(menuGame, GWM_SYM_EXIT, NULL, NULL);
	
	GWMMenu *menuHelp = gwmCreateMenu();
	gwmMenubarAdd(menubar, "Help", menuHelp);
	gwmMenuAddCommand(menuHelp, GWM_SYM_HELP, NULL, NULL);
	gwmMenuAddCommand(menuHelp, GWM_SYM_ABOUT, NULL, NULL);
	
	lblStatus = wt.wtStatusLabel;
	gwmSetLabelText(lblStatus, "Score: 0");
	
	resetGame();
	
	tetris = gwmCreateWindow(
		topWindow,
		"Tetris",
		GWM_POS_UNSPEC, GWM_POS_UNSPEC,
		0, 0,
		0
	);
	
	tetris->getMinSize = tetris->getPrefSize = sizeTetris;
	tetris->position = positionTetris;
	
	gwmPushEventHandler(tetris, tetrisHandler, NULL);
	gwmBoxLayoutAddWindow(boxLayout, tetris, 0, 0, 0);
	gwmCreateTimer(tetris, 250);
	
	gwmFit(topWindow);
	gwmFocus(tetris);
	gwmSetWindowFlags(topWindow, 0);
	gwmMainLoop();
	gwmQuit();
	return 0;
};
