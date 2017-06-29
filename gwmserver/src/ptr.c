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
#include <dirent.h>
#include <stdlib.h>
#include <errno.h>
#include <unistd.h>
#include <fcntl.h>

#include "ptr.h"
#include "screen.h"
#include "window.h"

static void* ptrThreadFunc(void *context)
{
	int fd = (int) (uintptr_t) context;

	_glidix_ptrstate state;
	state.width = screen->width;
	state.height = screen->height;
	state.posX = screen->width/2;
	state.posY = screen->height/2;
	write(fd, &state, sizeof(_glidix_ptrstate));
	
	while (1)
	{
		if (read(fd, &state, sizeof(_glidix_ptrstate)) < 0)
		{
			continue;
		};
		
		wndMouseMove(state.posX, state.posY);
	};
	
	return NULL;
};

void ptrInit()
{
	DIR *dirp = opendir("/dev");
	if (dirp == NULL)
	{
		fprintf(stderr, "[gwmserver] failed to scan /dev (%s): mouse input disabled\n", strerror(errno));
		return;
	};
	
	struct dirent *ent;
	while ((ent = readdir(dirp)) != NULL)
	{
		if (memcmp(ent->d_name, "ptr", 3) == 0)
		{
			char fullpath[PATH_MAX];
			sprintf(fullpath, "/dev/%s", ent->d_name);
			
			int fd = open(fullpath, O_RDWR);
			if (fd == -1)
			{
				fprintf(stderr, "[gwmserver] failed to open %s: %s\n", fullpath, strerror(errno));
				continue;
			};
			
			pthread_t thread;
			int errnum = pthread_create(&thread, NULL, ptrThreadFunc, (void*) (uintptr_t) fd);
			if (errnum != 0)
			{
				fprintf(stderr, "[gwmserver] cannot create pointer thread: %s\n", strerror(errnum));
				close(fd);
				continue;
			};
			
			pthread_detach(thread);
		};
	};
	
	closedir(dirp);
};
