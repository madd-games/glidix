/*
	Glidix kernel

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

#include <glidix/initrdfs.h>
#include <glidix/string.h>
#include <glidix/memory.h>
#include <glidix/console.h>
#include <glidix/semaphore.h>
#include <glidix/sched.h>
#include <glidix/ftree.h>

static FileSystem *initrdfs;

typedef struct
{
	char			filename[100];
	char			mode[8];
	char			uid[8];
	char			gid[8];
	char			size[12];
	char			mtime[12];
	char			checksum[8];
	char			type;
	char			link_name[100];
	char			ustar[5];
	char			ustart_garb[3];
	char			owner_uname[32];
	char			owner_gname[32];
	FileTree*		ft;			/* dev_major normally here; we don't need it */
	char			dev_minor[8];
	char			prefix[155];
	char			pad[12];
} PACKED TarHeader;

TarHeader *masterHeader;
static uint64_t initrdEnd;
SECTION(".initrd") uint8_t initrdImage[8*1024*1024];

typedef struct
{
	// what the file names need to start with
	char			prefix[100];

	// number of slashes (excluding the final slash for directories) that should be
	// in the name.
	int			dirDepth;

	// the current TarHeader
	TarHeader		*header;
} TarState;

typedef struct
{
	// points to the start of this file.
	char*			data;

	// the size of this file (in bytes).
	size_t			size;

	// current position in the file.
	off_t			pos;
	
	// the header
	TarHeader*		header;
} TarFile;

static void strcpyUntilSlash(char *dst, const char *src)
{
	while ((*src != 0) && (*src != '/'))
	{
		*dst++ = *src++;
	};
	*dst = 0;
};

static uint64_t parseOct(const char *data)
{
	uint64_t out = 0;
	while (*data != 0)
	{
		out = out * 8 + ((*data++)-'0');
	};
	return out;
};

static int opendir(Dir *me, Dir *dir, size_t szdir)
{
	(void)szdir;
	TarState *meState = (TarState*) me->fsdata;
	TarState *state = (TarState*) kmalloc(sizeof(TarState));
	dir->fsdata = state;

	strcpy(state->prefix, meState->prefix);
	strcat(state->prefix, me->dirent.d_name);
	strcat(state->prefix, "/");
	state->dirDepth = meState->dirDepth + 1;
	state->header = meState->header;

	dir->openfile = me->openfile;
	dir->opendir = me->opendir;
	dir->close = me->close;
	dir->next = me->next;
	dir->stat.st_blocks = 0;

	// if the directory is empty, this will return -1 as no suitable header will be found,
	// but if there are files, this will find the first file and fill in dirent and stat!
	return dir->next(dir);
};

static int dirNext(Dir *dir)
{
	TarState *state = (TarState*) dir->fsdata;

	while (1)
	{
		state->header = &state->header[1+dir->stat.st_blocks];
		if ((uint64_t)(state->header) >= initrdEnd)
		{
			return -1;
		};

		if (state->header->filename[0] == 0)
		{
			return -1;
		};

		dir->dirent.d_ino = (ino_t) state->header;
		strcpyUntilSlash(dir->dirent.d_name, &state->header->filename[strlen(state->prefix)]);
		dir->stat.st_ino = dir->dirent.d_ino;
		dir->stat.st_mode = 0555;
		if (state->header->filename[strlen(state->header->filename)-1] == '/') dir->stat.st_mode |= VFS_MODE_DIRECTORY;
		dir->stat.st_size = parseOct(state->header->size);
		dir->stat.st_blksize = 512;
		dir->stat.st_blocks = dir->stat.st_size / 512;
		if (dir->stat.st_size % 512) dir->stat.st_blocks++;
		dir->stat.st_ixperm = XP_ALL;
		dir->stat.st_oxperm = 0;
		dir->stat.st_dxperm = XP_ALL;

		size_t numSlashes = 0;
		const char *scan = state->header->filename;
		while (*scan != 0)
		{
			if ((*scan++) == '/') numSlashes++;
		};

		// last slash (for directories) does not count.
		if (state->header->filename[strlen(state->header->filename)-1] == '/') numSlashes--;

		if (numSlashes != state->dirDepth)
		{
			continue;
		};
		if (strlen(state->header->filename) < strlen(state->prefix))
		{
			continue;
		};
		if (memcmp(state->header->filename, state->prefix, strlen(state->prefix)) != 0)
		{
			continue;
		};

		break;
	};

	return 0;
};

static void dirClose(Dir *dir)
{
	kfree(dir->fsdata);
};

static void fileClose(File *file)
{
	kfree(file->fsdata);
};

static off_t seek(File *file, off_t offset, int whence)
{
	TarFile *tf = (TarFile*) file->fsdata;

	off_t newPos = offset;
	switch (whence)
	{
	case SEEK_CUR:
		newPos += tf->pos;
		break;
	case SEEK_END:
		newPos += (off_t) tf->size;
		break;
	};

	if (newPos < 0)
	{
		newPos = 0;
	};

	if (newPos > (off_t)tf->size)
	{
		newPos = (off_t) tf->size;
	};

	tf->pos = newPos;
	return newPos;
};

static ssize_t read(File *file, void *buffer, size_t size)
{
	TarFile *tf = (TarFile*) file->fsdata;
	if (((size_t)tf->pos + size) > tf->size)
	{
		size = tf->size - (size_t) tf->pos;
	};

	memcpy(buffer, &tf->data[tf->pos], size);
	tf->pos += size;

	return size;
};

