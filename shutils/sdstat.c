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
#include <stdio.h>
#include <signal.h>
#include <stdlib.h>
#include <unistd.h>
#include <sys/glidix.h>
#include <string.h>
#include <fcntl.h>
#include <sys/wait.h>
#include <sys/ioctl.h>
#include <sys/storage.h>
#include <sys/stat.h>
#include <dirent.h>
#include <time.h>
#include <errno.h>

int main(int argc, char *argv[])
{
	if (argc != 2)
	{
		fprintf(stderr, "USAGE:\t%s block-device\n", argv[0]);
		fprintf(stderr, "\tDisplay information about a block device.\n");
		return 1;
	};
	
	SDIdentity id;
	if (pathctl(argv[1], IOCTL_SDI_IDENTITY, &id) != 0)
	{
		fprintf(stderr, "%s: cannot sdstat %s: %s\n", argv[0], argv[1], strerror(errno));
		return 1;
	};
	
	char flagspec[256];
	flagspec[0] = 0;
	
	if (id.flags & SD_READONLY)
	{
		strcat(flagspec, "read-only ");
	}
	else
	{
		strcat(flagspec, "read-write ");
	};
	
	if (id.flags & SD_EJECTABLE)
	{
		strcat(flagspec, "ejectable ");
	}
	else
	{
		strcat(flagspec, "hard-disk ");
	};
	
	if (id.flags & SD_REMOVEABLE)
	{
		strcat(flagspec, "removeable ");
	}
	else
	{
		strcat(flagspec, "built-in ");
	};
	
	printf("Flags:                     %s\n", flagspec);
	printf("Partition index:           %d\n", id.partIndex);
	printf("Offset on disk:            0x%016lX\n", id.offset);
	printf("Size:                      0x%016lX\n", id.size);
	printf("Name:                      %s\n", id.name);
	return 0;
};
