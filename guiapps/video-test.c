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

#include <libddi.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <errno.h>
#include <fcntl.h>
#include <time.h>
#include <unistd.h>

int main(int argc, char *argv[])
{
	if (argc != 2)
	{
		fprintf(stderr, "USAGE:\t%s <display-device>\n", argv[0]);
		fprintf(stderr, "\tOpen graphics mode in the device for 5 seconds, showing a\n");
		fprintf(stderr, "\tpurple screen, then exit back to graphics mode.\n");
		return 1;
	};
	
	if (ddiInit(argv[1], O_RDWR) != 0)
	{
		fprintf(stderr, "%s: failed to open %s: %s\n", argv[0], argv[1], strerror(errno));
		return 1;
	};
	
	DDISurface *screen = ddiSetVideoMode(DDI_RES_AUTO);
	if (screen == NULL)
	{
		fprintf(stderr, "%s: failed to set graphics mode: %s\n", argv[0], strerror(errno));
		ddiQuit();
		return 1;
	};
	
	DDIColor color = {0x77, 0x00, 0x77, 0xFF};
	ddiFillRect(screen, 0, 0, screen->width, screen->height, &color);
	
	sleep(5);
	
	ddiQuit();
	return 0;
};
