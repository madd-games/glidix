/*
	Glidix Filesystem Tools

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
#include <unistd.h>
#include <fstools.h>
#include <stdio.h>
#include <dirent.h>
#include <errno.h>
#include <string.h>
#include <fcntl.h>

static FSMimeType *firstType = NULL;
static FSMimeType *lastType = NULL;

/**
 * List of MIME categories.
 */
static const char *mimeCats[] = {
	"application",
	"audio",
	"font",
	"image",
	"message",
	"model",
	"multipart",
	"text",
	"video",
	NULL			/* end of list marker */
};

static FSMimeType *textPlain;
static FSMimeType *octetStream;

static FSMimeType *inodeDir;
static FSMimeType *inodeBlockdev;
static FSMimeType *inodeChardev;
static FSMimeType *inodeFifo;
static FSMimeType *inodeSocket;

static FSMimeType *specialType(const char *name, const char *label, const char *icon)
{
	FSMimeType *type = (FSMimeType*) malloc(sizeof(FSMimeType));
	type->next = NULL;
	strcpy(type->mimename, name);
	type->filenames = NULL;
	type->numFilenames = 0;
	type->magics = NULL;
	type->numMagics = 0;
	type->label = strdup(label);
	type->iconName = strdup(icon);
	
	return type;
};

static void loadMimeInfo(const char *cat, const char *type)
{
	char path[PATH_MAX];
	snprintf(path, PATH_MAX, "/usr/share/mime/%s/%s", cat, type);
	
	FSMimeType *info = (FSMimeType*) malloc(sizeof(FSMimeType));
	info->next = NULL;
	sprintf(info->mimename, "%s/%s", cat, type);
	
	info->filenames = NULL;
	info->numFilenames = 0;
	
	info->magics = NULL;
	info->numMagics = 0;
	
	info->label = info->mimename;
	info->iconName = strdup("unknown");
	
	if (lastType == NULL)
	{
		firstType = lastType = info;
	}
	else
	{
		lastType->next = info;
		lastType = info;
	};
	
	FILE *fp = fopen(path, "r");
	if (fp == NULL)
	{
		fprintf(stderr, "libfstools: cannot read %s: %s\n", path, strerror(errno));
		free(info);
	};
	
	char linebuf[1024];
	char *line;
	
	while ((line = fgets(linebuf, 1024, fp)) != NULL)
	{
		char *endline = strchr(line, '\n');
		if (endline != NULL) *endline = 0;
		
		if (line[0] == '#')
		{
			continue;
		}
		else if (strlen(line) == 0)
		{
			continue;
		}
		else
		{
			char *saveptr;
			size_t linesz = strlen(line);
			char *cmd = strtok_r(line, " \t", &saveptr);
			
			if (cmd == NULL)
			{
				continue;
			}
			else if (strcmp(cmd, "filename") == 0)
			{
				char *pat;
				for (pat=strtok_r(NULL, " \t", &saveptr); pat!=NULL; pat=strtok_r(NULL, " \t", &saveptr))
				{
					size_t index = info->numFilenames++;
					info->filenames = realloc(info->filenames, sizeof(char*)*info->numFilenames);
					info->filenames[index] = strdup(pat);
				};
			}
			else if (strcmp(cmd, "label") == 0)
			{
				char *newLabel = NULL;
				if (linesz > 5)
				{
					newLabel = &line[6];
				};
				
				if (newLabel == NULL)
				{
					fprintf(stderr, "fstools: 'label' without parameter\n");
				}
				else
				{
					if (info->label != info->mimename) free(info->label);
					info->label = strdup(newLabel);
				};
			}
			else if (strcmp(cmd, "icon") == 0)
			{
				char *newIcon = NULL;
				if (linesz > 4)
				{
					newIcon = &line[5];
				};
				
				if (newIcon == NULL)
				{
					fprintf(stderr, "fstools: 'icon' without parameter\n");
				}
				else
				{
					free(info->iconName);
					info->iconName = strdup(newIcon);
				};
			}
			else
			{
				fprintf(stderr, "fstools: invalid command in %s: %s\n", path, cmd);
			};
		};
	};
	
	fclose(fp);
};

