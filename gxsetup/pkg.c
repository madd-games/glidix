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
#include <libgpm.h>

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
			 50, 10,
			 &startX, &startY);
	
	size_t scrollPos = 0;
	while (1)
	{
		size_t barStart = scrollPos * 10 / numPackages;
		size_t barEnd = (scrollPos+10) * 10 / numPackages;
		
		size_t i;
		for (i=scrollPos; i<scrollPos+10; i++)
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
		
			size_t linePos = i - scrollPos;
			char scrollChar = 176;
			if ((linePos >= barStart) && (linePos <= barEnd))
			{
				scrollChar = 219;
			};
			
			setCursor((uint8_t)startX, (uint8_t)(startY+i-scrollPos));
			printf("%c[%c] %-44s", prefix, tick, packages[i].name);
			
			setColor(COLOR_SCROLLBAR);
			printf("%c", scrollChar);
		};

		setCursor(79, 24);
		
		uint8_t c;
		if (read(0, &c, 1) != 1) continue;
		
		if (c == 0x8B)
		{
			// up
			if (selected != 0) selected--;
			if (selected < scrollPos) scrollPos = selected;
		}
		else if (c == 0x8C)
		{
			// down
			if (selected != (numPackages-1)) selected++;
			if (selected >= (scrollPos+10)) scrollPos = selected-9;
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
	// create the package database
	mkdir("/mnt/etc/gpm", 0755);
	
	// create the repo list
	int fd = open("/mnt/etc/gpm/repos.conf", O_CREAT | O_WRONLY | O_EXCL, 0644);
	if (fd != -1)
	{
		FILE *fp = fdopen(fd, "w");
		fprintf(fp, "# /etc/gpm/repos.conf\n");
		fprintf(fp, "# Repository configuration file. List URLs of GPM repositories here.\n");
		fprintf(fp, "# Run `gpm reindex' after changing this list to update the indexes!\n");
		fprintf(fp, "# See repos(3) for more information.\n");
		fprintf(fp, "http://glidix.madd-games.org/experimental.gpm\n");
		fclose(fp);
	};
	
	GPMContext *ctx = gpmCreateContext("/mnt", NULL);
	if (ctx == NULL)
	{
		msgbox("ERROR", "Failed to open GPM database!");
		exit(1);
	};
	
	GPMInstallRequest *req = gpmCreateInstallRequest(ctx, NULL);
	if (req == NULL)
	{
		msgbox("ERROR", "Failed to create installation request!");
		exit(1);
	};
	
	int i;
	for (i=0; i<numPackages; i++)
	{
		if (packages[i].install)
		{
			char *pkgpath;
			asprintf(&pkgpath, "/usr/share/pkg/%s", packages[i].name);
			
			if (gpmInstallRequestMIP(req, pkgpath, NULL) != 0)
			{
				char *errmsg;
				asprintf(&errmsg, "Failed to add package %s", pkgpath);
				msgbox("ERROR", errmsg);
				exit(1);
			};
			
			free(pkgpath);
		};
	};
	
	GPMTaskProgress *task = gpmRunInstall(req);
	if (task == NULL)
	{
		msgbox("ERROR", "Failed to execute installation task!");
		exit(1);
	};
	
	while (!gpmIsComplete(task))
	{
		drawProgress("SETUP", "Installing packages...", gpmGetProgress(task) * 100, 100);
		sleep(1);
	};
	
	char *result = gpmWait(task);
	if (result != NULL)
	{
		msgbox("ERROR", result);
		exit(1);
	};
	
	gpmDestroyContext(ctx);
};
