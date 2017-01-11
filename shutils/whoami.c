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

#include <pwd.h>
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <grp.h>

void printUser(const char *idname, uid_t uid)
{
	struct passwd *pwd = getpwuid(uid);
	const char *name = "error";
	if (pwd != NULL) name = pwd->pw_name;
	printf("%s:\t%d <%s>\n", idname, (int)uid, name);
};

int main()
{
	printUser("euid", geteuid());
	printUser("uid", getuid());
	
	int count = getgroups(0, NULL);
	gid_t *groups = (gid_t*) malloc(sizeof(gid_t)*count);
	getgroups(count, groups);
	
	printf("groups: ");
	
	int i;
	for (i=0; i<count; i++)
	{
		gid_t gid = groups[i];
		const char *name = "?";
		
		struct group *grp = getgrgid(gid);
		if (grp != NULL)
		{
			name = grp->gr_name;
		};
		
		printf("%u <%s>", (unsigned int) gid, name);
		if (i != (count-1)) printf(", ");
	};
	printf("\n");
	
	return 0;
};