static void loadCategory(const char *cat)
{
	char dirname[PATH_MAX];
	snprintf(dirname, PATH_MAX, "/usr/share/mime/%s", cat);
	
	DIR *dirp = opendir(dirname);
	if (dirp == NULL)
	{
		fprintf(stderr, "libfstools: cannot scan %s: ignoring category\n", dirname);
		return;
	};
	
	struct dirent *ent;
	while ((ent = readdir(dirp)) != NULL)
	{
		if (ent->d_name[0] != '.')
		{
			loadMimeInfo(cat, ent->d_name);
		};
	};
	
	closedir(dirp);
};

void fsInit()
{
	// some special types
	textPlain = specialType("text/plain", "Text file", "textfile");
	octetStream = specialType("application/octet-stream", "Binary file", "binfile");
	inodeDir = specialType("inode/directory", "Directory", "dir");
	inodeBlockdev = specialType("inode/blockdevice", "Block device", "device");
	inodeChardev = specialType("inode/chardevice", "Character device", "device");
	inodeFifo = specialType("inode/fifo", "Named pipe", "device");
	inodeSocket = specialType("inode/socket", "Socket", "device");
	
	// now load the MIME database (/usr/share/mime).
	const char **scan;
	for (scan=mimeCats; *scan!=NULL; scan++)
	{
		loadCategory(*scan);
	};
};

void fsQuit()
{
	FSMimeType *type = firstType;
	while (type != NULL)
	{
		size_t i;
		for (i=0; i<type->numFilenames; i++)
		{
			free(type->filenames[i]);
		};
		
		free(type->filenames);
		free(type->magics);
		
		if (type->label != type->mimename) free(type->label);
		
		FSMimeType *next = type->next;
		free(type);
		type = next;
	};
	
	firstType = lastType = NULL;
};

static int isTextFile(const char *path, struct stat *st)
{
	if (st->st_size > 6*1024*1024)
	{
		// text files larger than 6MB are improbable
		return 0;
	};
	
	int fd = open(path, O_RDONLY);
	if (fd == -1) return 0;
	
	char buffer[8*1024];
	
	while (1)
	{
		ssize_t sizeNow = read(fd, buffer, 8*1024);
		if (sizeNow == -1)
		{
			close(fd);
			return 0;
		};
		
		if (sizeNow == 0) break;
		
		ssize_t i;
		for (i=0; i<sizeNow; i++)
		{
			if (buffer[i] == 0)
			{
				close(fd);
				return 0;
			};
		};
	};
	
	close(fd);
	return 1;
};

static int isMatch(const char *str, const char *pattern)
{
	size_t len = strlen(str);
	
	while (*pattern != 0)
	{
		if (*pattern == '*')
		{
			const char *suffix = pattern + 1;
			const char *ending = str + len - strlen(suffix);
			return strcmp(suffix, ending) == 0;
		}
		else
		{
			if (*pattern++ != *str++) return 0;
		};
	};
	
	return (*str == 0);
};

FSMimeType* fsGetType(const char *filename)
{
	const char *basename = filename;
	const char *slashPos = strrchr(filename, '/');
	if (slashPos != NULL)
	{
		basename = slashPos + 1;
	};
	
	struct stat st;
	if (stat(filename, &st) != 0)
	{
		return octetStream;
	};
	
	if (S_ISBLK(st.st_mode))
	{
		return inodeBlockdev;
	}
	else if (S_ISCHR(st.st_mode))
	{
		return inodeChardev;
	}
	else if (S_ISDIR(st.st_mode))
	{
		return inodeDir;
	}
	else if (S_ISFIFO(st.st_mode))
	{
		return inodeFifo;
	}
	else if (S_ISSOCK(st.st_mode))
	{
		return inodeSocket;
	}
	else
	{
		// check filename patterns
		FSMimeType *type;
		for (type=firstType; type!=NULL; type=type->next)
		{
			size_t i;
			for (i=0; i<type->numFilenames; i++)
			{
				if (isMatch(basename, type->filenames[i]))
				{
					return type;
				};
			};
		};
		
		// TODO: magic values searches
		
		// matches nothing, check if it's plain text or binary
		if (isTextFile(filename, &st))
		{
			return textPlain;
		}
		else
		{
			return octetStream;
		};
	};
};
