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
#include <sys/fsinfo.h>
#include <sys/call.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <errno.h>

char *idToString(uint8_t *id)
{
	char *result = (char*) malloc(33);
	size_t i;
	
	for (i=0; i<16; i++)
	{
		sprintf(&result[2*i], "%02hhX", id[i]);
	};
	
	return result;
};

int main(int argc, char *argv[])
{
	struct fsinfo list[256];
	size_t count = _glidix_fsinfo(list, 256);
	
	if (count == 0)
	{
		fprintf(stderr, "%s: cannot get filesystem information: %s\n", argv[0], strerror(errno));
		return 1;
	};
	
	if (argc == 1)
	{
		printf("%-4s%-30s%-30s%-10s\n", "ID", "IMAGE", "MOUNTPOINT", "TYPE");
	
		size_t i;
		for (i=0; i<count; i++)
		{
			printf("%-4lu%-30s%-30s%-10s\n", list[i].fs_dev, list[i].fs_image, list[i].fs_mntpoint, list[i].fs_name);
		};
	
		return 0;
	}
	else if (argc == 2)
	{
		if (strcmp(argv[1], "-d") == 0 || strcmp(argv[1], "--drivers") == 0)
		{
			char names[256*16];
			int drvcount = (int) __syscall(__SYS_fsdrv, names, 256);
			if (drvcount == -1)
			{
				fprintf(stderr, "%s: cannot get driver list: %s\n", argv[0], strerror(errno));
				return 1;
			};
			
			const char *scan = names;
			while (drvcount--)
			{
				printf("%s\n", scan);
				scan += 16;
			};
			
			return 0;
		};
		
		dev_t id;
		if (sscanf(argv[1], "%lu", &id) != 1)
		{
			fprintf(stderr, "%s: invalid filesystem ID: %s\n", argv[0], argv[1]);
			return 1;
		};
		
		size_t i;
		for (i=0; i<count; i++)
		{
			if (list[i].fs_dev == id)
			{
				break;
			};
		};
		
		if (i == count)
		{
			fprintf(stderr, "%s: filesystem with ID %lu doesn't exist\n", argv[0], id);
			return 1;
		};
		
		printf("ID %lu: %s on %s mounted at %s\n", id, list[i].fs_name, list[i].fs_image, list[i].fs_mntpoint);
		
		if (list[i].fs_inodes == 0)
		{
			printf("No inode usage information.\n");
		}
		else
		{
			printf("Inode usage: %lu/%lu (%lu%%)\n", list[i].fs_usedino, list[i].fs_inodes,
					list[i].fs_usedino * 100 / list[i].fs_inodes);
		};
		
		if (list[i].fs_blocks == 0)
		{
			printf("No block usage information.\n");
		}
		else
		{
			printf("Block usage: %lu/%lu (%lu%%)\n", list[i].fs_usedblk, list[i].fs_blocks,
				list[i].fs_usedblk * 100 / list[i].fs_blocks);
		};
		
		printf("Block size: %lu bytes\n", list[i].fs_blksize);
		printf("Boot ID: %s\n", idToString(list[i].fs_bootid));
		return 0;
	}
	else
	{
		fprintf(stderr, "USAGE:\t%s [id]\n", argv[0]);
		fprintf(stderr, "\tList all mounted filesystems (and their IDs) or display information\n");
		fprintf(stderr, "\tabout the filesystem with the given ID.\n");
		return 1;
	};
};
