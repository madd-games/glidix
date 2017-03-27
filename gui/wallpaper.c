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
#include <stdlib.h>
#include <libgwm.h>
#include <libddi.h>
#include <string.h>

enum
{
	OP_CENTER,
	OP_SCALE,
};

int startsWith(const char *str, const char *prefix)
{
	if (strlen(str) < strlen(prefix))
	{
		return 0;
	};
	
	return memcmp(str, prefix, strlen(prefix)) == 0;
};

int main(int argc, char *argv[])
{
	int layoutOp = OP_CENTER;
	char *imagePath = NULL;
	DDIColor color = {0, 0, 0x77, 0xFF};
	
	int i;
	for (i=1; i<argc; i++)
	{
		if (startsWith(argv[i], "--color="))
		{
			if (ddiParseColor(&argv[i][8], &color) != 0)
			{
				fprintf(stderr, "%s: invalid color name: %s\n", argv[0], &argv[i][8]);
				return 1;
			};
		}
		else if (startsWith(argv[i], "--image="))
		{
			imagePath = &argv[i][8];
		}
		else if (strcmp(argv[i], "--scale") == 0)
		{
			layoutOp = OP_SCALE;
		}
		else
		{
			fprintf(stderr, "%s: unrecognised command-line option: %s\n", argv[0], argv[i]);
			return 1;
		};
	};
	
	if (gwmInit() != 0)
	{
		fprintf(stderr, "%s: failed to initialize GWM\n", argv[0]);
		return 1;
	};
	
	GWMInfo *info = gwmGetInfo();
	DDISurface *background = ddiOpenSurface(info->backgroundID);
	
	if (imagePath != NULL)
	{
		const char *error;
		DDISurface *image = ddiLoadAndConvertPNG(&background->format, imagePath, &error);
		
		if (image == NULL)
		{
			fprintf(stderr, "%s: cannot load image %s: %s\n", argv[0], imagePath, error);
			ddiDeleteSurface(background);
			gwmQuit();
			return 1;
		};
		
		ddiFillRect(background, 0, 0, background->width, background->height, &color);
		
		if (layoutOp == OP_CENTER)
		{
			ddiBlit(image, 0, 0, background, (background->width-image->width)/2, (background->height-image->height)/2, background->width, background->height);
		}
		else if (layoutOp == OP_SCALE)
		{
			DDISurface *scaled = ddiScale(image, background->width, background->height, DDI_SCALE_BEST);
			ddiBlit(scaled, 0, 0, background, 0, 0, background->width, background->height);
		};
	}
	else
	{
		ddiFillRect(background, 0, 0, background->width, background->height, &color);
	};
	
	ddiDeleteSurface(background);
	gwmRedrawScreen();
	gwmQuit();
	
	return 0;
};
