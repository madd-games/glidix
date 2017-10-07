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

int main(int argc, char *argv[])
{
	if (argc != 2)
	{
		fprintf(stderr, "USAGE:\t%s <display-device>\n", argv[0]);
		fprintf(stderr, "\tPrints information about a display device.\n");
		return 1;
	};
	
	if (ddiInit(argv[1], O_RDONLY) != 0)
	{
		fprintf(stderr, "%s: failed to open display device %s: %s\n", argv[0], argv[1], strerror(errno));
		return 1;
	};
	
	printf(" *** DISPLAY INFO ***\n");
	printf("Display device:          %s\n", argv[1]);
	printf("Renderer:                %s\n", ddiDisplayInfo.renderer);
	printf("Presentable surfaces:    %d\n", ddiDisplayInfo.numPresentable);
	
	printf("\n *** DDI DRIVER INFO ***\n");
	printf("Size of DDIDriver:       %lu (mine %lu)\n", ddiDriver->size, sizeof(DDIDriver));
	printf("Renderer string:         %s\n", ddiDriver->renderString);
	
	ddiQuit();
	return 0;
};
