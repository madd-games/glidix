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

#include <sys/stat.h>
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <string.h>
#include <errno.h>
#include <fcntl.h>
#include <inttypes.h>
#include <dirent.h>

const char *progName;

const char *optKernel = "/initrd/kernel.so";
const char *optOutput = "/boot/vmglidix.tar";
const char *optInit = "/initrd/init";
const char **optInitmods = NULL;
int optNumInitmods = 0;
int optDefaultInitmods = 1;

int outfile;

typedef struct
{
	char				filename[100];
	char				mode[8];
	char				uid[8];
	char				gid[8];
	char				size[12];
	char				mtime[12];
	char				checksum[8];
	char				type;
	char				pad[512-157];
} TARFileHeader;

int startsWith(const char *str, const char *prefix)
{
	if (strlen(str) < strlen(prefix)) return 0;
	return memcmp(str, prefix, strlen(prefix)) == 0;
};

void doChecksum(TARFileHeader *header)
{
	uint32_t sum = 0;
	uint8_t *scan = (uint8_t*) header;
	
	int i;
	for (i=0; i<511; i++)
	{
		sum += scan[i];
	};
	
	sprintf(header->checksum, "%06o", sum);
};

int appendFile(const char *name, const char *srcfile)
{
	struct stat st;
	if (stat(srcfile, &st) != 0)
	{
		fprintf(stderr, "%s: cannot stat %s: %s\n", progName, srcfile, strerror(errno));
		return -1;
	};
	
	TARFileHeader header;
	memset(&header, 0, 512);
	
	strcpy(header.filename, name);
	strcpy(header.mode, "0000755");
	strcpy(header.uid, "0000000");
	strcpy(header.gid, "0000000");
	sprintf(header.size, "%011lo", st.st_size);
	strcpy(header.mtime, "00000000000");
	memset(header.checksum, ' ', 8);
	header.type = '0';

	doChecksum(&header);
	
	write(outfile, &header, 512);
	
	int fd = open(srcfile, O_RDONLY);
	if (fd == -1)
	{
		fprintf(stderr, "%s: failed to open %s: %s\n", progName, srcfile, strerror(errno));
		return -1;
	};
	
	char buf[512];
	while (1)
	{
		memset(buf, 0, 512);
		ssize_t sz = read(fd, buf, 512);
		
		if (sz != 0)
		{
			write(outfile, buf, 512);
		};
		
		if (sz != 512)
		{
			break;
		};
	};
	
	close(fd);
	return 0;
};

int appendString(const char *name, const char *data)
{
	TARFileHeader header;
	memset(&header, 0, 512);
	
	strcpy(header.filename, name);
	strcpy(header.mode, "0000755");
	strcpy(header.uid, "0000000");
	strcpy(header.gid, "0000000");
	sprintf(header.size, "%011lo", strlen(data));
	strcpy(header.mtime, "00000000000");
	memset(header.checksum, ' ', 8);
	header.type = '0';

	doChecksum(&header);
	
	write(outfile, &header, 512);
	
	char pad[512];
	memset(pad, 0, 512);
	
	write(outfile, data, strlen(data));
	
	size_t bytesToPad = (512 - strlen(data)) & 511;
	write(outfile, pad, bytesToPad);
	
	return 0;
};

void appendDir(const char *dirname)
{
	TARFileHeader header;
	memset(&header, 0, 512);
	
	strcpy(header.filename, dirname);
	strcpy(header.mode, "0000755");
	strcpy(header.uid, "0000000");
	strcpy(header.gid, "0000000");
	strcpy(header.size, "00000000000");
	strcpy(header.mtime, "00000000000");
	memset(header.checksum, ' ', 8);
	header.type = '0';

	doChecksum(&header);
	
	write(outfile, &header, 512);
};

int main(int argc, char *argv[])
{
	progName = argv[0];
	
	int i;
	for (i=1; i<argc; i++)
	{
		if (argv[i][0] != '-')
		{
			optInitmods = (const char **) realloc(optInitmods, (optNumInitmods+1)*sizeof(void*));
			optInitmods[optNumInitmods++] = argv[i];
		}
		else if (strcmp(argv[i], "--no-default-initmod") == 0)
		{
			optDefaultInitmods = 0;
		}
		else if (startsWith(argv[i], "--kernel="))
		{
			optKernel = &argv[i][9];
		}
		else if (startsWith(argv[i], "--output="))
		{
			optOutput = &argv[i][9];
		}
		else if (startsWith(argv[i], "--init="))
		{
			optInit = &argv[i][7];
		}
		else
		{
			fprintf(stderr, "%s: unrecognised command-line option: %s\n", argv[0], argv[i]);
			return 1;
		};
	};
	
	outfile = open(optOutput, O_WRONLY | O_TRUNC | O_CREAT, 0644);
	if (outfile == -1)
	{
		fprintf(stderr, "%s: cannot open %s for writing: %s\n", argv[0], optOutput, strerror(errno));
		return 1;
	};
	
	if (appendFile("kernel.so", optKernel) != 0)
	{
		return 1;
	};
	
	if (appendFile("init", optInit) != 0)
	{
		return 1;
	};
	
	appendDir("initmod/");
	
	if (optDefaultInitmods)
	{
		DIR *dirp = opendir("/initrd/initmod");
		if (dirp == NULL)
		{
			fprintf(stderr, "%s: cannot read directory /initrd/initmod: %s\n", argv[0], strerror(errno));
			return 1;
		};
		
		struct dirent *ent;
		while ((ent = readdir(dirp)) != NULL)
		{
			if (ent->d_name[0] != '.')
			{
				char modpath[256];
				sprintf(modpath, "/initrd/initmod/%s", ent->d_name);
				
				char initmodname[256];
				sprintf(initmodname, "initmod/%s", ent->d_name);
				
				// first make sure we're not replacing this module
				for (i=0; i<optNumInitmods; i++)
				{
					const char *modname = optInitmods[i];
					if (strrchr(modname, '/') != NULL)
					{
						modname = strrchr(modname, '/') + 1;
					};
					
					if (strcmp(modname, ent->d_name) == 0)
					{
						break;
					};
				};
				
				if (i == optNumInitmods)
				{
					// we can include this module
					if (appendFile(initmodname, modpath) != 0)
					{
						return 1;
					};
				};
			};
		};
		
		closedir(dirp);
	};
	
	// put in other initmods
	for (i=0; i<optNumInitmods; i++)
	{
		const char *path = optInitmods[i];
		const char *lastSlash = strrchr(path, '/');
		
		char initmodname[256];
		if (lastSlash == NULL)
		{
			sprintf(initmodname, "initmod/%s", path);
		}
		else
		{
			sprintf(initmodname, "initmod/%s", lastSlash+1);
		};
		
		if (appendFile(initmodname, path) != 0)
		{
			return 1;
		};
	};
	
	close(outfile);
	return 0;
};
