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
#include <pthread.h>
#include <fcntl.h>
#include <unistd.h>
#include <signal.h>
#include "gui.h"

void printWindowInfo(const char *prefix, GWMGlobWinRef *ref)
{
	char exepath[256];
	char linkpath[256];

	sprintf(exepath, "/proc/%d/exe", ref->pid);
	strcpy(linkpath, "???");
	
	if (ref->pid != 0)
	{
		ssize_t sz = readlink(exepath, linkpath, 256);
		linkpath[sz] = 0;
	}
	else
	{
		strcpy(linkpath, "<null>");
	};

	GWMWindowParams params;
	if (gwmGetGlobWinParams(ref, &params) == -1)
	{
		params.width = 0;
		params.height = 0;
		strcpy(params.caption, "???");
	};
	
	printf("%s[%d:%d:%lu] belonging to `%s': %ux%u '%s'\n", prefix, ref->pid, ref->fd, ref->id, linkpath, params.width, params.height, params.caption);
};

int main()
{
	if (gwmInit() != 0)
	{
		fprintf(stderr, "Failed to initialize GWM!\n");
		return 1;
	};
	
	GWMGlobWinRef wins[128];
	GWMGlobWinRef focused;
	
	int count = gwmGetDesktopWindows(&focused, wins);
	printf("There are %d windows open\n", count);
	
	if (focused.pid != 0)
	{
		printWindowInfo("Focused window: ", &focused);
	};
	
	int i;
	for (i=0; i<count; i++)
	{
		printWindowInfo("", &wins[i]);
	};
	
	gwmQuit();
	return 0;
};
