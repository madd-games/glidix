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
#include <glidix/time.h>
#include <glidix/string.h>

void GXDumpInode(GXFileSystem *gxfs, ino_t ino)
{
	GXInode gxino;
	GXOpenInode(gxfs, &gxino, ino);
	gxfs->fp->seek(gxfs->fp, gxino.offset, SEEK_SET);
	gxfsInode inode;
	vfsRead(gxfs->fp, &inode, sizeof(gxfsInode));

	kprintf_debug("Dumping fragment list for inode %d\n", ino);
	kprintf_debug("#\tOffset\tBlock\tExtent\n");
	int i;
	for (i=0; i<16; i++)
	{
		gxfsFragment *frag = &inode.inoFrags[i];
		kprintf_debug("%d\t%d\t%d\t%d\n", i, frag->fOff, frag->fBlock, frag->fExtent);
	};
};

void GXDumpInodeBuffer(GXFileSystem *gxfs)
{
	kprintf_debug("Currently-buffered inodes: %d*%d, %d*%d, %d*%d, %d*%d, %d*%d, %d*%d, %d*%d, %d*%d, %d*%d, %d*%d, %d*%d, %d*%d, %d*%d, %d*%d, %d*%d, %d*%d\n",
		gxfs->ibuf[0].num, gxfs->ibuf[0].counter,
		gxfs->ibuf[1].num, gxfs->ibuf[1].counter,
		gxfs->ibuf[2].num, gxfs->ibuf[2].counter,
		gxfs->ibuf[3].num, gxfs->ibuf[3].counter,
		gxfs->ibuf[4].num, gxfs->ibuf[4].counter,
		gxfs->ibuf[5].num, gxfs->ibuf[5].counter,
		gxfs->ibuf[6].num, gxfs->ibuf[6].counter,
		gxfs->ibuf[7].num, gxfs->ibuf[7].counter,
		gxfs->ibuf[8].num, gxfs->ibuf[8].counter,
		gxfs->ibuf[9].num, gxfs->ibuf[9].counter,
		gxfs->ibuf[10].num, gxfs->ibuf[10].counter,
		gxfs->ibuf[11].num, gxfs->ibuf[11].counter,
		gxfs->ibuf[12].num, gxfs->ibuf[12].counter,
		gxfs->ibuf[13].num, gxfs->ibuf[13].counter,
		gxfs->ibuf[14].num, gxfs->ibuf[14].counter,
		gxfs->ibuf[15].num, gxfs->ibuf[15].counter
	);
};

