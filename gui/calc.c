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
#include <libgwm.h>

GWMWindow *win;

enum
{
	OP_NONE,
	OP_ADD,
	OP_SUB,
	OP_MUL,
	OP_DIV
};

long lastNumber = 0;
long currentNumber = 0;
int selectedOp = OP_NONE;

int myEventHandler(GWMEvent *ev, GWMWindow *win)
{
	switch (ev->type)
	{
	case GWM_EVENT_CLOSE:
		return -1;
	default:
		return gwmDefaultHandler(ev, win);
	};
};

void drawScreen()
{
	DDISurface *canvas = gwmGetWindowCanvas(win);
	
	DDIColor borderColor = {0, 0, 0, 0xFF};
	DDIColor fillColor = {0xFF, 0xFF, 0xFF, 0xFF};
	
	ddiFillRect(canvas, 2, 2, 126, 22, &borderColor);
	ddiFillRect(canvas, 3, 3, 124, 20, &fillColor);
	
	char text[256];
	sprintf(text, "%ld", currentNumber);
	
	DDIPen *pen = ddiCreatePen(&canvas->format, gwmGetDefaultFont(), 5, 3, 120, 20, 0, 0, NULL);
	ddiSetPenAlignment(pen, DDI_ALIGN_RIGHT);
	ddiWritePen(pen, text);
	ddiExecutePen(pen, canvas);
	ddiDeletePen(pen);
	
	gwmPostDirty(win);
};

long numgrid[3*3+1] = {
	7, 8, 9,
	4, 5, 6,
	1, 2, 3,
	0
};

int opList[4] = {OP_DIV, OP_MUL, OP_SUB, OP_ADD};
const char *opLabels[4] = {"/", "*", "-", "+"};

int onNumberButton(void *ptr)
{
	long num = *((long*)ptr);
	currentNumber = currentNumber * 10L + num;
	drawScreen();
	return 0;
};

int onOpButton(void *ptr)
{
	lastNumber = currentNumber;
	currentNumber = 0L;
	selectedOp = *((int*)ptr);
	
	return 0;
};

int onEqualsButton(void *ignore)
{
	(void)ignore;
	
	switch (selectedOp)
	{
	case OP_NONE:
		break;
	case OP_ADD:
		currentNumber = lastNumber + currentNumber;
		selectedOp = OP_NONE;
		break;
	case OP_SUB:
		currentNumber = lastNumber - currentNumber;
		selectedOp = OP_NONE;
		break;
	case OP_MUL:
		currentNumber = lastNumber * currentNumber;
		selectedOp = OP_NONE;
		break;
	case OP_DIV:
		if (currentNumber == 0)
		{
			gwmMessageBox(NULL, "Calculator", "Cannot divide by zero!", GWM_MBICON_ERROR | GWM_MBUT_OK);
		}
		else
		{
			currentNumber = lastNumber / currentNumber;
			selectedOp = OP_NONE;
		};
		break;
	};
	
	drawScreen();
	return 0;
};

int main()
{
	if (gwmInit() != 0)
	{
		fprintf(stderr, "Failed to initialize GWM!\n");
		return 1;
	};

	win = gwmCreateWindow(NULL, "Calculator", 0, 0, 130, 154, GWM_WINDOW_NOTASKBAR | GWM_WINDOW_HIDDEN);
	if (win == NULL)
	{
		fprintf(stderr, "Failed to create window!\n");
		return 1;
	};
	
	drawScreen();
	
	DDISurface *canvas = gwmGetWindowCanvas(win);
	DDISurface *icon = ddiLoadAndConvertPNG(&canvas->format, "/usr/share/images/calc.png", NULL);
	if (icon != NULL)
	{
		gwmSetWindowIcon(win, icon);
		ddiDeleteSurface(icon);
	};
	
	GWMWindow *button;
	int x, y;
	for (x=0; x<3; x++)
	{
		for (y=0; y<3; y++)
		{
			char str[16];
			sprintf(str, "%ld", numgrid[y*3+x]);
			button = gwmCreateButton(win, str, 2+32*x, 26+32*y, 30, 0);
			gwmSetButtonCallback(button, onNumberButton, &numgrid[y*3+x]);
		};
	};
	
	button = gwmCreateButton(win, "0", 2, 26+32*3, 30, 0);
	gwmSetButtonCallback(button, onNumberButton, &numgrid[3*3]);
	
	int i;
	for (i=0; i<4; i++)
	{
		button = gwmCreateButton(win, opLabels[i], 2+32*3, 26+32*i, 30, 0);
		gwmSetButtonCallback(button, onOpButton, &opList[i]);
	};
	
	button = gwmCreateButton(win, "=", 2+32*2, 26+32*3, 30, 0);
	gwmSetButtonCallback(button, onEqualsButton, NULL);
	
	button = gwmCreateButton(win, ".", 2+32*1, 26+32*3, 30, GWM_BUTTON_DISABLED);
	
	gwmSetWindowFlags(win, GWM_WINDOW_MKFOCUSED);
	
	gwmSetEventHandler(win, myEventHandler);
	gwmMainLoop();
	gwmQuit();
	return 0;
};

