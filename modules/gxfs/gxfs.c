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

#include <glidix/util/common.h>
#include <glidix/module/module.h>
#include <glidix/fs/vfs.h>
#include <glidix/fs/fsdriver.h>
#include <glidix/display/console.h>
#include <glidix/util/time.h>
#include <glidix/util/memory.h>
#include <glidix/util/string.h>
#include <glidix/util/errno.h>
#include <glidix/thread/sched.h>

#include "gxfs.h"

/* features supported by this driver */
#define	GXFS_SUPPORTED_FEATURES			(GXFS_FEATURE_BASE)

static int checkSuperblockHeader(GXFS_SuperblockHeader *sbh)
{
	uint64_t *scan = (uint64_t*) sbh;
	size_t count = 9;		/* 9 quadwords before the sbhChecksum field */
	uint64_t state = 0xF00D1234BEEFCAFEUL;
	
	while (count--)
	{
		state = (state << 1) ^ (*scan++);
	};
	
	return ((*scan) == state);
};

static void gxfsFlushSuperblock(GXFS *gxfs)
{
	vfsPWrite(gxfs->fp, &gxfs->sbb, sizeof(GXFS_SuperblockBody), GXFS_SBB_OFFSET);
};

static void gxfsUnmount(FileSystem *fs)
{
	GXFS *gxfs = (GXFS*) fs->fsdata;
	if ((gxfs->flags & MNT_RDONLY) == 0)
	{
		gxfs->sbb.sbbRuntimeFlags = 0;
		gxfsFlushSuperblock(gxfs);
	};
	vfsClose(gxfs->fp);
	kfree(gxfs);
};

static int gxfsReadBlock(GXFS *gxfs, uint64_t blockno, void *buffer)
{
	uint64_t off = 0x200000 + (blockno << 12);
	ssize_t size = vfsPRead(gxfs->fp, buffer, 4096, off);
	if (size != 4096)
	{
		kprintf("gxfs: block read failure: size=%ld, errno=%d\n", size, ERRNO);
		return -1;
	}
	else
	{
		return 0;
	};
};

static int gxfsWriteBlock(GXFS *gxfs, uint64_t blockno, const void *buffer)
{
	uint64_t off = 0x200000 + (blockno << 12);
	if (vfsPWrite(gxfs->fp, buffer, 4096, off) != 4096)
	{
		return -1;
	}
	else
	{
		return 0;
	};
};

static uint64_t gxfsAllocBlock(FileSystem *fs)
{
	GXFS *gxfs = (GXFS*) fs->fsdata;
	
	semWait(&gxfs->lock);
	if (gxfs->sbb.sbbFreeHead == 0)
	{
		if (gxfs->sbb.sbbUsedBlocks == gxfs->sbb.sbbTotalBlocks)
		{
			semSignal(&gxfs->lock);
			return 0;
		}
		else
		{
			uint64_t result = gxfs->sbb.sbbUsedBlocks++;
			__sync_fetch_and_add(&fs->freeBlocks, -1);
			__sync_fetch_and_add(&fs->freeInodes, -1);
			gxfsFlushSuperblock(gxfs);
			semSignal(&gxfs->lock);
			return result;
		};
	}
	else
	{
		char blockbuf[4096];
		uint64_t result = gxfs->sbb.sbbFreeHead;
		if (gxfsReadBlock(gxfs, result, blockbuf) != 0)
		{
			semSignal(&gxfs->lock);
			return 0;
		};
		
		gxfs->sbb.sbbFreeHead = *((uint64_t*)blockbuf);
		gxfs->sbb.sbbUsedBlocks++;
		__sync_fetch_and_add(&fs->freeBlocks, -1);
		__sync_fetch_and_add(&fs->freeInodes, -1);
		gxfsFlushSuperblock(gxfs);
		semSignal(&gxfs->lock);
		return result;
	};
};

