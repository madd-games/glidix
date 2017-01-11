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

#include <glidix/common.h>
#include <glidix/module.h>
#include <glidix/console.h>
#include <glidix/fsdriver.h>
#include <glidix/string.h>
#include <glidix/memory.h>
#include <glidix/vfs.h>

#include "gxfs.h"

uint64_t gxfsAllocBlock(GXFS *gxfs)
{
	semWait(&gxfs->semAlloc);
	
	if (gxfs->sb.sbFreeHead != 0)
	{
		uint64_t newBlock = gxfs->sb.sbFreeHead;
		uint64_t nextBlock;
		gxfs->fp->pread(gxfs->fp, &nextBlock, 8, 0x200000 + 512 * newBlock);
		gxfs->sb.sbFreeHead = nextBlock;
		
		gxfs->sb.sbUsedBlocks++;
		gxfs->fp->pwrite(gxfs->fp, &gxfs->sb, 512, 0x200000);
		semSignal(&gxfs->semAlloc);
		
		return newBlock;
	};
	
	if (gxfs->sb.sbUsedBlocks == gxfs->sb.sbTotalBlocks)
	{
		semSignal(&gxfs->semAlloc);
		return 0;
	};
	
	uint64_t block = gxfs->sb.sbUsedBlocks++;
	gxfs->fp->pwrite(gxfs->fp, &gxfs->sb, 512, 0x200000);
	semSignal(&gxfs->semAlloc);
	
	return block;
};

void gxfsFreeBlock(GXFS *gxfs, uint64_t block)
{
	if (block == 0)
	{
		stackTraceHere();
		panic("attempting to free the superblock!");
	};
	
	semWait(&gxfs->semAlloc);
	gxfs->fp->pwrite(gxfs->fp, &gxfs->sb.sbFreeHead, 8, 0x200000 + 512 * block);
	gxfs->sb.sbFreeHead = block;
	gxfs->sb.sbUsedBlocks--;
	gxfs->fp->pwrite(gxfs->fp, &gxfs->sb, 512, 0x200000);
	semSignal(&gxfs->semAlloc);
};

void gxfsZeroBlock(GXFS *gxfs, uint64_t block)
{
	char zeroes[512];
	memset(zeroes, 0, 512);
	
	gxfs->fp->pwrite(gxfs->fp, zeroes, 512, 0x200000 + (block << 9));
};
