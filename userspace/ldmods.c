/*
	Glidix Runtime

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
#include <string.h>
#include <stdlib.h>
#include <errno.h>
#include <dirent.h>

/**
 * ldmods
 * This program is ran during boot, when the system is in system state 1, and just before
 * entering system state 2. We are responsible for loading additional kernel modules from
 * /etc/modules (as opposed to the bare minimum "initmods" from the initrd).
 */
int main(int argc, char *argv[])
{
	DIR *dirp = opendir("/etc/modules");
	if (dirp == NULL)
	{
		fprintf(stderr, "%s: failed to open /etc/modules: %s\n", argv[0], strerror(errno));
		return 1;
	};
	
	struct dirent *ent;
	
	char path[512];
	char filename[512];
	char modname[512];
	while ((ent = readdir(dirp)) != NULL)
	{
		strcpy(filename, ent->d_name);
		if (strlen(filename) < 5)
		{
			fprintf(stderr, "%s: skipping misnamed file: %s\n", argv[0], filename);
			continue;
		};
		
		char *dot = strchr(filename, '.');
		if (dot != NULL)
		{
			*dot = 0;
		};
		
		sprintf(modname, "mod_%s", filename);
		sprintf(path, "/etc/modules/%s", ent->d_name);
		
		if (_glidix_insmod(modname, path, "", 0) != 0)
		{
			fprintf(stderr, "%s: insmod failed for %s (%s): %s\n", argv[0], path, modname, strerror(errno));
			continue;
		};
	};
	
	closedir(dirp);
};