static ino_t GXCreateInodeInSection(GXFileSystem *gxfs, GXInode *gxino, uint64_t section)
{
	uint64_t offbitmap = gxfs->sections[section].sdOffMapIno;
	uint64_t inoIndex = 0;
	if (section == 0)
	{
		inoIndex = gxfs->cis.cisFirstDataIno - 1;
	};
	gxfs->fp->seek(gxfs->fp, offbitmap + (inoIndex / 8), SEEK_SET);
	uint8_t mask = (1 << (inoIndex % 8));
	uint8_t byte;
	vfsRead(gxfs->fp, &byte, 1);

	while (1)
	{
		if ((byte & mask) == 0)
		{
			byte |= mask;
			gxfs->fp->seek(gxfs->fp, -1, SEEK_CUR);
			vfsWrite(gxfs->fp, &byte, 1);

			ino_t ino = section * gxfs->cis.cisInoPerSection + inoIndex + 1;
			GXOpenInode(gxfs, gxino, ino);
			//kprintf_debug("gxfs: allocated inode %d\n", ino);
			return ino;
		};

		inoIndex++;
		if (inoIndex == gxfs->cis.cisInoPerSection)
		{
			return 0;
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

ino_t GXCreateInode(GXFileSystem *gxfs, GXInode *gxino, ino_t closeTo)
{
	// first try the same section as the 'closeTo' inode
	if (closeTo != 0)
	{
		uint64_t section = (closeTo-1) / gxfs->cis.cisInoPerSection;
		ino_t maybe = GXCreateInodeInSection(gxfs, gxino, section);
		if (maybe != 0) return maybe;
	};

	// OK this section is full so let's try all the sections
	uint64_t i;
	for (i=0; i<gxfs->numSections; i++)
	{
		ino_t maybe = GXCreateInodeInSection(gxfs, gxino, i);
		if (maybe != 0) return maybe;
	};

	// no inodes left :(
	//kprintf_debug("gxfs: out of free inodes!\n");
	return 0;
};

int GXOpenInode(GXFileSystem *gxfs, GXInode *gxino, ino_t inode)
{
	if (inode > gxfs->cis.cisTotalIno) return -1;

	size_t idxSection = (inode-1) / gxfs->cis.cisInoPerSection;
	size_t idxInode = (inode-1) % gxfs->cis.cisInoPerSection;

	gxino->gxfs = gxfs;
	gxino->offset = gxfs->sections[idxSection].sdOffTabIno + idxInode * sizeof(gxfsInode);
	gxino->ino = inode;
	gxino->pos = 0;

	//kprintf_debug("gxfs: inode %d (section %d, index %d) at %a\n", inode, idxSection, idxInode, gxino->offset);
	return 0;
};

size_t GXReadInode(GXInode *gxino, void *buffer, size_t size)
{
	//kprintf_debug("GXReadInode(%d)\n", gxino->ino);
	GXFileSystem *gxfs = gxino->gxfs;
	gxfsInode inode;
	//gxfs->fp->seek(gxfs->fp, gxino->offset, SEEK_SET);
	//vfsRead(gxfs->fp, &inode, sizeof(gxfsInode));
	GXReadInodeHeader(gxino, &inode);

	if ((gxino->pos+size) > inode.inoSize)
	{
		size = inode.inoSize - gxino->pos;
	};

	uint8_t *put = (uint8_t*) buffer;
	size_t sizeRead = 0;

	while (sizeRead < size)
	{
		// find the index of the block which stores the current position
		uint64_t block, offsetIntoBlock;
		int i;
		for (i=0; i<16; i++)
		{
			uint64_t fragStart = inode.inoFrags[i].fOff;
			uint64_t fragEnd = fragStart + inode.inoFrags[i].fExtent * gxfs->cis.cisBlockSize;
			if ((fragStart <= gxino->pos) && (fragEnd > gxino->pos))
			{
				uint64_t offsetIntoFragment = gxino->pos - fragStart;
				block = inode.inoFrags[i].fBlock + offsetIntoFragment / gxfs->cis.cisBlockSize;
				offsetIntoBlock = offsetIntoFragment % gxfs->cis.cisBlockSize;
				break;
			};
		};

		// no fragment contains this position
		if (i == 16)
		{
			//kprintf_debug("gxfs: no fragment contains position %d\n", gxino->pos);
			break;
		};

		// find out where this block is.
		uint64_t sectionContainingBlock = block / gxfs->cis.cisBlocksPerSection;
		uint64_t blockIndexInSection = block % gxfs->cis.cisBlocksPerSection;
		uint64_t offsetToBlock = gxfs->sections[sectionContainingBlock].sdOffTabBlocks + blockIndexInSection * gxfs->cis.cisBlockSize;

		//kprintf_debug("gxfs: read block %d\n", block);

		gxfs->fp->seek(gxfs->fp, offsetToBlock + offsetIntoBlock, SEEK_SET);
		//kprintf_debug("gxfs: reading from offset %a\n", offsetToBlock + offsetIntoBlock);
		size_t sizeLeftInBlock = gxfs->cis.cisBlockSize - offsetIntoBlock;
		if (sizeLeftInBlock > (size-sizeRead)) sizeLeftInBlock = size - sizeRead;
		ssize_t sizeThisTime = vfsRead(gxfs->fp, put, sizeLeftInBlock);
		put += sizeThisTime;
		sizeRead += sizeThisTime;
		gxino->pos += sizeThisTime;
	};

	// that took a lot of code...
	//kprintf_debug("END OF READ INODE\n");
	//GXDumpInodeBuffer(gxino->gxfs);
	return sizeRead;
};

void GXExpandInodeToSize(GXInode *gxino, gxfsInode *inode, size_t newSize)
{
	//kprintf_debug("gxfs: expanding inode %d (current size=%d, target size=%d)\n", gxino->ino, inode->inoSize, newSize);
	GXFileSystem *gxfs = gxino->gxfs;

	int i;
	for (i=0; i<16; i++)
	{
		if (inode->inoFrags[i].fExtent == 0)
		{
			break;
		};
	};

	if (i == 0)
	{
		uint64_t prefferedBlock = (gxino->ino-1) * gxfs->cis.cisTotalBlocks / gxfs->cis.cisTotalIno;
		inode->inoFrags[i].fOff = 0;
		inode->inoFrags[i].fBlock = GXAllocBlock(gxino->gxfs, prefferedBlock);
		inode->inoFrags[i].fExtent = 1;

		if (newSize > gxfs->cis.cisBlockSize)
		{
			inode->inoSize = gxfs->cis.cisBlockSize;
		}
		else
		{
			inode->inoSize = newSize;
		};

		return;
	};

	i--;
	uint64_t maxExtent = inode->inoFrags[i].fOff + inode->inoFrags[i].fExtent * gxfs->cis.cisBlockSize;
	//kprintf_debug("gxfs: max extent is %d, new size is %d\n", maxExtent, newSize);
	if (maxExtent >= newSize)
	{
		inode->inoSize = newSize;
		return;
	}
	else if ((maxExtent > inode->inoSize) && (maxExtent <= newSize))
	{
		// we first have to make sure that we're up to maximum extent before
		// increasing the extent.
		inode->inoSize = maxExtent;
		return;
	};

	uint64_t prefferedBlock = inode->inoFrags[i].fBlock + inode->inoFrags[i].fExtent;
	uint64_t actualBlock = GXAllocBlock(gxino->gxfs, prefferedBlock);

	if (prefferedBlock == actualBlock)
	{
		inode->inoFrags[i].fExtent++;
		if (inode->inoSize + gxfs->cis.cisBlockSize > newSize)
		{
			inode->inoSize = newSize;
		}
		else
		{
			inode->inoSize += gxfs->cis.cisBlockSize;
		};

		return;
	};

	// we must make a new fragment
	i++;
	if (i == 16)
	{
		// TODO: defrag or something
		panic("gxfs: fragmentation problem");
	};

	inode->inoFrags[i].fOff = inode->inoSize;
	inode->inoFrags[i].fBlock = actualBlock;
	inode->inoFrags[i].fExtent = 1;

	if (inode->inoSize + gxfs->cis.cisBlockSize > newSize)
	{
		inode->inoSize = newSize;
	}
	else
	{
		inode->inoSize += gxfs->cis.cisBlockSize;
	};
};

size_t GXWriteInode(GXInode *gxino, const void *buffer, size_t size)
{
	GXFileSystem *gxfs = gxino->gxfs;
	gxfsInode inode;
	//gxfs->fp->seek(gxfs->fp, gxino->offset, SEEK_SET);
	//vfsRead(gxfs->fp, &inode, sizeof(gxfsInode));
	GXReadInodeHeader(gxino, &inode);
	int inodeDirty = 0;

	if (gxino->pos > inode.inoSize)
	{
		gxino->pos = inode.inoSize;
	};

	while ((gxino->pos+size) > inode.inoSize)
	{
		inodeDirty = 1;
		GXExpandInodeToSize(gxino, &inode, gxino->pos + size);
	};

	size_t sizeWritten = 0;
	const uint8_t *fetch = (const uint8_t*) buffer;

	while (sizeWritten < size)
	{
		// find the index of the block which stores the current position
		uint64_t block, offsetIntoBlock;
		int i;
		for (i=0; i<16; i++)
		{
			uint64_t fragStart = inode.inoFrags[i].fOff;
			uint64_t fragEnd = fragStart + inode.inoFrags[i].fExtent * gxfs->cis.cisBlockSize;
			if ((fragStart <= gxino->pos) && (fragEnd > gxino->pos))
			{
				uint64_t offsetIntoFragment = gxino->pos - fragStart;
				block = inode.inoFrags[i].fBlock + offsetIntoFragment / gxfs->cis.cisBlockSize;
				offsetIntoBlock = offsetIntoFragment % gxfs->cis.cisBlockSize;
				break;
			};
		};

		// no fragment contains this position
		if (i == 16)
		{
			//kprintf_debug("gxfs: no fragment contains position %d\n", gxino->pos);
			break;
		};

		// find out where this block is.
		uint64_t sectionContainingBlock = block / gxfs->cis.cisBlocksPerSection;
		uint64_t blockIndexInSection = block % gxfs->cis.cisBlocksPerSection;
		uint64_t offsetToBlock = gxfs->sections[sectionContainingBlock].sdOffTabBlocks + blockIndexInSection * gxfs->cis.cisBlockSize;

		//kprintf_debug("gxfs: write block %d\n", block);
		size_t sizeLeftInBlock = gxfs->cis.cisBlockSize - offsetIntoBlock;
		if (sizeLeftInBlock > (size-sizeWritten)) sizeLeftInBlock = size - sizeWritten;
		gxfs->fp->seek(gxfs->fp, offsetToBlock + offsetIntoBlock, SEEK_SET);
		ssize_t sizeThisTime = vfsWrite(gxfs->fp, fetch, sizeLeftInBlock);
		fetch += sizeThisTime;
		sizeWritten += sizeThisTime;
		gxino->pos += sizeThisTime;
	};

	if (inodeDirty)
	{
		//kprintf_debug("gxfs: flushing dirty inode %d\n", gxino->ino);
		//gxfs->fp->seek(gxfs->fp, gxino->offset, SEEK_SET);
		//vfsWrite(gxfs->fp, &inode, sizeof(gxfsInode));
		GXWriteInodeHeader(gxino, &inode);
	};

	return sizeWritten;
};

void GXReadInodeHeader(GXInode *gxino, gxfsInode *inode)
{
	//kprintf_debug("GXReadInodeHeader (%d)\n", gxino->ino);
#if 0
	gxino->gxfs->fp->seek(gxino->gxfs->fp, gxino->offset, SEEK_SET);
	vfsRead(gxino->gxfs->fp, inode, sizeof(gxfsInode));
#endif

	int i;
	int freeIndex = -1;
	for (i=0; i<INODE_BUFFER_SIZE; i++)
	{
		if (gxino->gxfs->ibuf[i].num == 0)
		{
			freeIndex = i;
		};

		if (gxino->gxfs->ibuf[i].num == gxino->ino)
		{
			if (gxino->gxfs->ibuf[i].counter < INODE_IMPORTANCE_LIMIT) gxino->gxfs->ibuf[i].counter++;
			memcpy(inode, &gxino->gxfs->ibuf[i].data, sizeof(gxfsInode));
			return;
		};
	};

	if (freeIndex == -1)
	{
		// throw out the least-important inode and make all others less important
		freeIndex = 0;
		int importance = gxino->gxfs->ibuf[i].counter;

		for (i=1; i<INODE_BUFFER_SIZE; i++)
		{
			if (gxino->gxfs->ibuf[i].num < importance)
			{
				freeIndex = i;
				importance = gxino->gxfs->ibuf[i].num;
			};

			if (gxino->gxfs->ibuf[i].counter > 0) gxino->gxfs->ibuf[i].counter--;
		};
	};

	gxino->gxfs->fp->seek(gxino->gxfs->fp, gxino->offset, SEEK_SET);
	vfsRead(gxino->gxfs->fp, inode, sizeof(gxfsInode));
	gxino->gxfs->ibuf[freeIndex].num = gxino->ino;
	gxino->gxfs->ibuf[freeIndex].counter = 1;
	memcpy(&gxino->gxfs->ibuf[freeIndex].data, inode, sizeof(gxfsInode));
};

void GXWriteInodeHeader(GXInode *gxino, gxfsInode *inode)
{
	//kprintf_debug("GXWriteInodeHeader (%d)\n", gxino->ino);

	int i;
	int freeIndex = -1;
	for (i=0; i<INODE_BUFFER_SIZE; i++)
	{
		if (gxino->gxfs->ibuf[i].num == 0)
		{
			freeIndex = i;
		};

		if (gxino->gxfs->ibuf[i].num == gxino->ino)
		{
			if (gxino->gxfs->ibuf[i].counter < INODE_IMPORTANCE_LIMIT) gxino->gxfs->ibuf[i].counter++;
			memcpy(&gxino->gxfs->ibuf[i].data, inode, sizeof(gxfsInode));
		};
	};

	if (freeIndex == -1)
	{
		// throw out the least-important inode and make all others less important
		freeIndex = 0;
		int importance = gxino->gxfs->ibuf[i].counter;

		for (i=1; i<INODE_BUFFER_SIZE; i++)
		{
			if (gxino->gxfs->ibuf[i].num < importance)
			{
				freeIndex = i;
				importance = gxino->gxfs->ibuf[i].num;
			};

			if (gxino->gxfs->ibuf[i].counter > 0) gxino->gxfs->ibuf[i].counter--;
		};
	};

	gxino->gxfs->ibuf[freeIndex].num = gxino->ino;
	gxino->gxfs->ibuf[freeIndex].counter = 1;
	memcpy(&gxino->gxfs->ibuf[freeIndex].data, inode, sizeof(gxfsInode));

	gxino->gxfs->fp->seek(gxino->gxfs->fp, gxino->offset, SEEK_SET);
	vfsWrite(gxino->gxfs->fp, inode, sizeof(gxfsInode));
};

void GXShrinkInode(GXInode *gxino, size_t shrinkBy, gxfsInode *inode)
{
	//gxfsFragment frags[16];
	//gxino->gxfs->fp->seek(gxino->gxfs->fp, gxino->offset+39, SEEK_SET);
	//vfsRead(gxino->gxfs->fp, frags, sizeof(gxfsFragment)*16);
	gxfsFragment *frags = inode->inoFrags;

	int i;
	for (i=0; i<16; i++)
	{
		if (frags[i].fExtent == 0) break;
	};
	i--;

	size_t blocksBefore = inode->inoSize / gxino->gxfs->cis.cisBlockSize;
	size_t blocksAfter = (inode->inoSize-shrinkBy) / gxino->gxfs->cis.cisBlockSize;
	size_t blocksToFree = blocksBefore-blocksAfter;

	while (blocksToFree--)
	{
		frags[i].fExtent--;
		uint64_t block = frags[i].fBlock + frags[i].fExtent;
		GXFreeBlock(gxino->gxfs, block);
		if (frags[i].fExtent == 0)
		{
			if (i == 0) break;
			i--;
		};	
	};

	inode->inoSize -= shrinkBy;
	//gxino->gxfs->fp->seek(gxino->gxfs->fp, gxino->offset+39, SEEK_SET);
	//vfsWrite(gxino->gxfs->fp, frags, sizeof(gxfsFragment)*16);
	GXWriteInodeHeader(gxino, inode);
};

void GXUnlinkInode(GXInode *gxino)
{
	gxfsInode inode;
	GXReadInodeHeader(gxino, &inode);
	inode.inoCTime = time();
	inode.inoLinks--;
	GXWriteInodeHeader(gxino, &inode);

	if (inode.inoLinks == 0)
	{
		//gxfsFragment frags[16];
		//gxino->gxfs->fp->seek(gxino->gxfs->fp, gxino->offset+39, SEEK_SET);
		//vfsRead(gxino->gxfs->fp, frags, sizeof(gxfsFragment)*16);
		gxfsFragment *frags = inode.inoFrags;

		int i;
		for (i=0; i<16; i++)
		{
			while (frags[i].fExtent != 0)
			{
				GXFreeBlock(gxino->gxfs, frags[i].fBlock);
				frags[i].fBlock++;
				frags[i].fExtent--;
			};
		};

		uint64_t section = gxino->ino / gxino->gxfs->cis.cisInoPerSection;
		uint64_t index = gxino->ino % gxino->gxfs->cis.cisInoPerSection;
		uint64_t offset = gxino->gxfs->sections[section].sdOffMapIno + (index/8);
		uint8_t mask = (1 << (index%8));

		gxino->gxfs->fp->seek(gxino->gxfs->fp, offset, SEEK_SET);
		uint8_t byte;
		vfsRead(gxino->gxfs->fp, &byte, 1);
		byte &= ~mask;
		gxino->gxfs->fp->seek(gxino->gxfs->fp, -1, SEEK_CUR);
		vfsWrite(gxino->gxfs->fp, &byte, 1);
	};
};