static void gxfsFreeBlock(FileSystem *fs, uint64_t block)
{
	GXFS *gxfs = (GXFS*) fs->fsdata;
	
	semWait(&gxfs->lock);
	char blockbuf[4096];
	*((uint64_t*)blockbuf) = gxfs->sbb.sbbFreeHead;
	gxfsWriteBlock(gxfs, block, blockbuf);
	gxfs->sbb.sbbFreeHead = block;
	gxfsFlushSuperblock(gxfs);
	semSignal(&gxfs->lock);
	
	__sync_fetch_and_add(&fs->freeBlocks, 1);
	__sync_fetch_and_add(&fs->freeInodes, 1);
};

static uint64_t gxfsAllocZeroBlock(FileSystem *fs)
{
	uint64_t block = gxfsAllocBlock(fs);
	if (block == 0) return 0;
	
	char buf[4096];
	memset(buf, 0, 4096);
	if (gxfsWriteBlock((GXFS*) fs->fsdata, block, buf) != 0)
	{
		gxfsFreeBlock(fs, block);
		return 0;
	};
	
	return block;
};

typedef struct
{
	/**
	 * The filesystem.
	 */
	FileSystem *fs;
	
	/**
	 * Scanner of original block indexes.
	 */
	uint64_t *blockScanner;
	
	/**
	 * Output block list.
	 */
	uint64_t *outBlocks;
	size_t numOutBlocks;
	
	/**
	 * Current block buffer.
	 */
	char blockbuf[4096];
	size_t writepos;
	
	/**
	 * Current block number.
	 */
	uint64_t blockno;
} InodeWriter;

static void gxfsWriteInodeRecord(InodeWriter *writer, const void *buffer, size_t size)
{
	const char *scan = (const char*) buffer;
	while (size)
	{
		if (writer->writepos == 4096)
		{
			uint64_t nextblock;
			if (*writer->blockScanner != 0)
			{
				nextblock = *writer->blockScanner++;
			}
			else
			{
				nextblock = gxfsAllocBlock(writer->fs);
				assert(nextblock != 0);
			};
			
			writer->outBlocks = (uint64_t*) krealloc(writer->outBlocks, 8 * (writer->numOutBlocks+1));
			writer->outBlocks[writer->numOutBlocks++] = nextblock;
			
			*((uint64_t*)writer->blockbuf) = nextblock;
			gxfsWriteBlock((GXFS*) writer->fs->fsdata, writer->blockno, writer->blockbuf);
			memset(writer->blockbuf, 0, 4096);
			writer->writepos = 8;
			writer->blockno = nextblock;
		};
		
		size_t writeNow = size;
		size_t maxWrite = 4096 - writer->writepos;
		if (writeNow > maxWrite) writeNow = maxWrite;
		
		memcpy(&writer->blockbuf[writer->writepos], scan, writeNow);
		
		scan += writeNow;
		writer->writepos += writeNow;
		size -= writeNow;
	};
};

