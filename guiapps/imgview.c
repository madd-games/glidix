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

#define	MIN_WIDTH			100
#define	MIN_HEIGHT			100

#define	MAX_WIDTH			640
#define	MAX_HEIGHT			480

DDISurface *makeCheckerboard(DDIPixelFormat *format, int width, int height)
{
	DDISurface *board = ddiCreateSurface(format, width, height, NULL, 0);
	
	DDIColor colors[2] = {
		{0xFF, 0xFF, 0xFF, 0xFF},
		{0x7F, 0x7F, 0x7F, 0xFF}
	};
	
	int x, y;
	for (y=0; y<height; y+=8)
	{
		for (x=0; x<width; x+=8)
		{
			int index = ((x >> 3) & 1) ^ ((y >> 3) & 1);
			ddiFillRect(board, x, y, 8, 8, &colors[index]);
		};
	};
	
	return board;
};

int main(int argc, char *argv[])
{
	if (gwmInit() != 0)
	{
		fprintf(stderr, "%s: failed to initialize GWM!\n", argv[0]);
		return 1;
	};
	
	const char *path;
	if (argc < 2)
	{
		GWMWindow *fc = gwmCreateFileChooser(NULL, "Select image file", GWM_FILE_OPEN);
		path = gwmRunFileChooser(fc);
		if (path == NULL) return 1;
	}
	else
	{
		path = argv[1];
	};
	
	DDIPixelFormat format;
	gwmGetScreenFormat(&format);
	
	const char *error;
	DDISurface *image = ddiLoadAndConvertPNG(&format, path, &error);
	if (image == NULL)
	{
		char message[4096];
		sprintf(message, "Failed to load image %s: %s", path, error);
		gwmMessageBox(NULL, "Image Viewer", message, GWM_MBUT_OK | GWM_MBICON_ERROR);
		gwmQuit();
		return 1;
	};
	
	int winWidth, winHeight;
	if (image->width < MIN_WIDTH)
	{
		winWidth = MIN_WIDTH;
	}
	else if (image->width > MAX_WIDTH)
	{
		winWidth = MAX_WIDTH;
	}
	else
	{
		winWidth = image->width;
	};
	
	if (image->height < MIN_HEIGHT)
	{
		winHeight = MIN_HEIGHT;
	}
	else if (image->height > MAX_HEIGHT)
	{
		winHeight = MAX_HEIGHT;
	}
	else
	{
		winHeight = image->height;
	};
	
	char caption[1024];
	const char *base = path;
	char *slashPos = strrchr(path, '/');
	if (slashPos != NULL) base = slashPos+1;
	sprintf(caption, "%s (%dx%d) - Image Viewer", base, image->width, image->height);
	GWMWindow *win = gwmCreateWindow(NULL, caption, GWM_POS_UNSPEC, GWM_POS_UNSPEC, winWidth, winHeight,
		GWM_WINDOW_HIDDEN | GWM_WINDOW_NOTASKBAR);
	DDISurface *canvas = gwmGetWindowCanvas(win);
	
	int newWidth = image->width;
	int newHeight = image->height;
	if (newWidth > MAX_WIDTH) newWidth = MAX_WIDTH;
	if (newHeight > MAX_HEIGHT) newHeight = MAX_HEIGHT;
	
	if (newWidth != image->width || newHeight != image->height)
	{
		DDISurface *newImage = ddiScale(image, newWidth, newHeight, DDI_SCALE_BEST);
		ddiDeleteSurface(image);
		image = newImage;
	};
	
	DDIColor black = {0x00, 0x00, 0x00, 0xFF};
	ddiFillRect(canvas, 0, 0, winWidth, winHeight, &black);
	
	DDISurface *board = makeCheckerboard(&canvas->format, image->width, image->height);
	ddiOverlay(board, 0, 0, canvas, (canvas->width - board->width)/2, (canvas->height - board->height)/2,
		board->width, board->height);
	ddiBlit(image, 0, 0, canvas, (canvas->width - image->width)/2, (canvas->height - image->height)/2,
		image->width, image->height);
	
	ddiDeleteSurface(image);
	ddiDeleteSurface(board);
	
	DDISurface *icon = ddiLoadAndConvertPNG(&canvas->format, "/usr/share/images/imgview.png", NULL);
	if (icon != NULL)
	{
		gwmSetWindowIcon(win, icon);
		ddiDeleteSurface(icon);
	};
	
	gwmPostDirty(win);
	gwmSetWindowFlags(win, GWM_WINDOW_MKFOCUSED);
	gwmMainLoop();
	gwmQuit();
	return 0;
};
