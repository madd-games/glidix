/*
	Glidix Installer

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

#include <sys/stat.h>
#include <sys/wait.h>
#include <unistd.h>
#include <dirent.h>
#include <string.h>
#include <stdio.h>
#include <stdlib.h>
#include <fcntl.h>

#include "progress.h"
#include "pkg.h"
#include "msgbox.h"
#include "render.h"
#include "error.h"

typedef struct
{
	const char*			name;
	int				install;
} Package;

static Package *packages = NULL;
static size_t numPackages = 0;

void pkgSelection()
{
	// make a list of all the packages
	DIR *dirp = opendir("/usr/share/pkg");
	if (dirp == NULL)
	{
		msgbox("ERROR", "Failed to open /usr/share/pkg!");
		return;
	};
	
	struct dirent *ent;
	while ((ent = readdir(dirp)) != NULL)
	{
		if (ent->d_name[0] != '.')
		{
			size_t index = numPackages++;
			packages = (Package*) realloc(packages, sizeof(Package) * numPackages);
			packages[index].name = strdup(ent->d_name);
			packages[index].install = 1;
		};
	};
	
	closedir(dirp);
	
	int startX, startY;
	size_t selected = 0;
	
	renderWindow("\30" "\31" " Select package\t<ENTER> Toggle installation\t" "\x1A" " Proceed",
			"SELECT PACKAGES",
			 50, (int) numPackages,
			 &startX, &startY);
	
	while (1)
	{
		size_t i;
		for (i=0; i<numPackages; i++)
		{
			char prefix, tick;
			if (i == selected)
			{
				setColor(COLOR_SELECTION);
				prefix = 16;
			}
			else
			{
				setColor(COLOR_WINDOW);
				prefix = ' ';
			};
		
			if (packages[i].install)
			{
				tick = '*';
			}
			else
			{
				tick = ' ';
			};
		
			setCursor((uint8_t)startX, (uint8_t)(startY+i));
			printf("%c[%c] %-45s", prefix, tick, packages[i].name);
		};

		setCursor(79, 24);
		
		uint8_t c;
		if (read(0, &c, 1) != 1) continue;
		
		if (c == 0x8B)
		{
			// up
			if (selected != 0) selected--;
		}
		else if (c == 0x8C)
		{
			// down
			if (selected != (numPackages-1)) selected++;
		}
		else if (c == '\n')
		{
			// toggle
			packages[selected].install = !packages[selected].install;
		}
		else if (c == 0x8E)
		{
			// right
			break;
		};
	};
};

void pkgInstall()
{
	int numPkg = 0;
	size_t i;
	for (i=0; i<numPackages; i++)
	{
		if (packages[i].install) numPkg++;
	};
	
	if (numPkg == 0) return;
	
	int count = 0;
	for (i=0; i<numPackages; i++)
	{
		if (packages[i].install)
		{
			char msg[256];
			sprintf(msg, "Unpacking %s...", packages[i].name);
		
			char path[256];
			sprintf(path, "/usr/share/pkg/%s", packages[i].name);
		
			drawProgress("SETUP", msg, count++, numPkg);
		
			pid_t pid = fork();
			if (pid == -1)
			{
				msgbox("ERROR", "fork() failed for mip-install!");
				continue;
			};
		
			if (pid == 0)
			{
				close(0);
				close(1);
				close(2);
				open("/run/mip-log", O_RDWR | O_CREAT | O_TRUNC);
				dup(0);
				dup(1);
				execl("/usr/bin/mip-inst", "mip-install", path, "--dest=/mnt", NULL);
				exit(1);
			};
		
			int status;
			waitpid(pid, &status, 0);
		
			if (!WIFEXITED(status))
			{
				msgbox("ERROR", "mip-install terminated with an error!");
				displayError("/run/mip-log");
			};
		
			if (WEXITSTATUS(status) != 0)
			{
				msgbox("ERROR", "mip-install terminated with an error!");
				displayError("/run/mip-log");
			};
		};
	};
};