static int gxfsFlushInode(Inode *inode)
{
	if (inode->fs->flags & VFS_ST_RDONLY) return 0;
	
	GXFS_Inode *idata = (GXFS_Inode*) inode->fsdata;
	InodeWriter writer;
	writer.fs = inode->fs;
	writer.blockScanner = idata->blocks + 1;	// ignore the main inode block
	writer.outBlocks = (uint64_t*) kmalloc(8);
	writer.outBlocks[0] = inode->ino;
	writer.numOutBlocks = 1;
	memset(writer.blockbuf, 0, 4096);
	writer.writepos = 8;
	writer.blockno = inode->ino;
	
	// ATTR record
	GXFS_AttrRecord ar;
	ar.arType = GXFS_RT("ATTR");
	ar.arRecordSize = sizeof(GXFS_AttrRecord);
	ar.arLinks = inode->links;
	ar.arFlags = inode->mode;
	if (inode->ft != NULL)
	{
		if (inode->ft->flags & FT_FIXED_SIZE)
		{
			ar.arFlags |= GXFS_INODE_FIXED_SIZE;
		};
	};
	ar.arOwner = inode->uid;
	ar.arGroup = inode->gid;
	ar.arSize = 0;
	if (inode->ft != NULL) ar.arSize = inode->ft->size;
	ar.arATime = inode->atime;
	ar.arMTime = inode->mtime;
	ar.arCTime = inode->ctime;
	ar.arBTime = inode->btime;
	ar.arANano = inode->anano;
	ar.arMNano = inode->mnano;
	ar.arCNano = inode->cnano;
	ar.arBNano = inode->bnano;
	ar.arIXPerm = inode->ixperm;
	ar.arOXPerm = inode->oxperm;
	ar.arDXPerm = inode->dxperm;
	gxfsWriteInodeRecord(&writer, &ar, sizeof(GXFS_AttrRecord));
	
	// _ACL record
	GXFS_AclRecord cr;
	cr.crType = GXFS_RT("_ACL");
	cr.crSize = sizeof(GXFS_AclRecord);
	memcpy(cr.acl, inode->acl, sizeof(AccessControlEntry)*VFS_ACL_SIZE);
	gxfsWriteInodeRecord(&writer, &cr, sizeof(GXFS_AclRecord));
	
	// LINK record
	if (inode->target != NULL)
	{
		size_t reclen = (8 + strlen(inode->target) + 7) & ~7;
		char *buffer = (char*) kmalloc(reclen);
		memset(buffer, 0, reclen);
		
		GXFS_RecordHeader *rh = (GXFS_RecordHeader*) buffer;
		rh->rhType = GXFS_RT("LINK");
		rh->rhSize = reclen;
		
		memcpy(buffer + 8, inode->target, strlen(inode->target));
		gxfsWriteInodeRecord(&writer, buffer, reclen);
		kfree(buffer);
	};
	
	// DENT records
	Dentry *dent;
	for (dent=inode->dents; dent!=NULL; dent=dent->next)
	{
		if (dent->ino != 0 && (dent->flags & VFS_DENTRY_TEMP) == 0)
		{
			size_t recsize = (sizeof(GXFS_DentRecord) + strlen(dent->name) + 7) & ~7;
			GXFS_DentRecord *dr = (GXFS_DentRecord*) kmalloc(recsize);
			memset(dr, 0, recsize);
		
			dr->drType = GXFS_RT("DENT");
			dr->drRecordSize = recsize;
			dr->drInode = dent->ino;
			dr->drInoType = 0xFF;		// "unknown"
			memcpy(dr->drName, dent->name, strlen(dent->name));
			
			gxfsWriteInodeRecord(&writer, dr, recsize);
			kfree(dr);
		};
	};
	
	// TREE record if needed
	if (inode->ft != NULL)
	{
		GXFS_Tree *tree = (GXFS_Tree*) inode->ft->data;
		GXFS_TreeRecord tr;
		tr.trType = GXFS_RT("TREE");
		tr.trSize = sizeof(GXFS_TreeRecord);
		tr.trDepth = tree->depth;
		tr.trHead = tree->head;
		gxfsWriteInodeRecord(&writer, &tr, tr.trSize);
	};
	
	// free remaining unused blocks
	while (*writer.blockScanner != 0)
	{
		gxfsFreeBlock(inode->fs, *writer.blockScanner++);
	};
	
	// write the final block and terminate
	gxfsWriteBlock((GXFS*) inode->fs->fsdata, writer.blockno, writer.blockbuf);
	writer.outBlocks = (uint64_t*) krealloc(writer.outBlocks, 8 * (writer.numOutBlocks+1));
	writer.outBlocks[writer.numOutBlocks] = 0;
	kfree(idata->blocks);
	idata->blocks = writer.outBlocks;
	
	return 0;
};

static void gxfsFreeInode(Inode *inode)
{
	GXFS_Inode *idata = (GXFS_Inode*) inode->fsdata;
	kfree(idata->blocks);
	kfree(idata);
};

static void gxfsDeleteTreeRecur(FileSystem *fs, uint64_t depth, uint64_t head)
{
	if (head == 0) return;
	
	if (depth == 0)
	{
		gxfsFreeBlock(fs, head);
	}
	else
	{
		uint64_t table[512];
		if (gxfsReadBlock((GXFS*) fs->fsdata, head, table) != 0)
		{
			enableDebugTerm();
			kprintf("gxfs: failed to read a tree node %lu: corruption likely\n", head);
			return;
		};
		
		int i;
		for (i=0; i<512; i++)
		{
			gxfsDeleteTreeRecur(fs, depth-1, table[i]);
		};
	};
};