static ssize_t pread(File *file, void *buffer, size_t size, off_t off)
{
	TarFile *tf = (TarFile*) file->fsdata;
	
	if ((size_t)off > tf->size)
	{
		return 0;
	};
	
	if (((size_t)off + size) > tf->size)
	{
		size = tf->size - (size_t) off;
	};

	memcpy(buffer, &tf->data[off], size);

	return size;
};

static int fstat(File *file, struct stat *st)
{
	TarFile *tf = (TarFile*) file->fsdata;

	st->st_dev = 0;
	st->st_ino = (ino_t) tf;
	st->st_mode = 0755;
	st->st_nlink = 1;
	st->st_uid = 0;
	st->st_gid = 0;
	st->st_rdev = 0;
	st->st_size = tf->size;
	st->st_blksize = 512;
	st->st_blocks = st->st_size / 512;
	if (st->st_size % 512) st->st_blocks++;
	st->st_atime = 0;
	st->st_ctime = 0;
	st->st_mtime = 0;
	st->st_ixperm = XP_ALL;
	st->st_oxperm = 0;
	st->st_dxperm = XP_ALL;

	return 0;
};

static void pollinfo(File *fp, Semaphore **sems)
{
	sems[PEI_READ] = vfsGetConstSem();
	sems[PEI_WRITE] = vfsGetConstSem();
};

static FileTree* tree(File *fp)
{
	TarFile *tf = (TarFile*) fp->fsdata;
	ftUp(tf->header->ft);
	return tf->header->ft;
};

static int openfile(Dir *me, File *file, size_t szfile)
{
	(void)szfile;
	TarFile *tf = (TarFile*) kmalloc(sizeof(TarFile));
	file->fsdata = tf;

	TarState *state = (TarState*) me->fsdata;
	tf->data = (char*) &state->header[1];
	tf->size = me->stat.st_size;
	tf->pos = 0;
	tf->header = state->header;

	file->read = read;
	file->pread = pread;
	file->close = fileClose;
	file->seek = seek;
	file->fstat = fstat;
	file->pollinfo = pollinfo;
	file->tree = tree;

	return 0;
};

static int openroot(FileSystem *fs, Dir *dir, size_t szdir)
{
	(void)szdir;
	TarState *state = (TarState*) kmalloc(sizeof(TarState));
	dir->fsdata = state;
	state->prefix[0] = 0;
	state->dirDepth = 0;
	state->header = masterHeader;

	dir->dirent.d_ino = (ino_t) masterHeader;
	strcpyUntilSlash(dir->dirent.d_name, masterHeader->filename);
	dir->stat.st_dev = 0;
	dir->stat.st_ino = (ino_t) masterHeader;
	dir->stat.st_mode = 0555;
	if (masterHeader->filename[strlen(masterHeader->filename)-1] == '/') dir->stat.st_mode |= VFS_MODE_DIRECTORY;
	dir->stat.st_nlink = 1;
	dir->stat.st_uid = 0;
	dir->stat.st_gid = 0;
	dir->stat.st_rdev = 0;
	dir->stat.st_size = parseOct(masterHeader->size);
	dir->stat.st_blksize = 512;
	dir->stat.st_blocks = dir->stat.st_size / 512;
	if (dir->stat.st_size % 512) dir->stat.st_blocks++;
	dir->stat.st_atime = 0;
	dir->stat.st_ctime = 0;
	dir->stat.st_mtime = 0;
	dir->stat.st_ixperm = XP_ALL;
	dir->stat.st_oxperm = 0;
	dir->stat.st_dxperm = XP_ALL;

	dir->openfile = openfile;
	dir->opendir = opendir;
	dir->next = dirNext;
	dir->close = dirClose;

	return 0;
};

static int ft_load(FileTree *ft, off_t pos, void *buffer)
{
	TarHeader *head = (TarHeader*) ft->data;
	
	size_t sz = parseOct(head->size);
	char *data = (char*) &head[1];
	
	if (pos >= sz)
	{
		return -1;
	};
	
	size_t toRead = sz - pos;
	if (toRead > 0x1000) toRead = 0x1000;
	
	memcpy(buffer, &data[pos], toRead);
	return 0;
};

void initInitrdfs(KernelBootInfo *info)
{
	initrdfs = (FileSystem*) kmalloc(sizeof(FileSystem));
	memset(initrdfs, 0, sizeof(FileSystem));
	initrdfs->fsname = "initrdfs";
	initrdfs->openroot = openroot;

	initrdEnd = (uint64_t) initrdImage + info->initrdSize;
	masterHeader = (TarHeader*) initrdImage;

	TarHeader *header = masterHeader;
	while (1)
	{
		header->ft = ftCreate(FT_READONLY);
		header->ft->data = header;
		header->ft->load = ft_load;
		
		size_t sz = parseOct(header->size);
		header->ft->size = sz;
		size_t blocks = (sz + 511) / 512;
		
		header = &header[1+blocks];
		if ((uint64_t)(header) >= initrdEnd)
		{
			break;
		};

		if (header->filename[0] == 0)
		{
			break;
		};
	};
};

FileSystem *getInitrdfs()
{
	return initrdfs;
};
