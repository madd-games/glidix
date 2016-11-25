/*
	Glidix Installer

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

void pkgInstall()
{
	int numPkg = 0;
	DIR *dirp = opendir("/usr/share/pkg");
	if (dirp == NULL)
	{
		msgbox("ERROR", "Failed to open /usr/share/pkg!");
		return;
	};
	
	struct dirent *ent;
	while ((ent = readdir(dirp)) != NULL)
	{
		if (ent->d_name[0] != '.') numPkg++;
	};
	
	closedir(dirp);
	
	if (numPkg == 0) return;
	
	dirp = opendir("/usr/share/pkg");
	if (dirp == NULL)
	{
		msgbox("ERROR", "Failed to open /usr/share/pkg!");
		return;
	};
	
	int count = 0;
	while ((ent = readdir(dirp)) != NULL)
	{
		if (ent->d_name[0] != '.')
		{
			char msg[256];
			sprintf(msg, "Unpacking %s...", ent->d_name);
		
			char path[256];
			sprintf(path, "/usr/share/pkg/%s", ent->d_name);
		
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
				open("/dev/null", O_RDWR);
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
				continue;
			};
		
			if (WEXITSTATUS(status) != 0)
			{
				msgbox("ERROR", "mip-install terminated with an error!");
				continue;
			};
		};
	};
	
	closedir(dirp);
};