static void gxfsDropInode(Inode *inode)
{
	GXFS_Inode *idata = (GXFS_Inode*) inode->fsdata;
	uint64_t *iter;
	for (iter=idata->blocks; *iter!=0; iter++)
	{
		gxfsFreeBlock(inode->fs, *iter);
		*iter = 0;
	};
	
	if (inode->ft != NULL)
	{
		GXFS_Tree *data = (GXFS_Tree*) inode->ft->data;
		gxfsDeleteTreeRecur(inode->fs, data->depth, data->head);
	};
};

static void gxfsSetInodeCallbacks(Inode *inode)
{
	inode->flush = gxfsFlushInode;
	inode->free = gxfsFreeInode;
	inode->drop = gxfsDropInode;
};

static int gxfsTreeLoad(FileTree *ft, off_t pos, void *buffer)
{
	GXFS_Tree *data = (GXFS_Tree*) ft->data;
	
	uint64_t sizeLimit = (1UL << 57) - 1;
	if (pos > sizeLimit)
	{
		return -1;
	};
	
	while (pos >= (1UL << (12 + 9 * data->depth)))
	{
		// we must increase the depth
		uint64_t indirect = gxfsAllocBlock(data->fs);
		if (indirect == 0) return -1;
		
		uint64_t table[512];
		memset(table, 0, 4096);
		table[0] = data->head;
		
		if (gxfsWriteBlock((GXFS*) data->fs->fsdata, indirect, table) != 0)
		{
			gxfsFreeBlock(data->fs, indirect);
			return -1;
		};
		
		data->head = indirect;
		data->depth++;
	};
	
	uint64_t lvl[5];
	lvl[4] = (pos >> 12) & 0x1FF;
	lvl[3] = (pos >> 21) & 0x1FF;
	lvl[2] = (pos >> 30) & 0x1FF;
	lvl[1] = (pos >> 39) & 0x1FF;
	lvl[0] = (pos >> 48) & 0x1FF;
	
	// get to the data block
	uint64_t datablock = data->head;
	int i;
	for (i=(5-data->depth); i<5; i++)
	{
		uint64_t table[512];
		if (gxfsReadBlock((GXFS*) data->fs->fsdata, datablock, table) != 0)
		{
			return -1;
		};
		
		if (table[lvl[i]] == 0)
		{
			uint64_t newblock = gxfsAllocZeroBlock(data->fs);
			if (newblock == 0)
			{
				return -1;
			};
			
			table[lvl[i]] = newblock;
			if (gxfsWriteBlock((GXFS*) data->fs->fsdata, datablock, table) != 0)
			{
				gxfsFreeBlock(data->fs, newblock);
				return -1;
			};

			datablock = newblock;
		}
		else
		{
			datablock = table[lvl[i]];
		};
	};
	
	// finally, load the data
	if (gxfsReadBlock((GXFS*) data->fs->fsdata, datablock, buffer) != 0)
	{
		return -1;
	};
	
	return 0;
};

static int gxfsTreeFlush(FileTree *ft, off_t pos, const void *buffer)
{
	GXFS_Tree *data = (GXFS_Tree*) ft->data;
	if (data->fs->flags & VFS_ST_RDONLY)
	{
		return 0;
	};
	
	uint64_t sizeLimit = (1UL << 57) - 1;
	if (pos > sizeLimit)
	{
		panic("tree inconsistent: offset larger than 57 bits");
	};
	
	if (pos >= (1UL << (12 + 9 * data->depth)))
	{
		panic("tree inconsistent: offset out of bounds");
	};
	
	uint64_t lvl[5];
	lvl[4] = (pos >> 12) & 0x1FF;
	lvl[3] = (pos >> 21) & 0x1FF;
	lvl[2] = (pos >> 30) & 0x1FF;
	lvl[1] = (pos >> 39) & 0x1FF;
	lvl[0] = (pos >> 48) & 0x1FF;
	
	// get to the data block
	uint64_t datablock = data->head;
	int i;
	for (i=(5-data->depth); i<5; i++)
	{
		uint64_t table[512];
		if (gxfsReadBlock((GXFS*) data->fs->fsdata, datablock, table) != 0)
		{
			return -1;
		};
		
		if (table[lvl[i]] == 0)
		{
			panic("tree inconsistent: flushing non-allocated block");
		}
		else
		{
			datablock = table[lvl[i]];
		};
	};
	
	// write the data
	if (gxfsWriteBlock((GXFS*) data->fs->fsdata, datablock, buffer) != 0)
	{
		return -1;
	};
	
	return 0;
};	

