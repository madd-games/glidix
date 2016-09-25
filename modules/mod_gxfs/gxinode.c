/*
	Glidix kernel

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

#include "gxfs.h"
#include <glidix/console.h>
#include <glidix/time.h>
#include <glidix/string.h>
#include <glidix/memory.h>

#if 0
void GXDumpInode(GXFileSystem *gxfs, ino_t ino)
{
	GXInode gxino;
	GXOpenInode(gxfs, &gxino, ino);
	gxfs->fp->seek(gxfs->fp, gxino.offset, SEEK_SET);
	gxfsInode inode;
	vfsRead(gxfs->fp, &inode, sizeof(gxfsInode));

	//kprintf("Dumping fragment list for inode %d\n", ino);
	//kprintf("#\tOffset\tBlock\tExtent\n");
	int i;
	for (i=0; i<14; i++)
	{
		gxfsFragment *frag = &inode.inoFrags[i];
		//kprintf("%d\t%p\t%p\t%p\n", i, frag->fOff, frag->fBlock, frag->fExtent);
	};

	gxfsXFT *xft = (gxfsXFT*) kmalloc(gxfs->cis.cisBlockSize);

	uint64_t xftBlock = inode.inoExFrag;
	int count = 0;
	while (xftBlock != 0)
	{
		uint64_t sectionContainingBlock = xftBlock / gxfs->cis.cisBlocksPerSection;
		uint64_t blockIndexInSection = xftBlock % gxfs->cis.cisBlocksPerSection;
		uint64_t offsetToBlock = gxfs->sections[sectionContainingBlock].sdOffTabBlocks + blockIndexInSection * gxfs->cis.cisBlockSize;
		gxfs->fp->seek(gxfs->fp, offsetToBlock, SEEK_SET);
		vfsRead(gxfs->fp, xft, gxfs->cis.cisBlockSize);
		//kprintf("<XFT at %p, level %d, count %d>\n", xftBlock, count++, xft->xftCount);
		
		for (i=0; i<xft->xftCount; i++)
		{
			gxfsFragment *frag = &xft->xftFrags[i];
			if (frag->fExtent == 0) break;
			//kprintf("%d\t%p\t%p\t%p\n", i, frag->fOff, frag->fBlock, frag->fExtent);
		};
		
		xftBlock = xft->xftNext;
	};
};
#endif

void GXDumpInodeBuffer(GXFileSystem *gxfs)
{
#if 0
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
#endif
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
			gxfs->usedInodes++;
			
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

/**
 * Try to find a given file offset in an XFT (recursively traversing subsequent XFTs),
 * return 0 if successful, -1 otherwise. The block number and the offset into the block
 * are returned in the given pointers.
 */
