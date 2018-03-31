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

#include <dirent.h>
#include <stdio.h>
#include <stdlib.h>
#include <errno.h>
#include <string.h>
#include <fcntl.h>
#include <unistd.h>
#include <libgwm.h>

#include "input.h"
#include "window.h"

// kernel headers must always come last
#include <glidix/humin/humin.h>

static void* inputThreadFunc(void *context)
{
	int fd = (int) (uintptr_t) context;
	
	HuminEvent hev;
	while (1)
	{
		if (read(fd, &hev, sizeof(HuminEvent)) == -1)
		{
			continue;
		};
		
		if (hev.type == HUMIN_EV_BUTTON_DOWN)
		{
			// mouse
			if (hev.button.keycode == GWM_KC_MOUSE_LEFT)
			{
				wndOnLeftDown();
			};
			
			wndInputEvent(GWM_EVENT_DOWN, hev.button.scancode, hev.button.keycode);
		}
		else if (hev.type == HUMIN_EV_BUTTON_UP)
		{
			// mouse
			if (hev.button.keycode == GWM_KC_MOUSE_LEFT)
			{
				wndOnLeftUp();
			};
			
			wndInputEvent(GWM_EVENT_UP, hev.button.scancode, hev.button.keycode);
		};
	};
	
	return NULL;
};

void inputInit()
{
	DIR *dirp = opendir("/dev");
	if (dirp == NULL)
	{
		fprintf(stderr, "[gwmserver] cannot scan /dev: %s: input subsystem disabled\n", strerror(errno));
		return;
	};
	
	struct dirent *ent;
	while ((ent = readdir(dirp)) != NULL)
	{
		if (memcmp(ent->d_name, "humin", 5) == 0)
		{
			char fullpath[256];
			sprintf(fullpath, "/dev/%s", ent->d_name);
			
			int fd = open(fullpath, O_RDWR);
			if (fd == -1)
			{
				fprintf(stderr, "[gwmserver] cannot open %s: %s\n", fullpath, strerror(errno));
				continue;
			};
			
			pthread_t thread;
			int errnum = pthread_create(&thread, NULL, inputThreadFunc, (void*) (uintptr_t) fd);
			if (errnum != 0)
			{
				fprintf(stderr, "[gwmserver] cannot create input thread: %s\n", strerror(errnum));
				close(fd);
				continue;
			};
			
			pthread_detach(thread);
		};
	};
	
	closedir(dirp);
};