static void gxfsTruncateRecur(FileSystem *fs, uint64_t depth, uint64_t head, uint64_t base, uint64_t maxpage)
{
	if (depth == 0) return;			// cannot free if there is only a single block in the tree
	
	uint64_t table[512];
	int dirty = 0;
	if (gxfsReadBlock((GXFS*) fs->fsdata, head, table) != 0)
	{
		kprintf("gxfs: WARNING: failed to read a tree block; filesystem probably corrupt\n");
		return;
	};
	
	uint64_t i;
	for (i=0; i<512; i++)
	{
		if (table[i] != 0)
		{
			uint64_t dest = (base << 9) | i;
			if (depth == 1)
			{
				if (dest >= maxpage)
				{
					gxfsFreeBlock(fs, table[i]);
					table[i] = 0;
					dirty = 1;
				};
			}
			else
			{
				gxfsTruncateRecur(fs, depth-1, table[i], dest, maxpage);
			};
		};
	};
	
	if (dirty)
	{
		if (gxfsWriteBlock((GXFS*) fs->fsdata, head, table) != 0)
		{
			kprintf("gxfs: WARNING: failed to write a tree block; filesystem probably corrupt\n");
			return;
		};
	};
};

static void gxfsTreeUpdate(FileTree *ft)
{
	GXFS_Tree *data = (GXFS_Tree*) ft->data;
	
	// free all blocks that are further than the new file size
	gxfsTruncateRecur(data->fs, data->depth, data->head, 0, (ft->size >> 12) + (!!(ft->size & 0xFFF)));
};

static FileTree* gxfsTree(FileSystem *fs, uint64_t depth, uint64_t head, size_t size, uint32_t iflags)
{
	GXFS_Tree *data = NEW(GXFS_Tree);
	data->fs = fs;
	data->depth = depth;
	data->head = head;
	
	int treeFlags = 0;
	if (fs->flags & VFS_ST_RDONLY)
	{
		treeFlags = FT_READONLY;
	};
	
	if (iflags & GXFS_INODE_FIXED_SIZE)
	{
		treeFlags |= FT_FIXED_SIZE;
	};
	
	FileTree *ft = ftCreate(treeFlags);
	ft->size = size;
	ft->data = data;
	ft->load = gxfsTreeLoad;
	ft->flush = gxfsTreeFlush;
	ft->update = gxfsTreeUpdate;
	ftDown(ft);
	
	return ft;
};

static int gxfsRegInode(FileSystem *fs, Inode *inode)
{
	uint64_t num = gxfsAllocBlock(fs);
	if (num == 0)
	{
		ERRNO = ENOSPC;
		return -1;
	};
	
	inode->ino = num;
	
	if ((inode->mode & VFS_MODE_TYPEMASK) == 0)
	{
		// regular file needs a tree
		uint64_t head = gxfsAllocZeroBlock(fs);
		if (head == 0)
		{
			gxfsFreeBlock(fs, num);
			ERRNO = ENOSPC;
			return -1;
		};
		
		inode->ft = gxfsTree(fs, 0, head, 0, 0);
	};
	
	GXFS_Inode *idata = NEW(GXFS_Inode);
	idata->blocks = (uint64_t*) kmalloc(16);
	idata->blocks[0] = num;
	idata->blocks[1] = 0;
	
	inode->fsdata = idata;
	gxfsSetInodeCallbacks(inode);
	return 0;
};

