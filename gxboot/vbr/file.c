/*
	Glidix bootloader (gxboot)

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

#include "gxboot.h"

extern qword_t blockBase;			/* LBA of start of block table */

void readBlock(qword_t index, void *buffer)
{
	dap.lba = blockBase + index;
	biosRead();
	memcpy(buffer, sectorBuffer, 512);
};

static void setFileBlock(FileHandle *fh, qword_t blockno)
{
	fh->bufIndex = blockno;
	
	if (blockno >= (1 << (6 * fh->inode.inoTreeDepth)))
	{
		memset(fh->buffer, 0, 512);
		return;
	};
	
	qword_t lba = fh->inode.inoRoot;
	
	word_t i;
	for (i=0; i<fh->inode.inoTreeDepth; i++)
	{
		qword_t ind[64];
		readBlock(lba, ind);
		
		unsigned int bitpos = 6 * (fh->inode.inoTreeDepth - i - 1);
		qword_t index = (blockno >> bitpos) & 0x3F;
		
		if (ind[index] == 0)
		{
			memset(fh->buffer, 0, 512);
			return;
		};
		
		lba = ind[index];
	};
	
	readBlock(lba, fh->buffer);
};

void openFileByIno(FileHandle *fh, qword_t ino)
{
	readBlock(ino, &fh->inode);
	fh->bufIndex = (qword_t) -1;
};

void readFile(FileHandle *fh, void *buffer, qword_t size, qword_t pos)
{
	memset(buffer, 0, size);
	
	char *put = (char*) buffer;
	while (size > 0)
	{
		qword_t blockno = pos >> 9;
		qword_t offset = pos & 0x1FFULL;
		qword_t sizeNow = 512 - offset;
		if (sizeNow > size) sizeNow = size;
		
		if (fh->bufIndex != blockno) setFileBlock(fh, blockno);
		
		memcpy(put, &fh->buffer[offset], sizeNow);
		
		put += sizeNow;
		size -= sizeNow;
		pos += sizeNow;
	};
};

static int dirWalk(FileHandle *fh, const char *token)
{
	Dirent ent;
	
	qword_t pos = 0;
	while (pos < fh->inode.inoSize)
	{
		readFile(fh, &ent, 256, pos);
		
		if (strcmp(ent.deName, token) == 0)
		{
			openFileByIno(fh, ent.deInode);
			return 0;
		};
		
		pos += (ent.deOpt & 0xF0);
	};
	
	return -1;
};

int openFile(FileHandle *fh, const char *path)
{
	openFileByIno(fh, 2);
	
	char token[128];
	while (*path != 0)
	{
		if ((fh->inode.inoMode & 0xF000) != 0x1000)
		{
			return -1;
		};
		
		char *put = token;
		while (*path == '/') path++;
		
		while ((*path != '/') && (*path != 0))
		{
			*put++ = *path++;
		};
		
		*put = 0;
		
		if (dirWalk(fh, token) != 0) return -1;
	};
	
	if ((fh->inode.inoMode & 0xF000) != 0)
	{
		// not a regular file
		return -1;
	};
	
	return 0;
};
