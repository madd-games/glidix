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

#ifdef GXBOOT_FS_ELTORITO

#include "gxboot.h"

byte_t fsBootID[16];
ISOPrimaryVolumeDescriptor pvd;

static void readBlock(qword_t index, void *buffer)
{
	dap.lba = index;
	biosRead();
	memcpy(buffer, sectorBuffer, 2048);
};

static int checkPVD(ISOPrimaryVolumeDescriptor *pvd)
{
	if (pvd->type != 1)
	{
		return 0;
	};

	if (memcmp(pvd->magic, "CD001", 5) != 0)
	{
		return 0;
	};

	if (pvd->version != 1)
	{
		return 0;
	};

	return 1;
};

void fsInit()
{
	memcpy(fsBootID, "\0\0" "ISOBOOT" "\0\0\0\0\0\xF0\x0D", 16);
	dap.numSectors = 1;
	dap.offset = (word_t) (dword_t) sectorBuffer;
	
	// read the Primary Volume Descriptor
	dtermput("Loading the Primary Volume Descriptor... ");
	char blockbuf[2048];
	readBlock(0x10, blockbuf);
	memcpy(&pvd, blockbuf, sizeof(ISOPrimaryVolumeDescriptor));
	
	if (!checkPVD(&pvd))
	{
		dtermput("FAILED\n");
		termput("ERROR: invalid PVD\n");
		while (1) asm ("cli; hlt");
	};
	
	dtermput("OK\n");
};

static int dirWalk(ISODirentHeader *head, const char *name)
{
	if ((head->flags & 2) == 0)
	{
		return -1;
	};
	
	qword_t nextLBA = head->startLBA;
	qword_t sizeLeft = head->fileSize;
	qword_t readpos = 0;
	
	byte_t buffer[2048];
	readBlock(nextLBA++, buffer);
	
	while (sizeLeft)
	{
		char filename[256];
		memset(filename, 0, 256);
		
		if (readpos == 2048 || buffer[readpos] == 0)
		{
			qword_t sizeHere = 2048 - readpos;
			if (sizeHere > sizeLeft) break;
			sizeLeft -= sizeHere;
			
			readpos = 0;
			readBlock(nextLBA++, buffer);
			continue;
		};
		
		ISODirentHeader *ent = (ISODirentHeader*) &buffer[readpos];
		memcpy(filename, &ent[1], ent->filenameLen);
		
		char *scan;
		for (scan=filename; *scan!=0; scan++)
		{
			if (*scan == ';')
			{
				*scan = 0;
				break;
			};
			
			*scan |= (1 << 5);			// make it lowercase
		};
		
		if (strcmp(filename, name) == 0)
		{
			memcpy(head, ent, sizeof(ISODirentHeader));
			return 0;
		};
		
		readpos += ent->size;
		sizeLeft -= ent->size;
	};
	
	return -1;
};

int openFile(FileHandle *fh, const char *path)
{
	ISODirentHeader head;
	memcpy(&head, pvd.rootDir, 34);
	
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
		
		if (dirWalk(&head, token) != 0) return -1;
	};
	
	if (head.flags & 2)
	{
		// directory; not a file
		return -1;
	};
	
	fh->startLBA = head.startLBA;
	fh->size = head.fileSize;
	fh->currentLBA = 0;
	
	return 0;
};

void readFile(FileHandle *fh, void *buffer, qword_t size, qword_t pos)
{
	char *put = (char*) buffer;
	while (size)
	{
		qword_t targetBlock = fh->startLBA + (pos >> 11);
		if (fh->currentLBA != targetBlock)
		{
			fh->currentLBA = targetBlock;
			readBlock(fh->currentLBA, fh->buffer);
		};
		
		qword_t offset = pos & 0x7FF;
		qword_t sizeNow = 2048 - offset;
		if (sizeNow > size) sizeNow = size;
		
		memcpy(put, &fh->buffer[offset], sizeNow);
		
		put += sizeNow;
		size -= sizeNow;
		pos += sizeNow;
	};
};

#endif
