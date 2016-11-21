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

#include <sys/glidix.h>
#include <stdio.h>
#include <stdlib.h>

const char *progName;
int flags = 0;

void processSwitches(const char *sw)
{
	const char *scan;
	for (scan=&sw[1]; *scan!=0; scan++)
	{
		switch (*scan)
		{
		case 'v':
			flags |= _GLIDIX_INSMOD_VERBOSE;
			break;
		default:
			fprintf(stderr, "%s: unrecognised command-line option: -%c\n", progName, *scan);
			break;
		};
	};
};

void usage()
{
	fprintf(stderr, "USAGE:\t%s [-v] modname path [options]\n", progName);
	fprintf(stderr, "\tLoad a module with the specified name from the file at the given path.\n");
	fprintf(stderr, "\tYou may pass an additional option string to the module.\n");
	fprintf(stderr, "\t-v\tBe more verbose.\n");
};

int main(int argc, char *argv[])
{
	progName = argv[0];

	const char *modname = NULL;
	const char *path = NULL;
	const char *options = NULL;

	int i;
	for (i=1; i<argc; i++)
	{
		if (argv[i][0] == '-')
		{
			processSwitches(argv[i]);
		}
		else if (modname == NULL)
		{
			modname = argv[i];
		}
		else if (path == NULL)
		{
			path = argv[i];
		}
		else if (options == NULL)
		{
			options = argv[i];
		}
		else
		{
			usage();
			return 1;
		};
	};

	if ((modname == NULL) || (path == NULL))
	{
		usage();
		return 1;
	};

	if (_glidix_insmod(modname, path, options, flags) != 0)
	{
		fprintf(stderr, "%s: loading failed\n", progName);
		return 1;
	};

	return 0;
};