static int gxfsLoadInode(FileSystem *fs, Inode *inode)
{
	char blockbuf[4096];
	size_t readpos;
	
	GXFS *gxfs = (GXFS*) fs->fsdata;
	
	if (gxfsReadBlock(gxfs, inode->ino, blockbuf) != 0)
	{
		kprintf("gxfs: cannot read initial block of inode %lu\n", inode->ino);
		return -1;
	};
	
	GXFS_InodeHeader *ih = (GXFS_InodeHeader*) blockbuf;
	readpos = 8;		// after ihNext
	
	int foundAttr = 0;
	size_t fileSize = 0;
	uint32_t fileFlags = 0;
	
	uint64_t *blocks = (uint64_t*) kmalloc(8);
	size_t numBlocks = 1;
	blocks[0] = inode->ino;
	
	while (1)
	{
		if (readpos == 4096)
		{
			if (ih->ihNext == 0)
			{
				break;
			}
			else
			{
				blocks = (uint64_t*) krealloc(blocks, 8 * (numBlocks+1));
				blocks[numBlocks++] = ih->ihNext;
				
				if (gxfsReadBlock(gxfs, ih->ihNext, blockbuf) != 0)
				{
					kprintf("gxfs: cannot read follow-up block %lu of inode %lu\n", ih->ihNext, inode->ino);
					kfree(blocks);
					return -1;
				};
				
				readpos = 8;
			};
		};
		
		GXFS_RecordHeader *rh = (GXFS_RecordHeader*) &blockbuf[readpos];
		if ((rh->rhType & 0xFF) == 0)
		{
			break;
		};
		
		if ((rh->rhSize < 8) || ((rh->rhSize & 7) != 0))
		{
			kprintf("gxfs: encountered record with invalid size\n");
			kfree(blocks);
			return -1;
		};
		
		void *buffer = kmalloc(rh->rhSize+1);
		if (buffer == NULL)
		{
			kprintf("gxfs: out of memory\n");
			kfree(blocks);
			return -1;
		};
		((char*)buffer)[rh->rhSize] = 0;	// to terminate strings at end of some records such as DENT or LINK
		
		char *put = (char*) buffer;
		size_t sizeLeft = rh->rhSize;
		
		while (sizeLeft)
		{
			if (readpos == 4096)
			{
				if (ih->ihNext == 0)
				{
					kfree(buffer);
					kprintf("gxfs: unterminated inode record\n");
					kfree(blocks);
					return -1;
				}
				else
				{
					blocks = (uint64_t*) krealloc(blocks, 8 * (numBlocks+1));
					blocks[numBlocks++] = ih->ihNext;

					if (gxfsReadBlock(gxfs, ih->ihNext, blockbuf) != 0)
					{
						kfree(buffer);
						kprintf("gxfs: cannot read follow-up block %lu of inode %lu\n", ih->ihNext, inode->ino);
						kfree(blocks);
						return -1;
					};
				
					readpos = 8;
				};
			};
			
			size_t readNow = sizeLeft;
			size_t maxRead = 4096 - readpos;
			if (readNow > maxRead) readNow = maxRead;
			
			memcpy(put, &blockbuf[readpos], readNow);
			
			put += readNow;
			readpos += readNow;
			sizeLeft -= readNow;
		};
		
		// process the record as necessary
		if (rh->rhType == GXFS_RT("ATTR"))
		{
			if (foundAttr)
			{
				kfree(buffer);
				kfree(blocks);
				kprintf("gxfs: encountered multiple ATTR records\n");
				return -1;
			};
			
			foundAttr = 1;
			if (rh->rhSize != sizeof(GXFS_AttrRecord))
			{
				kfree(buffer);
				kprintf("gxfs: encountered invalid ATTR record (bad size)\n");
				kfree(blocks);
				return -1;
			};
			
			GXFS_AttrRecord *ar = (GXFS_AttrRecord*) buffer;
			inode->links = ar->arLinks;
			inode->mode = ar->arFlags & 0xFFFF;
			inode->uid = ar->arOwner;
			inode->gid = ar->arGroup;
			inode->atime = ar->arATime;
			inode->mtime = ar->arMTime;
			inode->ctime = ar->arCTime;
			inode->btime = ar->arBTime;
			inode->anano = ar->arANano;
			inode->mnano = ar->arMNano;
			inode->cnano = ar->arCNano;
			inode->bnano = ar->arBNano;
			inode->ixperm = ar->arIXPerm;
			inode->oxperm = ar->arOXPerm;
			inode->dxperm = ar->arDXPerm;
			fileSize = ar->arSize;
			fileFlags = ar->arFlags;
		}
		else if (rh->rhType == GXFS_RT("DENT"))
		{
			if (!foundAttr)
			{
				kfree(buffer);
				kfree(blocks);
				kprintf("gxfs: encountered a DENT record before an ATTR record\n");
				return -1;
			};
			
			GXFS_DentRecord *dr = (GXFS_DentRecord*) buffer;
			vfsAppendDentry(inode, dr->drName, dr->drInode);
		}
		else if (rh->rhType == GXFS_RT("TREE"))
		{
			if (!foundAttr)
			{
				kfree(buffer);
				kfree(blocks);
				kprintf("gxfs: encountered a TREE record before an ATTR record\n");
				return -1;
			};
			
			GXFS_TreeRecord *tr = (GXFS_TreeRecord*) buffer;
			if (tr->trSize != sizeof(GXFS_TreeRecord))
			{
				kfree(buffer);
				kfree(blocks);
				kprintf("gxfs: encountered a TREE record with an incorrect size\n");
				return -1;
			};
			
			inode->ft = gxfsTree(fs, tr->trDepth, tr->trHead, fileSize, fileFlags);
		}
		else if (rh->rhType == GXFS_RT("LINK"))
		{
			if (!foundAttr)
			{
				kfree(buffer);
				kfree(blocks);
				kprintf("gxfs: encountered a LINK record before an ATTR record\n");
				return -1;
			};
			
			inode->target = strdup(buffer + 8);
		}
		else if (rh->rhType == GXFS_RT("_ACL"))
		{
			if (!foundAttr)
			{
				kfree(buffer);
				kfree(blocks);
				kprintf("gxfs: encountered a _ACL record before an ATTR record\n");
				return -1;
			};
			
			GXFS_AclRecord *cr = (GXFS_AclRecord*) buffer;
			if (cr->crSize != sizeof(GXFS_AclRecord))
			{
				kfree(buffer);
				kfree(blocks);
				kprintf("gxfs: encountered a _ACL record with an incorrect size\n");
				return -1;
			};
			
			memcpy(inode->acl, cr->acl, sizeof(AccessControlEntry)*VFS_ACL_SIZE);
		};
		
		// next!
		kfree(buffer);
	};
	
	if (!foundAttr)
	{
		kprintf("gxfs: inode without ATTR record\n");
		kfree(blocks);
		return -1;
	};
	
	GXFS_Inode *idata = NEW(GXFS_Inode);
	blocks = (uint64_t*) krealloc(blocks, 8 * (numBlocks+1));
	blocks[numBlocks] = 0;
	idata->blocks = blocks;
	inode->fsdata = idata;
	gxfsSetInodeCallbacks(inode);
	return 0;
};

