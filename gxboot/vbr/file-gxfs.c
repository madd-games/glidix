/*
	Glidix bootloader (gxboot)

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

#ifdef GXBOOT_FS_GXFS

#include "gxboot.h"

extern dword_t part_start;
extern qword_t blockBase;			/* LBA of start of block table */
byte_t fsBootID[16];

static void readBlock(qword_t index, void *buffer)
{
	dap.lba = blockBase + (index << 3);
	biosRead();
	memcpy(buffer, sectorBuffer, 4096);
};

void fsInit()
{
	dtermput("Validating superblock... ");
	blockBase = (qword_t) part_start + 0x1000;
	dap.numSectors = 8;
	dap.offset = (word_t) (dword_t) sectorBuffer;
	
	char sb[4096];
	readBlock(0, sb);
	
	GXFS_SuperblockHeader *sbh = (GXFS_SuperblockHeader*) sb;
	if (sbh->sbhMagic != (*((const qword_t*)"__GXFS__")))
	{
		dtermput("FAILED\n");
		termput("ERROR: Superblock has invalid magic number (should be __GXFS__)\n");
		return;
	};
	
	qword_t state = 0xF00D1234BEEFCAFE;
	qword_t *scan = (qword_t*) sb;
	int count = 9;
	
	while (count--)
	{
		state = (state << 1) ^ (*scan++);
	};
	
	if ((*scan) != state)
	{
		dtermput("FAILED\n");
		termput("ERROR: Superblock has invalid checksum\n");
		return;
	};
	
	memcpy(fsBootID, sbh->sbhBootID, 16);
	dtermput("OK\n");
};

static qword_t dirWalk(qword_t inode, const char *name)
{
	char blockbuf[4096];
	readBlock(inode, blockbuf);
	
	GXFS_InodeHeader *ih = (GXFS_InodeHeader*) blockbuf;
	qword_t readpos = 8;
	
	while (1)
	{
		while (readpos >= 4096)
		{
			if (ih->ihNext == 0)
			{
				return 0;
			};
			
			readBlock(ih->ihNext, blockbuf);
			readpos -= 4096;
			readpos += 8;
		};
		
		GXFS_RecordHeader *rh = (GXFS_RecordHeader*) &blockbuf[readpos];
		if ((rh->rhType & 0xFF) == 0)
		{
			return 0;
		};
		
		if (rh->rhType != (*((const dword_t*)"DENT")))
		{
			readpos += rh->rhSize;
			continue;
		};
		
		if (rh->rhSize > 128)
		{
			readpos += rh->rhSize;
		};
		
		char recbuf[129];
		memset(recbuf, 0, 129);
		
		qword_t toRead = rh->rhSize;
		char *put = recbuf;
		while (toRead != 0)
		{
			while (readpos >= 4096)
			{
				if (ih->ihNext == 0)
				{
					return 0;
				};
			
				readBlock(ih->ihNext, blockbuf);
				readpos -= 4096;
				readpos += 8;
			};
			
			qword_t maxRead = 4096 - readpos;
			qword_t readNow = toRead;
			if (readNow > maxRead) readNow = maxRead;
			
			memcpy(put, &blockbuf[readpos], readNow);
			put += readNow;
			toRead -= readNow;
			readpos += readNow;
		};
		
		GXFS_DentRecord *dr = (GXFS_DentRecord*) recbuf;
		if (strcmp(dr->drName, name) == 0)
		{
			return dr->drInode;
		};
	};
};

static void loadFileBlock(FileHandle *fh, qword_t offset)
{
	fh->bufferBase = offset & ~0xFFFULL;
	
	qword_t lvl[5];
	lvl[4] = (offset >> 12) & 0x1FF;
	lvl[3] = (offset >> 21) & 0x1FF;
	lvl[2] = (offset >> 30) & 0x1FF;
	lvl[1] = (offset >> 39) & 0x1FF;
	lvl[0] = (offset >> 48) & 0x1FF;
	
	qword_t datablock = fh->head;
	int i;
	for (i=(5-fh->depth); i<5; i++)
	{
		qword_t table[512];
		readBlock(datablock, table);
		
		if (table[lvl[i]] == 0)
		{
			memset(fh->buffer, 0, 4096);
			return;
		}
		else
		{
			datablock = table[lvl[i]];
		};
	};
	
	readBlock(datablock, fh->buffer);
};

int openFile(FileHandle *fh, const char *path)
{
	// start with the root directory
	qword_t currentIno = 2;
	
	char token[128];
	while (*path != 0)
	{
		char *put = token;
		while (*path == '/') path++;
		
		while ((*path != '/') && (*path != 0))
		{
			*put++ = *path++;
		};
		
		*put = 0;
		
		currentIno = dirWalk(currentIno, token);
		if (currentIno == 0) return -1;
	};
	
	char blockbuf[4096];
	readBlock(currentIno, blockbuf);
	
	GXFS_InodeHeader *ih = (GXFS_InodeHeader*) blockbuf;
	qword_t readpos = 8;
	
	// unknown size right now
	fh->size = 0;
	
	while (1)
	{
		while (readpos >= 4096)
		{
			if (ih->ihNext == 0)
			{
				return -1;
			};
			
			readBlock(ih->ihNext, blockbuf);
			readpos -= 4096;
			readpos += 8;
		};
		
		GXFS_RecordHeader *rh = (GXFS_RecordHeader*) &blockbuf[readpos];
		if ((rh->rhType & 0xFF) == 0)
		{
			return -1;
		};
		
		if (rh->rhType != (*((const dword_t*)"TREE")) && rh->rhType != (*((const dword_t*)"ATTR")))
		{
			readpos += rh->rhSize;
			continue;
		};
		
		char recbuf[129];
		memset(recbuf, 0, 129);
		
		qword_t toRead = rh->rhSize;
		char *put = recbuf;
		while (toRead != 0)
		{
			while (readpos >= 4096)
			{
				if (ih->ihNext == 0)
				{
					return -1;
				};
			
				readBlock(ih->ihNext, blockbuf);
				readpos -= 4096;
				readpos += 8;
			};
			
			qword_t maxRead = 4096 - readpos;
			qword_t readNow = toRead;
			if (readNow > maxRead) readNow = maxRead;
			
			memcpy(put, &blockbuf[readpos], readNow);
			put += readNow;
			toRead -= readNow;
			readpos += readNow;
		};
		
		rh = (GXFS_RecordHeader*) recbuf;
		if (rh->rhType == (*((const dword_t*)"TREE")))
		{
			GXFS_TreeRecord *tr = (GXFS_TreeRecord*) recbuf;
			fh->depth = tr->trDepth;
			fh->head = tr->trHead;
			loadFileBlock(fh, 0);
			return 0;
		}
		else
		{
			GXFS_AttrRecord *ar = (GXFS_AttrRecord*) recbuf;
			fh->size = ar->arSize;
		};
	};
};

void readFile(FileHandle *fh, void *buffer, qword_t size, qword_t pos)
{
	char *put = (char*) buffer;
	while (size != 0)
	{
		qword_t base = pos & ~0xFFFULL;
		qword_t offset = pos & 0xFFFULL;
		
		if (base != fh->bufferBase)
		{
			loadFileBlock(fh, base);
		};
		
		qword_t sizeNow = 4096 - offset;
		if (sizeNow > size) sizeNow = size;
		
		memcpy(put, &fh->buffer[offset], sizeNow);
		put += sizeNow;
		size -= sizeNow;
		pos += sizeNow;
	};
};

#endif	/* GXBOOT_FS_GXFS */
