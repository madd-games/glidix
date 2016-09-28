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

#include <stdio.h>
#include <libgwm.h>
#include <time.h>
#include <sys/time.h>
#include <stdlib.h>
#include <string.h>
#include <signal.h>
#include <unistd.h>
#include <dirent.h>
#include <pthread.h>
#include <poll.h>
#include <pwd.h>

#define	DEFAULT_WIDTH			500
#define	DEFAULT_HEIGHT			500

GWMWindow *winMain;
GWMWindow *txtPath;

int main(int argc, char *argv[])
{
	if (argc == 2)
	{
		chdir(argv[1]);
	}
	else
	{
		struct passwd *pwent = getpwuid(getuid());
		if (pwent != NULL)
		{
			chdir(pwent->pw_dir);
		};
	};
	
	if (gwmInit() != 0)
	{
		fprintf(stderr, "Failed to initialize GWM!\n");
		return 1;
	};
	
	winMain = gwmCreateWindow(NULL, "File Manager", GWM_POS_UNSPEC, GWM_POS_UNSPEC, DEFAULT_WIDTH, DEFAULT_HEIGHT,
					GWM_WINDOW_NOTASKBAR | GWM_WINDOW_HIDDEN);
	
	DDISurface *canvas = gwmGetWindowCanvas(winMain);
	DDISurface *iconFileMgr = ddiLoadAndConvertPNG(&canvas->format, "/usr/share/images/fileman.png", NULL);
	gwmSetWindowIcon(winMain, iconFileMgr);
	
	char cwd[PATH_MAX];
	getcwd(cwd, PATH_MAX);
	
	txtPath = gwmCreateTextField(winMain, cwd, 0, 20, DEFAULT_WIDTH, 0);
	
	gwmSetWindowFlags(winMain, GWM_WINDOW_MKFOCUSED);
	gwmMainLoop();
	gwmQuit();
	return 0;
};
