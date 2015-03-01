/*
	Glidix kernel

	Copyright (c) 2014-2015, Madd Games.
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

#include "gxfs.h"
#include <glidix/console.h>

uint64_t GXAllocBlockInSection(GXFileSystem *gxfs, uint64_t section)
{
	uint64_t offbitmap = gxfs->sections[section].sdOffMapBlocks;
	uint64_t block = 0;
	gxfs->fp->seek(gxfs->fp, offbitmap, SEEK_SET);
	uint8_t mask = (1 << 0);
	uint8_t byte;
	vfsRead(gxfs->fp, &byte, 1);

	while (1)
	{
		if ((byte & mask) == 0)
		{
			byte |= mask;
			gxfs->fp->seek(gxfs->fp, -1, SEEK_CUR);
			vfsWrite(gxfs->fp, &byte, 1);

			//kprintf_debug("gxfs: allocated block %d\n", block);
			return block + gxfs->cis.cisBlocksPerSection * section;
		};

		block++;
		if (block == gxfs->cis.cisBlocksPerSection)
		{
			return GX_NO_BLOCK;
		};

		if (mask == 0x80)
		{
			mask = 1;
			vfsRead(gxfs->fp, &byte, 1);
		}
		else
		{
			mask <<= 1;
		};
	};
};

uint64_t GXAllocBlock(GXFileSystem *gxfs, uint64_t tryFirst)
{
	uint64_t section = tryFirst / gxfs->cis.cisBlocksPerSection;
	uint64_t index = tryFirst % gxfs->cis.cisBlocksPerSection;

	uint64_t offset = gxfs->sections[section].sdOffMapBlocks + (index/8);
	uint8_t mask = (1 << (index % 8));
	uint8_t byte;

	gxfs->fp->seek(gxfs->fp, offset, SEEK_SET);
	vfsRead(gxfs->fp, &byte, 1);

	if ((byte & mask) == 0)
	{
		byte |= mask;
		gxfs->fp->seek(gxfs->fp, -1, SEEK_CUR);
		vfsWrite(gxfs->fp, &byte, 1);
		//kprintf_debug("gxfs: allocated block %d\n", tryFirst);
		return tryFirst;
	};

	// now try the same section as the requested block
	uint64_t block = GXAllocBlockInSection(gxfs, section);
	if (block != GX_NO_BLOCK) return block;

	// now try all other sections
	uint64_t i;
	for (i=0; i<gxfs->numSections; i++)
	{
		if (i != section)
		{
			block = GXAllocBlockInSection(gxfs, i);
			if (block != GX_NO_BLOCK) return block;
		};
	};

	// no blocks left
	kprintf_debug("gxfs: out of free blocks!\n");
	return GX_NO_BLOCK;
};

void GXFreeBlock(GXFileSystem *gxfs, uint64_t block)
{
	uint64_t section = block / gxfs->cis.cisBlocksPerSection;
	uint64_t index = block % gxfs->cis.cisBlocksPerSection;

	uint64_t offset = gxfs->sections[section].sdOffMapBlocks + (index/8);
	uint8_t mask = (1 << (index % 8));
	uint8_t byte;

	gxfs->fp->seek(gxfs->fp, offset, SEEK_SET);
	vfsRead(gxfs->fp, &byte, 1);
	byte &= ~mask;
	gxfs->fp->seek(gxfs->fp, -1, SEEK_CUR);
	vfsWrite(gxfs->fp, &byte, 1);
};