static Inode* gxfsOpenRoot(const char *image, int flags, const void *options, size_t optlen, int *error)
{
	int oflags;
	if (flags & MNT_RDONLY)
	{
		oflags = O_RDONLY;
	}
	else
	{
		oflags = O_RDWR;
	};
	
	File *fp = vfsOpen(VFS_NULL_IREF, image, oflags, 0, error);
	if (fp == NULL)
	{
		return NULL;
	};
	
	GXFS_SuperblockHeader sbh;
	if (vfsPRead(fp, &sbh, sizeof(GXFS_SuperblockHeader), 0x200000) != sizeof(GXFS_SuperblockHeader))
	{
		kprintf("gxfs: failed to read the superblock header\n");
		vfsClose(fp);
		*error = EINVAL;
		return NULL;
	};
	
	if (sbh.sbhMagic != GXFS_SBH_MAGIC)
	{
		kprintf("gxfs: superblock header has invalid signature\n");
		vfsClose(fp);
		*error = EINVAL;
		return NULL;
	};
	
	if (!checkSuperblockHeader(&sbh))
	{
		kprintf("gxfs: superblock checksum invalid\n");
		vfsClose(fp);
		*error = EINVAL;
		return NULL;
	};
	
	if ((sbh.sbhWriteFeatures & sbh.sbhReadFeatures) != sbh.sbhReadFeatures)
	{
		kprintf("gxfs: superblock header invalid: read features are not a subset of write features\n");
		vfsClose(fp);
		*error = EINVAL;
		return NULL;
	};
	
	if ((sbh.sbhWriteFeatures & sbh.sbhOptionalFeatures) != 0)
	{
		kprintf("gxfs: superblock header invalid: optional features are not disjoint from requrired features\n");
		vfsClose(fp);
		*error = EINVAL;
		return NULL;
	};
	
	uint64_t requiredFeatures;
	if (flags & MNT_RDONLY)
	{
		requiredFeatures = sbh.sbhReadFeatures;
	}
	else
	{
		requiredFeatures = sbh.sbhWriteFeatures;
	};
	
	if ((requiredFeatures & GXFS_SUPPORTED_FEATURES) != GXFS_SUPPORTED_FEATURES)
	{
		kprintf("gxfs: this filesystem uses unsupported features; try read-only\n");
		vfsClose(fp);
		*error = EINVAL;
		return NULL;
	};
	
	GXFS *gxfs = NEW(GXFS);
	if (gxfs == NULL)
	{
		vfsClose(fp);
		*error = ENOMEM;
		return NULL;
	};
	
	gxfs->fp = fp;
	gxfs->flags = flags;
	semInit(&gxfs->lock);
	
	if (vfsPRead(fp, &gxfs->sbb, sizeof(GXFS_SuperblockBody), GXFS_SBB_OFFSET) != sizeof(GXFS_SuperblockBody))
	{
		vfsClose(fp);
		kfree(gxfs);
		kprintf("gxfs: failed to read the SBB\n");
		*error = EIO;
		return NULL;
	};
	
	if (gxfs->sbb.sbbRuntimeFlags & GXFS_RF_DIRTY)
	{
		kprintf("gxfs: WARNING: filesystem on `%s' is dirty!\n", image);
	};
	
	if ((flags & MNT_RDONLY) == 0)
	{
		// read-write mode, so make the filesystem dirty and update mount time
		gxfs->sbb.sbbLastMountTime = time();
		gxfs->sbb.sbbRuntimeFlags |= GXFS_RF_DIRTY;
		gxfsFlushSuperblock(gxfs);
		vfsFlush(fp->iref.inode);
	};

	// set up the filesystem
	FileSystem *fs = vfsCreateFileSystem("gxfs");
	fs->fsdata = gxfs;
	fs->unmount = gxfsUnmount;
	fs->loadInode = gxfsLoadInode;
	fs->regInode = gxfsRegInode;
	
	fs->blockSize = 4096;
	fs->blocks = gxfs->sbb.sbbTotalBlocks;
	fs->freeBlocks = gxfs->sbb.sbbTotalBlocks - gxfs->sbb.sbbUsedBlocks;
	fs->inodes = gxfs->sbb.sbbTotalBlocks;
	fs->freeInodes = gxfs->sbb.sbbTotalBlocks - gxfs->sbb.sbbUsedBlocks;
	memcpy(fs->bootid, sbh.sbhBootID, 16);
	
	Inode *root = vfsCreateInode(NULL, VFS_MODE_DIRECTORY);
	root->fs = fs;
	fs->imap = root;
	root->ino = 2;
	
	if (gxfsLoadInode(fs, root) != 0)
	{
		vfsDownrefInode(root);
		kfree(fs);
		kfree(gxfs);
		vfsClose(fp);
		*error = EIO;
		return NULL;
	};
	
	return root;
};

static FSDriver gxfs = {
	.fsname = "gxfs",
	.openroot = gxfsOpenRoot,
};

MODULE_INIT()
{
	kprintf("gxfs: registering GXFS driver\n");
	registerFSDriver(&gxfs);
	return MODINIT_OK;
};

MODULE_FINI()
{
	kprintf("gxfs: removing GXFS driver\n");
	unregisterFSDriver(&gxfs);
	return 0;
};
