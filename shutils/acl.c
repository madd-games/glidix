/*
	Glidix Shell Utilities

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

#define	_GLIDIX_SOURCE
#include <sys/stat.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <pwd.h>
#include <grp.h>
#include <errno.h>
#include <fcntl.h>
#include <unistd.h>

int main(int argc, char *argv[])
{
	if (argc != 5)
	{
		fprintf(stderr, "USAGE:\t%s <filename> <entry-type> <entry-name> <permissions>\n", argv[0]);
		fprintf(stderr, "\tChanges the access control list of a file named by <filename>.\n");
		fprintf(stderr, "\t<entry-type> is 'user' (in which case <entry-name> is the user name),\n");
		fprintf(stderr, "\tor 'group' (in which case <entry-name> is the group name).\n");
		fprintf(stderr, "\t<permissions> is either 'clear' (to remove the entry), or it is a set\n");
		fprintf(stderr, "\tof letters, each indicating a permissions: r (read), w (write),\n");
		fprintf(stderr, "\tx (execute), or it may be `-' to indicate that all permissions\n");
		fprintf(stderr, "\tare to be removed.\n");
		return 1;
	};
	
	const char *filename = argv[1];
	
	int type;
	if (strcmp(argv[2], "user") == 0)
	{
		type = ACE_USER;
	}
	else if (strcmp(argv[2], "group") == 0)
	{
		type = ACE_GROUP;
	}
	else
	{
		fprintf(stderr, "%s :invalid entry type `%s' (expected `user' or `group')\n", argv[0], argv[2]);
		return 1;
	};
	
	const char *idname = argv[3];
	int id;
	
	if (type == ACE_USER)
	{
		struct passwd *pwd = getpwnam(idname);
		if (pwd == NULL)
		{
			fprintf(stderr, "%s: no user named `%s'!\n", argv[0], idname);
			return 1;
		};
		
		id = (int) pwd->pw_uid;
	}
	else if (type == ACE_GROUP)
	{
		struct group *grp = getgrnam(idname);
		if (grp == NULL)
		{
			fprintf(stderr, "%s: no group named `%s'!\n", argv[0], idname);
			return 1;
		};
		
		id = (int) grp->gr_gid;
	};
	
	if (strcmp(argv[4], "clear") == 0)
	{
		if (aclclear(filename, type, id) != 0)
		{
			fprintf(stderr, "%s: cannot clear entry: %s\n", argv[0], strerror(errno));
			return 1;
		};
		
		return 0;
	}
	else
	{
		int perms;
		const char *scan;
		for (scan=argv[4]; *scan!=0; scan++)
		{
			switch (*scan)
			{
			case 'r':
				perms |= ACE_READ;
				break;
			case 'w':
				perms |= ACE_WRITE;
				break;
			case 'x':
				perms |= ACE_EXEC;
				break;
			case '-':
				perms = 0;
				break;
			default:
				fprintf(stderr, "%s: unknown permission letter: `%c'\n", argv[0], *scan);
				break;
			};
		};
		
		if (aclput(filename, type, id, perms) != 0)
		{
			fprintf(stderr, "%s: cannot add/update entry: %s\n", argv[0], strerror(errno));
			return 1;
		};
		
		return 0;
	};
};
