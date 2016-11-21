/*
	Glidix Shell

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

#include <unistd.h>
#include <stdio.h>
#include <string.h>
#include <stdlib.h>

void cd_usage()
{
	fprintf(stderr, "USAGE:\tcd [dirname]\n\n");
	fprintf(stderr, "\tChange the current working directory. If dirname is not\n");
	fprintf(stderr, "\tspecified, the HOME environment variable is used.\n");
};

int cmd_cd(int argc, char **argv)
{
	if ((argc != 1) && (argc != 2))
	{
		cd_usage();
		return 1;
	};

	const char *dirname;
	if (argc == 2)
	{
		if (argv[1][0] == '-')
		{
			cd_usage();
			return 1;
		};

		dirname = argv[1];
	}
	else
	{
		dirname = getenv("HOME");
		if (dirname == NULL)
		{
			dirname = "/initrd";
		};
	};

	if (chdir(dirname) != 0)
	{
		perror(dirname);
		return 1;
	};

	return 0;
};
