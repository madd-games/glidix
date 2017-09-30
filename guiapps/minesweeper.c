/*
	Madd Development Tools

	Copyright (c) 2017, Madd Games.
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
#include <libgwm.h>
#include <time.h>
#include <pthread.h>

#define CELL_WIDTH	15
#define CELL_HEIGHT 	15
#define	DIALOG_WIDTH	200
#define	DIALOG_HEIGHT	150

typedef struct
{
	int	isUncovered;
	int	hasMine;
	int	isFlagged;
	int	nOfMines;
} Cell;

DDISurface *imgSprites;
DDISurface *minesText[9];

int gameOver;
int gameWon;
int numFlags;
int succFlags;
int totalUncovered;
volatile int gameTime = 0;

pthread_t* timerThread;
pthread_mutex_t timeLock = PTHREAD_MUTEX_INITIALIZER;

GWMWindow *widthInput;
GWMWindow *heightInput;
GWMWindow *mineInput;

GWMWindow *win;
int width, height, totalMines;
Cell *board = NULL;


void* timerFunc(void *arg)
{
	while(1)
	{
		sleep(1);
		if(!gameOver)
		{
			pthread_mutex_lock(&timeLock);
			gameTime++;
			pthread_mutex_unlock(&timeLock);
			gwmPostUpdate(win);
		}
	}
};



void renderGame()
{
	DDISurface *canvas = gwmGetWindowCanvas(win);
	gwmClearWindow(win);
	
	for( int i = 0; i < width * height; i++)
	{
		if(!board[i].isUncovered) ddiBlit(imgSprites, 0, 0, canvas, (i % width)*CELL_WIDTH, (i / width)*CELL_HEIGHT+20, CELL_WIDTH, CELL_HEIGHT);
		
		if(board[i].isUncovered)
		{
			if((board[i].hasMine) && (gameOver)) ddiBlit(imgSprites, 30, 15, canvas, (i % width)*CELL_WIDTH, (i / width)*CELL_HEIGHT+20, CELL_WIDTH, CELL_HEIGHT);
			
			else if(!board[i].nOfMines) ddiBlit(imgSprites, 15, 0, canvas, (i % width)*CELL_WIDTH, (i / width)*CELL_HEIGHT+20, CELL_WIDTH, CELL_HEIGHT);
			
			else if(board[i].nOfMines)
			{
				ddiBlit(imgSprites, 15, 0, canvas, (i % width)*CELL_WIDTH, (i / width)*CELL_HEIGHT+20, CELL_WIDTH, CELL_HEIGHT);
				ddiBlit(minesText[board[i].nOfMines-1], 0, 0, canvas, (i % width)*CELL_WIDTH, (i / width)*CELL_HEIGHT+20, CELL_WIDTH, CELL_HEIGHT);
			}
		}
		
		if(board[i].isFlagged) ddiBlit(imgSprites, 30, 0, canvas, (i % width)*CELL_WIDTH, (i / width)*CELL_HEIGHT+20, CELL_WIDTH, CELL_HEIGHT);
		
		if((gameOver) && (board[i].hasMine))
		{
			if(!board[i].isUncovered) ddiBlit(imgSprites, 15, 15, canvas, (i % width)*CELL_WIDTH, (i / width)*CELL_HEIGHT+20, CELL_WIDTH, CELL_HEIGHT);
			if(board[i].isFlagged) ddiBlit(imgSprites, 0, 15, canvas, (i % width)*CELL_WIDTH, (i / width)*CELL_HEIGHT+20, CELL_WIDTH, CELL_HEIGHT);
		}
	}
	DDIPen *pen = ddiCreatePen(&canvas->format, gwmGetDefaultFont(), 5, width*CELL_WIDTH+25, canvas->width, 20, 0, 0, NULL);
	
	char msg[1024];
	
	pthread_mutex_lock(&timeLock);
	if ((gameOver) && (gameWon)) sprintf(msg, "Time: %02d:%02d   %d/%d/%d      Game won!", gameTime/60, gameTime%60, succFlags, numFlags, totalMines);
	else if ((gameOver) && (!gameWon)) sprintf(msg, "Time: %02d:%02d   0/%d/%d      Game over!", gameTime/60, gameTime%60, numFlags, totalMines);
	else sprintf(msg, "Time: %02d:%02d   0/%d/%d", gameTime/60, gameTime%60, numFlags, totalMines);
	pthread_mutex_unlock(&timeLock);
	
	ddiWritePen(pen, msg);
	ddiExecutePen(pen, canvas);
	ddiDeletePen(pen);
	
	
	gwmPostDirty(win);
};

void emptyFieldsFill(int i)
{
	if(((i+1)<(width*height)) && ((i+1) % width != 0))
	{
		if((!board[i+1].hasMine) && (!board[i+1].isUncovered))
		{
			if(!board[i].nOfMines)
			{
				board[i+1].isUncovered = 1;
				totalUncovered++;
				if(!board[i+1].nOfMines) emptyFieldsFill(i+1);
			}
		}
	}
	if(((i-1)>=0) && ((i-1) % width != width-1))
	{
		if((!board[i-1].hasMine) && (!board[i-1].isUncovered))
		{
			if(!board[i].nOfMines)
			{
				board[i-1].isUncovered = 1;
				totalUncovered++;
				if(!board[i-1].nOfMines) emptyFieldsFill(i-1);
			}
		}
	}
	if((i+width)<(width*height))
	{
		if((!board[i+width].hasMine) && (!board[i+width].isUncovered))
		{
			if(!board[i].nOfMines)
			{
				board[i+width].isUncovered = 1;
				totalUncovered++;
				if(!board[i+width].nOfMines) emptyFieldsFill(i+width);
			}
		}
	}
	if(((i+width-1)<(width*height)) && ((i+width-1) % width != width-1))
	{
		if((!board[i+width-1].hasMine) && (!board[i+width-1].isUncovered))
		{
			if(!board[i].nOfMines)
			{
				board[i+width-1].isUncovered = 1;
				totalUncovered++;
				if(!board[i+width-1].nOfMines) emptyFieldsFill(i+width-1);
			}
		}
	}
	if(((i+width+1)<(width*height)) && ((i+width+1) % width != 0))
	{
		if((!board[i+width+1].hasMine) && (!board[i+width+1].isUncovered))
		{
			if(!board[i].nOfMines)
			{
				board[i+width+1].isUncovered = 1;
				totalUncovered++;
				if(!board[i+width+1].nOfMines) emptyFieldsFill(i+width+1);
			}
		}
	}
	if((i-width)>=0)
	{
		if((!board[i-width].hasMine) && (!board[i-width].isUncovered))
		{
			if(!board[i].nOfMines)
			{
				board[i-width].isUncovered = 1;
				totalUncovered++;
				if(!board[i-width].nOfMines) emptyFieldsFill(i-width);
			}
		}
	}
	if(((i-width-1)>=0) && ((i-width-1) % width != width-1))
	{
		if((!board[i-width-1].hasMine) && (!board[i-width-1].isUncovered))
		{
			if(!board[i].nOfMines)
			{
				board[i-width-1].isUncovered = 1;
				totalUncovered++;
				if(!board[i-width-1].nOfMines) emptyFieldsFill(i-width-1);
			}
		}
	}
	if(((i-width+1)>=0) && ((i-width+1) % width != 0))
	{
		if((!board[i-width+1].hasMine) && (!board[i-width+1].isUncovered))
		{
			if(!board[i].nOfMines)
			{
				board[i-width+1].isUncovered = 1;
				totalUncovered++;
				if(!board[i-width+1].nOfMines) emptyFieldsFill(i-width+1);
			}
		}
	}
};

void leftClick(int x, int y)
{
	if((x<0) || (x>=width*CELL_WIDTH) || (y<20) || ((y-20)>=height*CELL_HEIGHT))
	{
		return;
	}
	int i = x/CELL_WIDTH + ((y-20)/CELL_HEIGHT)*width;
	if((!board[i].isUncovered) && (!board[i].isFlagged))
	{
		board[i].isUncovered = 1;
		if(board[i].hasMine == 1)
		{
			gameOver = 1;
		}
		else 
		{
			totalUncovered++;
			emptyFieldsFill(i);
		}
		if((width*height - totalMines) == totalUncovered)
		{
			gameWon = 1;
			gameOver = 1;
		}
	};
};

void rightClick(int x, int y)
{
	if((x<0) || (x>=width*CELL_WIDTH) || (y<20) || ((y-20)>=height*CELL_HEIGHT))
	{
		return;
	}
	int i = x/CELL_WIDTH + ((y-20)/CELL_HEIGHT)*width;
	if(!board[i].isUncovered)
	{
		if (!board[i].isFlagged)
		{
			board[i].isFlagged = 1;
			numFlags++;
			if(board[i].hasMine) succFlags++;
		}
		else 
		{
			board[i].isFlagged = 0;
			numFlags--;
			if(board[i].hasMine) succFlags--;
		}
	};
};


int eventHandler(GWMEvent *ev, GWMWindow *win)
{
	switch (ev->type)
	{
	case GWM_EVENT_CLOSE:
		return -1;
	case GWM_EVENT_UP:
		if ((ev->scancode == GWM_SC_MOUSE_LEFT) && (!gameOver))
		{
			leftClick(ev->x, ev->y);
			renderGame();
		}
		else if ((ev->scancode == GWM_SC_MOUSE_RIGHT) && (!gameOver))
		{
			rightClick(ev->x, ev->y);
			renderGame();
		};
		return 0;
	case GWM_EVENT_UPDATE:
		renderGame();
		return 0;
	default:
		return gwmDefaultHandler(ev, win);
	};
};

int dialogHandler(GWMEvent *ev, GWMWindow *win)
{
	switch (ev->type)
	{
	case GWM_EVENT_CLOSE:
		return -1;
	default:
		return gwmDefaultHandler(ev, win);
	};
	
};

int getTextFieldInt(GWMWindow* input)
{
	size_t size = gwmGetTextFieldSize(input);
	if (size == 0) return -1;
	
	char *buffer = (char*) malloc(size+1);
	buffer[size] = 0;
	gwmReadTextField(input, buffer, 0, (off_t)size);
	
	int result, count;
	int elements = sscanf(buffer, "%d%n", &result, &count);
	if ((elements < 1) || (count != strlen(buffer)))
	{
		free(buffer);
		printf("result=%d, count=%d, elements=%d, buffer=[%s]\n", result, count, elements, buffer);
		gwmMessageBox(NULL, "Settings error", "Failed to input value", GWM_MBICON_ERROR | GWM_MBUT_OK);
		return -1;
	}
	else if (result < 10) 
	{
		free(buffer);
		return 10;
	}
	else if (result > 99)
	{
		free(buffer);
		return 99;
	}
	else
	{
		free(buffer);
		return result;
	};
};


void resetBoard()
{
	free(board);
	board = (Cell*) malloc(width * height * sizeof(Cell));
	int i;
	int j;
	int k;
	for ( i = 0; i < width * height; i++)
	{
		if (i<totalMines) board[i].hasMine = 1;
		else board[i].hasMine = 0;
		board[i].isUncovered = 0;
		board[i].isFlagged = 0;
		board[i].nOfMines = 0;
	};
	
	for ( i = 0; i < width * height; i++)
	{
		j = i + rand() % (width * height - i);
		k = board[i].hasMine;
		board[i].hasMine = board[j].hasMine;
		board[j].hasMine = k;
	};
	
	for ( i = 0; i < width * height; i++)
	{
		if(((i+1)<(width*height)) && ((i+1) % width != 0))
		{
			if(board[i+1].hasMine == 1) board[i].nOfMines++;
		}
		if(((i-1)>=0) && ((i-1) % width != width-1))
		{
			if(board[i-1].hasMine == 1) board[i].nOfMines++;
		}
		if((i+width)<(width*height))
		{
			if(board[i+width].hasMine == 1) board[i].nOfMines++;
		}
		if(((i+width-1)<(width*height)) && ((i+width-1) % width != width-1))
		{
			if(board[i+width-1].hasMine == 1) board[i].nOfMines++;
		}
		if(((i+width+1)<(width*height)) && ((i+width+1) % width != 0))
		{
			if(board[i+width+1].hasMine == 1) board[i].nOfMines++;
		}
		if((i-width)>=0)
		{
			if(board[i-width].hasMine == 1) board[i].nOfMines++;
		}
		if(((i-width-1)>=0) && ((i-width-1) % width != width-1))
		{
			if(board[i-width-1].hasMine == 1) board[i].nOfMines++;
		}
		if(((i-width+1)>=0) && ((i-width+1) % width != 0))
		{
			if(board[i-width+1].hasMine == 1) board[i].nOfMines++;
		}
	};
	
	gameWon = 0;
	gameOver = 0;
	numFlags = 0;
	succFlags = 0;
	totalUncovered = 0;
	
	pthread_mutex_lock(&timeLock);
	gameTime = 0;
	pthread_mutex_unlock(&timeLock);
	
	gwmResizeWindow(win, CELL_WIDTH * width, CELL_HEIGHT * height + 40);	
};

int inputOK(void *context)
{
	
	int resultW = getTextFieldInt(widthInput);
	if(resultW == -1) return 0;
	int resultH = getTextFieldInt(heightInput);
	if(resultH == -1) return 0;
	int resultM = getTextFieldInt(mineInput);
	if(resultM == -1) return 0;
	if (resultW * resultH <= resultM)
	{
		gwmMessageBox(NULL, "Settings error", "More mines than fields entered!", GWM_MBICON_ERROR | GWM_MBUT_OK);
	}
	else
	{
		width = resultW;
		height = resultH;
		totalMines = resultM;
	}
	
	resetBoard();
	renderGame();

	return -1;
};

int inputCancel(void *context)
{
	return -1;
};

int onSettings(void *ignore)
{
	int mineX, mineY;
	gwmRelToAbs(win, 0, 0, &mineX, &mineY);
	GWMWindow *dialog = gwmCreateModal("Settings", mineX+20, mineY+20, DIALOG_WIDTH, DIALOG_HEIGHT);
	
	char txtBuffer[64];
	
	DDISurface *canvas = gwmGetWindowCanvas(dialog);
	
	DDIPen *pen = ddiCreatePen(&canvas->format, gwmGetDefaultFont(), 2, 4, canvas->width, 20, 0, 0, NULL);
	
	ddiWritePen(pen, "Width: \n\n Height: \n\n Mines:");
	ddiExecutePen(pen, canvas);
	ddiDeletePen(pen);
	
	
	gwmPostDirty(dialog);
	

	sprintf(txtBuffer, "%d", width);
	widthInput = gwmCreateTextField(dialog, txtBuffer, 55, 4, DIALOG_WIDTH-72, 0);
	sprintf(txtBuffer, "%d", height);
	heightInput = gwmCreateTextField(dialog, txtBuffer, 55, 20+4+6, DIALOG_WIDTH-72, 0);
	sprintf(txtBuffer, "%d", totalMines);
	mineInput = gwmCreateTextField(dialog, txtBuffer, 55, 20*2+4+6*2, DIALOG_WIDTH-72, 0);
	
	int buttonWidth = (DIALOG_WIDTH-6)/2;
	GWMWindow *btnOK = gwmCreateButton(dialog, "OK", 2, 20*3+34, buttonWidth, 0);
	gwmSetButtonCallback(btnOK, inputOK, dialog);
	GWMWindow *btnCancel = gwmCreateButton(dialog, "Cancel", 4+buttonWidth, 20*3+34, buttonWidth, 0);
	gwmSetButtonCallback(btnCancel, inputCancel, dialog);
	
	gwmSetEventHandler(dialog, dialogHandler);
	
	gwmRunModal(dialog, GWM_WINDOW_NOTASKBAR | GWM_WINDOW_NOICON);

	gwmDestroyTextField(widthInput);
	gwmDestroyTextField(heightInput);
	gwmDestroyTextField(mineInput);
	gwmDestroyButton(btnOK);
	gwmDestroyButton(btnCancel);
	gwmDestroyWindow(dialog);
	return 0;
};

int onReset(void *ignore)
{
	resetBoard();
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
	gwmMenuAddEntry(menuGame, "Settings", onSettings, NULL);
	gwmMenuAddEntry(menuGame, "Reset", onReset, NULL);
	gwmMenuAddEntry(menuGame, "Exit", onExit, NULL);
	
	gwmMenubarAdd(menubar, "Game", menuGame);
	gwmMenubarAdjust(menubar);
};


int main()
{
	srand(time(NULL));
	
	if(gwmInit() !=0)
	{
		gwmMessageBox(NULL, "GWM Error", "Failed to initialize GWM!", GWM_MBICON_ERROR | GWM_MBUT_OK);
		return 1;
	}
	win = gwmCreateWindow(NULL, "Minesweeper", 10, 10, 200, 200, GWM_WINDOW_NOTASKBAR | GWM_WINDOW_HIDDEN);
	if (win == NULL)
	{
		gwmMessageBox(NULL, "GWM Error", "Failed to create window!", GWM_MBICON_ERROR | GWM_MBUT_OK);
		return 1;
	};

	width = 20;
	height = 20;
	totalMines = 50;
	
	const char *error;
	
	DDISurface *canvas = gwmGetWindowCanvas(win);
	DDISurface *icon = ddiLoadAndConvertPNG(&canvas->format, "/usr/share/images/minesweeper.png", NULL);
	if (icon != NULL)
	{
		gwmSetWindowIcon(win, icon);
		ddiDeleteSurface(icon);
	};
	
	if ((imgSprites = ddiLoadAndConvertPNG(&canvas->format, "/usr/share/images/sprite-mines.png", &error)) == NULL) 
	{
		gwmMessageBox(NULL, "Sprite error", "Failed to load sprite!", GWM_MBICON_ERROR | GWM_MBUT_OK);
		fprintf(stderr, "Failed to load sprite file: %s\n", error);
		return 0;
	};
	if ((minesText[0] = ddiRenderText(&canvas->format, gwmGetDefaultFont(), "1", &error)) == NULL)
	{
		gwmMessageBox(NULL, "Text render error", "Failed to render text!", GWM_MBICON_ERROR | GWM_MBUT_OK);
		fprintf(stderr, "Failed to render text: %s\n", error);
		return 0;
	};
	if ((minesText[1] = ddiRenderText(&canvas->format, gwmGetDefaultFont(), "2", &error)) == NULL)
	{
		gwmMessageBox(NULL, "Text render error", "Failed to render text!", GWM_MBICON_ERROR | GWM_MBUT_OK);
		fprintf(stderr, "Failed to render text: %s\n", error);
		return 0;
	};
	if ((minesText[2] = ddiRenderText(&canvas->format, gwmGetDefaultFont(), "3", &error)) == NULL)
	{
		gwmMessageBox(NULL, "Text render error", "Failed to render text!", GWM_MBICON_ERROR | GWM_MBUT_OK);
		fprintf(stderr, "Failed to render text: %s\n", error);
		return 0;
	};
	if ((minesText[3] = ddiRenderText(&canvas->format, gwmGetDefaultFont(), "4", &error)) == NULL)
	{
		gwmMessageBox(NULL, "Text render error", "Failed to render text!", GWM_MBICON_ERROR | GWM_MBUT_OK);
		fprintf(stderr, "Failed to render text: %s\n", error);
		return 0;
	};
	if ((minesText[4] = ddiRenderText(&canvas->format, gwmGetDefaultFont(), "5", &error)) == NULL)
	{
		gwmMessageBox(NULL, "Text render error", "Failed to render text!", GWM_MBICON_ERROR | GWM_MBUT_OK);
		fprintf(stderr, "Failed to render text: %s\n", error);
		return 0;
	};
	if ((minesText[5] = ddiRenderText(&canvas->format, gwmGetDefaultFont(), "6", &error)) == NULL)
	{
		gwmMessageBox(NULL, "Text render error", "Failed to render text!", GWM_MBICON_ERROR | GWM_MBUT_OK);
		fprintf(stderr, "Failed to render text: %s\n", error);
		return 0;
	};
	if ((minesText[6] = ddiRenderText(&canvas->format, gwmGetDefaultFont(), "7", &error)) == NULL)
	{
		gwmMessageBox(NULL, "Text render error", "Failed to render text!", GWM_MBICON_ERROR | GWM_MBUT_OK);
		fprintf(stderr, "Failed to render text: %s\n", error);
		return 0;
	};
	if ((minesText[7] = ddiRenderText(&canvas->format, gwmGetDefaultFont(), "8", &error)) == NULL)
	{
		gwmMessageBox(NULL, "Text render error", "Failed to render text!", GWM_MBICON_ERROR | GWM_MBUT_OK);
		fprintf(stderr, "Failed to render text: %s\n", error);
		return 0;
	};
	
	pthread_create(timerThread, NULL,  timerFunc, NULL);

	resetBoard();
	renderGame();
	setupMenus();

	gwmSetEventHandler(win, eventHandler);
	gwmSetWindowFlags(win, GWM_WINDOW_MKFOCUSED);
	gwmMainLoop();
	gwmQuit();
	return 0;
};