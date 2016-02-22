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
#include "gui.h"

int main()
{
	printf("Waiting for GUI...\n");
	FILE *fp = NULL;
	
	while (1)
	{
		fp = fopen("/usr/share/gui.pid", "rb");
		if (fp == NULL) continue;
		int pid, fd;
		int count = (int) fscanf(fp, "%d.%d", &pid, &fd);
		if (count != 2)
		{
			printf("count is %d\n", count);
			fclose(fp);
			continue;
		};
		fclose(fp);
		break;
	};
	
	gwmInit();
	printf("About to call gwmCreateWindow\n");
	GWMWindowHandle wnd = gwmCreateWindow(NULL, "Hello world meme", 10, 10, 200, 200, 0);
	if (wnd == NULL) fprintf(stderr, "Error\n");
	printf("end of gwmCreateWindow\n");
	while (1) pause();
	
	return 0;
};
