/*
	Glidix Shell Utilities

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

#include <sys/types.h>
#include <sys/stat.h>
#include <sys/procstat.h>
#include <unistd.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <dirent.h>
#include <errno.h>
#include <pwd.h>
#include <grp.h>

int showAll = 0;

void getUserName(uid_t uid, char *buffer)
{
#if 0
	struct passwd *pwd = getpwuid(uid);
	if (pwd == NULL)
	{
		strcpy(buffer, "?");
	}
	else
	{
		snprintf(buffer, 256, "%s", pwd->pw_name);
	};
#endif

	strcpy(buffer, "TEST");
};

void getGroupName(gid_t gid, char *buffer)
{
#if 0
	struct group *grp = getgrgid(gid);
	if (grp == NULL)
	{
		strcpy(buffer, "?");
	}
	else
	{
		snprintf(buffer, 256, "%s", grp->gr_name);
	};
#endif

	strcpy(buffer, "TEST");
};

int main(int argc, char *argv[])
{
	int i;
	for (i=1; i<argc; i++)
	{
		if (argv[i][0] != '-')
		{
			fprintf(stderr, "%s: unrecognised command-line option: %s\n", argv[0], argv[i]);
			return 1;
		}
		else
		{
			const char *sw = &argv[i][1];
			for (; *sw!=0; sw++)
			{
				switch (*sw)
				{
				case 'a':
					showAll = 1;
					break;
				default:
					fprintf(stderr, "%s: unrecognised command-line option: -%c\n", argv[0], *sw);
					return 1;
				};
			};
		};
	};
	
	DIR *dirp = opendir("/proc");
	if (dirp == NULL)
	{
		fprintf(stderr, "%s: cannot open /proc: %s\n", argv[0], strerror(errno));
		return 1;
	};
	
	// column headers
	printf("%-12s%-16s%-16s%-10s%-10s%s\n", "PID", "User", "Group", "Ticks", "Entries", "Executable");
	
	struct dirent *ent;
	while ((ent = readdir(dirp)) != NULL)
	{
		pid_t pid;
		
		// only bother looking at that numbered directories (not ".", "..", "self", etc)
		if (sscanf(ent->d_name, "%d", &pid) == 1)
		{
			char path[PATH_MAX];
			sprintf(path, "/proc/%d", pid);
			
			struct stat st;
			if (stat(path, &st) == 0)
			{
				if ((st.st_uid == getuid()) || showAll)
				{
					char exepath[PATH_MAX];
					sprintf(exepath, "/proc/%d/exe", pid);
					
					char username[256];
					char grpname[256];
					
					ssize_t sz = readlink(exepath, exepath, PATH_MAX);
					if (sz == -1)
					{
						continue;
					};
					
					getUserName(st.st_uid, username);
					getGroupName(st.st_gid, grpname);
					
					struct procstat st;
					if (_glidix_procstat(pid, &st, sizeof(struct procstat)) == -1)
					{
						continue;
					};
					
					exepath[sz] = 0;
					printf("%-12d%-16s%-16s%-10lu%-10lu%s\n", pid, username, grpname, st.ps_ticks, st.ps_entries, exepath);
				};
			};
		};
	};
	
	closedir(dirp);
	return 0;
};
