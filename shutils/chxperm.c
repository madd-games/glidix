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

#define _GLIDIX_SOURCE
#include <sys/stat.h>
#include <sys/xperm.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <errno.h>

char *progName;

typedef struct
{
	const char *name;
	xperm_t mask;
} PermName;

PermName perms[] = {
	{"all",					XP_ALL},
	{"rawsock",				XP_RAWSOCK},
	{"netconf",				XP_NETCONF},
	{"module",				XP_MODULE},
	{"mount",				XP_MOUNT},
	{"chxperm",				XP_CHXPERM},
	{"nice",				XP_NICE},
	{"dispconf",				XP_DISPCONF},
	{NULL,					0}
};

void usage()
{
	fprintf(stderr, "USAGE:\t%s <path> [set.name=value]...\n", progName);
	fprintf(stderr, "\tSet executable permissions of a file.\n");
};

xperm_t getPermMask(const char *name)
{
	PermName *pn;
	for (pn=perms; pn->name!=NULL; pn++)
	{
		if (strcmp(pn->name, name) == 0) break;
	};
	
	return pn->mask;
};

int main(int argc, char *argv[])
{
	progName = argv[0];
	
	if (argc < 2)
	{
		usage();
		return 1;
	};
	
	if (_glidix_dxperm() != XP_ALL)
	{
		fprintf(stderr, "%s: i do not have all delegatable permissions!\n", argv[0]);
		return 1;
	};
	
	if (!_glidix_haveperm(XP_CHXPERM))
	{
		fprintf(stderr, "%s: i was not granted the 'chxperm' permission!\n", argv[0]);
		return 1;
	};
	
	xperm_t ixperm, oxperm, dxperm;
	struct stat st;
	if (stat(argv[1], &st) != 0)
	{
		fprintf(stderr, "%s: stat on %s failed: %s\n", argv[0], argv[1], strerror(errno));
		return 1;
	};
	
	ixperm = st.st_ixperm;
	oxperm = st.st_oxperm;
	dxperm = st.st_dxperm;
	
	int i;
	for (i=2; i<argc; i++)
	{
		char *spec = argv[i];
		char *dot = strchr(spec, '.');
		char *equal = strchr(spec, '=');
		
		if ((dot == NULL) || (equal == NULL))
		{
			fprintf(stderr, "%s: invalid permission specification: %s\n", argv[0], spec);
			return 1;
		};
		
		*dot = 0;
		*equal = 0;
		
		const char *setname = spec;
		const char *permname = dot+1;
		const char *value = equal+1;
		
		xperm_t mask = getPermMask(permname);
		if (mask == 0)
		{
			fprintf(stderr, "%s: invalid permission name: %s\n", argv[0], permname);
			return 1;
		};
		
		xperm_t *ptr;
		if (strcmp(setname, "inherit") == 0)
		{
			ptr = &ixperm;
		}
		else if (strcmp(setname, "own") == 0)
		{
			ptr = &oxperm;
		}
		else if (strcmp(setname, "delegate") == 0)
		{
			ptr = &dxperm;
		}
		else
		{
			fprintf(stderr, "%s: invalid permission set: %s\n", argv[0], setname);
			return 1;
		};
		
		if (strcmp(value, "0") == 0)
		{
			(*ptr) &= ~mask;
		}
		else if (strcmp(value, "1") == 0)
		{
			(*ptr) |= mask;
		}
		else
		{
			fprintf(stderr, "%s: invalid permission value '%s' (should be '0' or '1')\n", argv[0], value);
			return 1;
		};
	};
	
	if (_glidix_chxperm(argv[1], ixperm, oxperm, dxperm) != 0)
	{
		fprintf(stderr, "%s: cannot change permissions on %s: %s\n", argv[0], argv[1], strerror(errno));
		return 1;
	};
	
	return 0;
};
