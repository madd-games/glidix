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

int main(int argc, char *argv[])
{
	if (argc != 2)
	{
		fprintf(stderr, "USAGE:\t%s username\n", argv[0]);
		fprintf(stderr, "\t%s uid\n", argv[0]);
		fprintf(stderr, "\tFind a user by UID or username\n");
		return 1;
	};

	struct passwd *pwd;
	uid_t uid;
	
	if (sscanf(argv[1], "%lu", &uid) != 1)
	{
		pwd = getpwnam(argv[1]);
	}
	else
	{
		pwd = getpwuid(uid);
	};

	if (pwd == NULL)
	{
		fprintf(stderr, "%s: username/uid %s (%d) not in database\n", argv[0], argv[1], (int)uid);
		return 1;
	};

	printf("Username:\t%s\n", pwd->pw_name);
	printf("UID:\t\t%d\n", (int) pwd->pw_uid);
	printf("GID:\t\t%d\n", (int) pwd->pw_gid);
	printf("Full name:\t%s\n", pwd->pw_gecos);
	printf("Home:\t\t%s\n", pwd->pw_dir);
	printf("Shell:\t\t%s\n", pwd->pw_shell);

	return 0;
};