static int GXPositionXFT(GXFileSystem *gxfs, uint64_t xftBlock, uint64_t pos, uint64_t *outBlock, uint64_t *outOffsetIntoBlock)
{
	int status = -1;
	gxfsXFT *xft = (gxfsXFT*) kmalloc(gxfs->cis.cisBlockSize);
	
	while (xftBlock != 0)
	{
		uint64_t sectionContainingBlock = xftBlock / gxfs->cis.cisBlocksPerSection;
		uint64_t blockIndexInSection = xftBlock % gxfs->cis.cisBlocksPerSection;
		uint64_t offsetToBlock = gxfs->sections[sectionContainingBlock].sdOffTabBlocks + blockIndexInSection * gxfs->cis.cisBlockSize;
		gxfs->fp->seek(gxfs->fp, offsetToBlock, SEEK_SET);
		vfsRead(gxfs->fp, xft, gxfs->cis.cisBlockSize);
		
		uint16_t numFragsExpected = (gxfs->cis.cisBlockSize-10)/20;
		if (xft->xftCount > numFragsExpected)
		{
			// return error on bad XFTs.
			panic("[GXFS] bad XFT: xftCount is %d, max possible is %d", (int)xft->xftCount, (int)numFragsExpected);
		};
		
		uint16_t i;
		for (i=0; i<xft->xftCount; i++)
		{
			uint64_t fragStart = xft->xftFrags[i].fOff;
			uint64_t fragEnd = fragStart + xft->xftFrags[i].fExtent * gxfs->cis.cisBlockSize;
			if ((fragStart <= pos) && (fragEnd > pos))
			{
				// found a block!
				uint64_t offsetIntoFragment = pos - fragStart;
				*outBlock = xft->xftFrags[i].fBlock + offsetIntoFragment / gxfs->cis.cisBlockSize;
				*outOffsetIntoBlock = offsetIntoFragment % gxfs->cis.cisBlockSize;
				status = 0;
				break;
			};
		};
		
		// try next XFT
		xftBlock = xft->xftNext;
	};
	
	kfree(xft);
	return status;
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
		for (i=0; i<14; i++)
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

		
		// no fragment in the main table contains this position; try XFT
		if (i == 14)
		{
			if (GXPositionXFT(gxino->gxfs, inode.inoExFrag, gxino->pos, &block, &offsetIntoBlock) != 0)
			{
				// not found in XFT either, stop trying to read
				break;
			};
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

/**
 * Create an XFT at the specified block, and place a single fragment with a single
 * block in it.
 */
static void GXCreateXFT(GXInode *gxino, gxfsInode *inode, uint64_t xftBlock)
{
	gxfsXFT *xft = (gxfsXFT*) kmalloc(gxino->gxfs->cis.cisBlockSize);
	memset(xft, 0, gxino->gxfs->cis.cisBlockSize);
	
	// set the count to maximum cause why waste space
	xft->xftCount = (gxino->gxfs->cis.cisBlockSize-10)/20;
	
	uint16_t initBlock = GXAllocBlock(gxino->gxfs, xftBlock+1);
	xft->xftFrags[0].fOff = inode->inoSize;
	xft->xftFrags[0].fBlock = initBlock;
	xft->xftFrags[0].fExtent = 1;
	
	uint64_t sectionContainingBlock = xftBlock / gxino->gxfs->cis.cisBlocksPerSection;
	uint64_t blockIndexInSection = xftBlock % gxino->gxfs->cis.cisBlocksPerSection;
	uint64_t offsetToBlock = gxino->gxfs->sections[sectionContainingBlock].sdOffTabBlocks + blockIndexInSection * gxino->gxfs->cis.cisBlockSize;
	gxino->gxfs->fp->seek(gxino->gxfs->fp, offsetToBlock, SEEK_SET);
	vfsWrite(gxino->gxfs->fp, xft, gxino->gxfs->cis.cisBlockSize);
	
	kfree(xft);
};

/**
 * Expand an inode in its XFS - that is, allocate a new block to the XFT. Expand inoSize by 1 block,
 * or up to 'newSize', whichever is smaller.
 */
static void GXExpandXFT(GXInode *gxino, gxfsInode *inode, size_t newSize)
{
	//kprintf("EXPAND TO %p (from %p)\n", newSize, inode->inoSize);
	GXFileSystem *gxfs = gxino->gxfs;
	gxfsXFT *xft = (gxfsXFT*) kmalloc(gxfs->cis.cisBlockSize);
	
	uint64_t xftBlock = inode->inoExFrag;
	while (xftBlock != 0)
	{
		uint64_t sectionContainingBlock = xftBlock / gxfs->cis.cisBlocksPerSection;
		uint64_t blockIndexInSection = xftBlock % gxfs->cis.cisBlocksPerSection;
		uint64_t offsetToBlock = gxfs->sections[sectionContainingBlock].sdOffTabBlocks + blockIndexInSection * gxfs->cis.cisBlockSize;
		gxfs->fp->seek(gxfs->fp, offsetToBlock, SEEK_SET);
		vfsRead(gxfs->fp, xft, gxfs->cis.cisBlockSize);
		
		uint16_t numFragsExpected = (gxfs->cis.cisBlockSize-10)/20;
		if (xft->xftCount > numFragsExpected)
		{
			// return error on bad XFTs.
			panic("[GXFS] bad XFT: xftCount is %d, max possible is %d\n", (int)xft->xftCount, (int)numFragsExpected);
			break;
		};
		
		uint16_t i;
		for (i=0; i<xft->xftCount; i++)
		{
			if (xft->xftFrags[i].fExtent == 0)
			{
				break;
			};
		};
		
		if (i == 0)
		{
			panic("[GXFS] Empty XFT encountered");
		};
		
		// select the last fragment
		i--;
		
		// expand to max extent if possible
		uint64_t maxExtent = xft->xftFrags[i].fOff + xft->xftFrags[i].fExtent * gxfs->cis.cisBlockSize;
		if (maxExtent >= newSize)
		{
			inode->inoSize = newSize;
			break;
		}
		else if ((maxExtent > inode->inoSize) && (maxExtent <= newSize))
		{
			// we first have to make sure that we're up to maximum extent before
			// increasing the extent.
			inode->inoSize = maxExtent;
			break;
		};

		uint64_t prefferedBlock = xft->xftFrags[i].fBlock + xft->xftFrags[i].fExtent;
		uint64_t actualBlock = GXAllocBlock(gxino->gxfs, prefferedBlock);

		if (prefferedBlock == actualBlock)
		{
			xft->xftFrags[i].fExtent++;
			if (inode->inoSize + gxfs->cis.cisBlockSize > newSize)
			{
				inode->inoSize = newSize;
			}
			else
			{
				inode->inoSize += gxfs->cis.cisBlockSize;
			};

			break;
		};
		
		i++;
		if (i == xft->xftCount)
		{
			if (xft->xftNext == 0)
			{
				GXCreateXFT(gxino, inode, actualBlock);
				xft->xftNext = actualBlock;
			}
			else
			{
				GXFreeBlock(gxfs, actualBlock);
				xftBlock = xft->xftNext;
				continue;
			};
		}
		else
		{
			xft->xftFrags[i].fOff = inode->inoSize;
			xft->xftFrags[i].fBlock = actualBlock;
			xft->xftFrags[i].fExtent = 1;
		};
		
		if (inode->inoSize + gxfs->cis.cisBlockSize > newSize)
		{
			inode->inoSize = newSize;
		}
		else
		{
			inode->inoSize += gxfs->cis.cisBlockSize;
		};
		
		break;
	};

	uint64_t sectionContainingBlock = xftBlock / gxfs->cis.cisBlocksPerSection;
	uint64_t blockIndexInSection = xftBlock % gxfs->cis.cisBlocksPerSection;
	uint64_t offsetToBlock = gxfs->sections[sectionContainingBlock].sdOffTabBlocks + blockIndexInSection * gxfs->cis.cisBlockSize;
	gxfs->fp->seek(gxfs->fp, offsetToBlock, SEEK_SET);
	vfsWrite(gxfs->fp, xft, gxfs->cis.cisBlockSize);
		
	kfree(xft);
};

void GXExpandInodeToSize(GXInode *gxino, gxfsInode *inode, size_t newSize)
{	
	//kprintf_debug("gxfs: expanding inode %d (current size=%d, target size=%d)\n", gxino->ino, inode->inoSize, newSize);
	GXFileSystem *gxfs = gxino->gxfs;

	int i;
	for (i=0; i<14; i++)
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
	if (i == 14)
	{
		// if there is no XFT, create it in the newly-allocated block, allocating
		// an initial fragment (in GXCreateXFT() ). otherwise expand existing XFTs
		// with an extra block.
		if (inode->inoExFrag == 0)
		{
			GXCreateXFT(gxino, inode, actualBlock);
			inode->inoExFrag = actualBlock;
		}
		else
		{
			// this function will also free "actualBlock" if it's not actually needed.
			// it will take care of adding the size to inoSize etc so we can safely
			// just return straight after.
			GXFreeBlock(gxino->gxfs, actualBlock);
			GXExpandXFT(gxino, inode, newSize);
			return;
		};
	}
	else
	{
		inode->inoFrags[i].fOff = inode->inoSize;
		inode->inoFrags[i].fBlock = actualBlock;
		inode->inoFrags[i].fExtent = 1;
	};

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
	GXReadInodeHeader(gxino, &inode);
	int inodeDirty = 0;

	while ((gxino->pos+size) > inode.inoSize)
	{
		inodeDirty = 1;
		GXExpandInodeToSize(gxino, &inode, gxino->pos + size);
		//kprintf("NOW SIZE IS %p\n", inode.inoSize);
	};

	size_t sizeWritten = 0;
	const uint8_t *fetch = (const uint8_t*) buffer;

	while (sizeWritten < size)
	{
		// find the index of the block which stores the current position
		uint64_t block, offsetIntoBlock;
		int i;
		for (i=0; i<14; i++)
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
		if (i == 14)
		{
			if (GXPositionXFT(gxino->gxfs, inode.inoExFrag, gxino->pos, &block, &offsetIntoBlock) != 0)
			{
				// not found in XFT either, stop trying to write
				//GXDumpInode(gxino->gxfs, gxino->ino);
				panic("cannot find position %p in the file!", gxino->pos);
				break;
			};
		};

		// find out where this block is.
		uint64_t sectionContainingBlock = block / gxfs->cis.cisBlocksPerSection;
		uint64_t blockIndexInSection = block % gxfs->cis.cisBlocksPerSection;
		uint64_t offsetToBlock = gxfs->sections[sectionContainingBlock].sdOffTabBlocks + blockIndexInSection * gxfs->cis.cisBlockSize;

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

/**
 * Shrinks an inode by removing its last few blocks from XFTs. This finds the last XFT,
 * shrinks as much as possible, and if no fragments are left on that XFT, it deletes it,
 * seeting xftNext of the previous XFT to zero. If this is the only XFT, then 'xftOut'
 * is set to zero. 'repeat' is set to 1 if the function must be called again, or 0 if
 * that is not needed.
 */
static void GXShrinkXFT(GXInode *gxino, size_t shrinkBy, gxfsInode *inode, uint64_t *xftOut, int *repeat, uint64_t *outBlocksToFree)
{
	GXFileSystem *gxfs = gxino->gxfs;
	if ((*outBlocksToFree) == 0)
	{
		*repeat = 0;
		return;
	};
	
	gxfsXFT *xft = (gxfsXFT*) kmalloc(gxfs->cis.cisBlockSize);
	xft->xftNext = 0;
	
	uint64_t xftBlock = *xftOut;
	uint64_t prevXFT = 0;
	while (1)
	{
		uint64_t sectionContainingBlock = xftBlock / gxfs->cis.cisBlocksPerSection;
		uint64_t blockIndexInSection = xftBlock % gxfs->cis.cisBlocksPerSection;
		uint64_t offsetToBlock = gxfs->sections[sectionContainingBlock].sdOffTabBlocks + blockIndexInSection * gxfs->cis.cisBlockSize;
		gxfs->fp->seek(gxfs->fp, offsetToBlock, SEEK_SET);
		vfsRead(gxfs->fp, xft, gxfs->cis.cisBlockSize);
		
		if (xft->xftNext != 0)
		{
			prevXFT = xftBlock;
			xftBlock = xft->xftNext;
		}
		else
		{
			break;
		};
	};
	
	uint16_t i;
	for (i=0; i<xft->xftCount; i++)
	{
		if (xft->xftFrags[i].fExtent == 0)
		{
			break;
		};
	};
	
	// select last fragment
	i--;

	// don't worry about remainders because they cancel out when calculating blocksToFree.
	uint64_t blocksToFree = *outBlocksToFree;
	if (blocksToFree == 0xffffffffffffffff)
	{
		size_t blocksBefore = inode->inoSize / gxino->gxfs->cis.cisBlockSize;
		size_t blocksAfter = (inode->inoSize-shrinkBy) / gxino->gxfs->cis.cisBlockSize;
		blocksToFree = blocksBefore-blocksAfter;
	};
	
	while (blocksToFree--)
	{
		xft->xftFrags[i].fExtent--;
		uint64_t block = xft->xftFrags[i].fBlock + xft->xftFrags[i].fExtent;
		GXFreeBlock(gxino->gxfs, block);
		if (xft->xftFrags[i].fExtent == 0)
		{
			if (i == 0)
			{
				GXFreeBlock(gxino->gxfs, xftBlock);
				if (prevXFT == 0)
				{
					*xftOut = 0;
					*repeat = 0;
					*outBlocksToFree = blocksToFree;
				}
				else
				{
					*repeat = 1;
					*outBlocksToFree = blocksToFree;
					*xftOut = prevXFT;
					
					uint64_t sectionContainingBlock = prevXFT / gxfs->cis.cisBlocksPerSection;
					uint64_t blockIndexInSection = prevXFT % gxfs->cis.cisBlocksPerSection;
					uint64_t offsetToBlock = gxfs->sections[sectionContainingBlock].sdOffTabBlocks
								+ blockIndexInSection * gxfs->cis.cisBlockSize;
					gxfs->fp->seek(gxfs->fp, offsetToBlock, SEEK_SET);
					vfsRead(gxfs->fp, xft, gxfs->cis.cisBlockSize);
					
					xft->xftNext = 0;
					
					gxfs->fp->seek(gxfs->fp, offsetToBlock, SEEK_SET);
					vfsWrite(gxfs->fp, xft, gxfs->cis.cisBlockSize);
				};
				break;
			};
			
			i--;
		};
	};
	
	kfree(xft);
};

void GXShrinkInode(GXInode *gxino, size_t shrinkBy, gxfsInode *inode)
{
	//gxfsFragment frags[16];
	//gxino->gxfs->fp->seek(gxino->gxfs->fp, gxino->offset+39, SEEK_SET);
	//vfsRead(gxino->gxfs->fp, frags, sizeof(gxfsFragment)*16);
	
	uint64_t blocksToFree = 0xffffffffffffffff;
	if (inode->inoExFrag != 0)
	{
		int repeat;
		uint64_t xftBlock = inode->inoExFrag;
		do
		{
			GXShrinkXFT(gxino, shrinkBy, inode, &xftBlock, &repeat, &blocksToFree);
		} while (repeat);
		inode->inoExFrag = xftBlock;
	};
	
	gxfsFragment *frags = inode->inoFrags;

	int i;
	for (i=0; i<14; i++)
	{
		if (frags[i].fExtent == 0) break;
	};
	i--;

	// don't worry about remainders because they cancel out when calculating blocksToFree.
	if (blocksToFree == 0xffffffffffffffff)
	{
		size_t blocksBefore = inode->inoSize / gxino->gxfs->cis.cisBlockSize;
		size_t blocksAfter = (inode->inoSize-shrinkBy) / gxino->gxfs->cis.cisBlockSize;
		blocksToFree = blocksBefore-blocksAfter;
	};

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

static void GXFreeXFT(GXFileSystem *gxfs, uint64_t xftBlock)
{
	gxfsXFT *xft = (gxfsXFT*) kmalloc(gxfs->cis.cisBlockSize);
	
	while (xftBlock != 0)
	{
		uint64_t sectionContainingBlock = xftBlock / gxfs->cis.cisBlocksPerSection;
		uint64_t blockIndexInSection = xftBlock % gxfs->cis.cisBlocksPerSection;
		uint64_t offsetToBlock = gxfs->sections[sectionContainingBlock].sdOffTabBlocks + blockIndexInSection * gxfs->cis.cisBlockSize;
		gxfs->fp->seek(gxfs->fp, offsetToBlock, SEEK_SET);
		vfsRead(gxfs->fp, xft, gxfs->cis.cisBlockSize);
		
		uint16_t numFragsExpected = (gxfs->cis.cisBlockSize-10)/20;
		if (xft->xftCount > numFragsExpected)
		{
			// return error on bad XFTs.
			kprintf_debug("[GXFS] bad XFT: xftCount is %d, max possible is %d\n", (int)xft->xftCount, (int)numFragsExpected);
			break;
		};
		
		uint16_t i;
		for (i=0; i<xft->xftCount; i++)
		{
			while (xft->xftFrags[i].fExtent != 0)
			{
				GXFreeBlock(gxfs, xft->xftFrags[i].fBlock);
				xft->xftFrags[i].fBlock++;
				xft->xftFrags[i].fExtent--;
			};
		};
		
		// next
		GXFreeBlock(gxfs, xftBlock);
		xftBlock = xft->xftNext;
	};
	
	kfree(xft);
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
		gxfsFragment *frags = inode.inoFrags;

		if ((inode.inoMode & 0xF000) != 0x5000)
		{
			int i;
			for (i=0; i<14; i++)
			{
				while (frags[i].fExtent != 0)
				{
					GXFreeBlock(gxino->gxfs, frags[i].fBlock);
					frags[i].fBlock++;
					frags[i].fExtent--;
				};
			};
			
			GXFreeXFT(gxino->gxfs, inode.inoExFrag);
		};

		uint64_t section = (gxino->ino-1) / gxino->gxfs->cis.cisInoPerSection;
		uint64_t index = (gxino->ino-1) % gxino->gxfs->cis.cisInoPerSection;
		uint64_t offset = gxino->gxfs->sections[section].sdOffMapIno + (index/8);
		uint8_t mask = (1 << (index%8));

		gxino->gxfs->fp->seek(gxino->gxfs->fp, offset, SEEK_SET);
		uint8_t byte;
		vfsRead(gxino->gxfs->fp, &byte, 1);
		byte &= ~mask;
		gxino->gxfs->fp->seek(gxino->gxfs->fp, -1, SEEK_CUR);
		vfsWrite(gxino->gxfs->fp, &byte, 1);
		
		gxino->gxfs->usedInodes--;
	};
};
