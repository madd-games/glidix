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

#include <glidix/module/module.h>
#include <glidix/fs/fsdriver.h>
#include <glidix/fs/vfs.h>
#include <glidix/util/common.h>
#include <glidix/util/memory.h>
#include <glidix/util/string.h>
#include <glidix/display/console.h>
#include <glidix/util/errno.h>
#include <glidix/thread/sched.h>

#include "fatfs.h"

static void readClusterChain(Fatfs *fatfs, uint32_t cluster, uint32_t **clustersOut, size_t *countOut)
{
	uint32_t *clusters = NULL;
	size_t numClusters = 0;
	
	while (cluster != 0 && cluster < 0x0FFFFFF7)
	{
		size_t index = numClusters++;
		clusters = (uint32_t*) krealloc(clusters, 4 * numClusters);
		clusters[index] = cluster;
		
		if (vfsPRead(fatfs->fp, &cluster, 4, fatfs->fatpos + 4 * cluster) != 4)
		{
			break;
		};
		
		cluster &= 0x0FFFFFFF;
	};
	
	*clustersOut = clusters;
	*countOut = numClusters;
};

static uint32_t getFreeCluster(Fatfs *fatfs)
{
	uint32_t *track = (uint32_t*) kmalloc(2 * 1024 * 1024);
	size_t currentTrack = (size_t) -1;
	
	size_t i;
	for (i=2; i<fatfs->numClusters; i++)
	{
		size_t trackno = i / (2 * 1024 * 1024 / 4);
		size_t offset = i % (2 * 1024 * 1024 / 4);
		
		if (currentTrack != trackno)
		{
			currentTrack = trackno;
			if (vfsPRead(fatfs->fp, track, 2 * 1024 * 1024, fatfs->fatpos + currentTrack * 2 * 1024 * 1024) < 2 * 1024 * 1024)
			{
				break;
			};
		};
		
		if (track[offset] == 0)
		{
			kfree(track);
			return (uint32_t) i;
		};
	};
	
	kfree(track);
	return 0;
};

static int expandClusterChain(FATInodeTable *itab)
{
	// allocate a new cluster
	uint32_t newCluster = getFreeCluster(itab->fatfs);
	if (newCluster == 0)
	{
		return -1;
	};
	
	// zero it out
	char buffer[itab->fatfs->clusterSize];
	memset(buffer, 0, itab->fatfs->clusterSize);
	if (vfsPWrite(itab->fatfs->fp, buffer, itab->fatfs->clusterSize, itab->fatfs->clusterPos + itab->fatfs->clusterSize * (newCluster-2))
		!= itab->fatfs->clusterSize)
	{
		return -1;
	};
	
	// mark it as used
	uint32_t usedval = 0x0FFFFFFF;
	if (vfsPWrite(itab->fatfs->fp, &usedval, 4, itab->fatfs->fatpos + 4 * newCluster) != 4)
	{
		return -1;
	};
	
	// link the previous cluster
	uint32_t prevCluster = itab->clusters[itab->numClusters-1];
	if (vfsPWrite(itab->fatfs->fp, &newCluster, 4, itab->fatfs->fatpos + 4 * prevCluster) != 4)
	{
		return -1;
	};
	
	// append to list
	size_t index = itab->numClusters++;
	itab->clusters = (uint32_t*) krealloc(itab->clusters, 4 * itab->numClusters);
	itab->clusters[index] = newCluster;
	
	// increase count
	__sync_fetch_and_add(&itab->fatfs->fs->freeBlocks, -1);
	
	return 0;
};

static int shrinkClusterChain(FATInodeTable *itab)
{
	// last cluster
	uint32_t toFree = itab->clusters[--itab->numClusters];
	
	// free it
	uint32_t freeval = 0;
	if (vfsPWrite(itab->fatfs->fp, &freeval, 4, itab->fatfs->fatpos + 4 * toFree) != 4)
	{
		return -1;
	};
	
	// mark the previous one as last
	uint32_t prevCluster = itab->clusters[itab->numClusters-1];
	uint32_t endval = 0x0FFFFFFF;
	if (vfsPWrite(itab->fatfs->fp, &endval, 4, itab->fatfs->fatpos + 4 * prevCluster) != 4)
	{
		return -1;
	};

	// decrease count
	__sync_fetch_and_add(&itab->fatfs->fs->freeBlocks, 1);

	return 0;
};

static FATInodeTable* getInodeInfoByNo(Fatfs *fatfs, ino_t ino)
{
	FATInodeTable *itab;
	for (itab=fatfs->itab; itab!=NULL; itab=itab->next)
	{
		if (itab->ino == ino) return itab;
	};
	
	return NULL;
};

static FATInodeTable* getInodeInfoByCluster(Fatfs *fatfs, uint32_t cluster)
{
	FATInodeTable *itab;
	for (itab=fatfs->itab; itab!=NULL; itab=itab->next)
	{
		if (itab->clusters != NULL && itab->clusters[0] == cluster) return itab;
	};
	
	// create a new one if not found
	itab = NEW(FATInodeTable);
	memset(itab, 0, sizeof(FATInodeTable));
	itab->ino = __sync_fetch_and_add(&fatfs->nextIno, 1);
	readClusterChain(fatfs, cluster, &itab->clusters, &itab->numClusters);
	itab->fatfs = fatfs;
	
	itab->next = fatfs->itab;
	fatfs->itab = itab;
	
	return itab;
};

static int fatfsTreeLoad(FileTree *ft, off_t offset, void *buffer)
{
	char *put = (char*) buffer;
	size_t sizeLeft = 0x1000;
	
	FATInodeTable *itab = (FATInodeTable*) ft->data;
	semWait(&itab->fatfs->lock);
	
	while (sizeLeft)
	{
		size_t clusterIndex = offset / itab->fatfs->clusterSize;
		size_t clusterOffset = offset % itab->fatfs->clusterSize;
		
		while (itab->numClusters <= clusterIndex)
		{
			if (expandClusterChain(itab) != 0)
			{
				semSignal(&itab->fatfs->lock);
				return -1;
			};
		};
		
		off_t clusterPos = itab->fatfs->clusterPos + (itab->clusters[clusterIndex]-2) * itab->fatfs->clusterSize
					+ clusterOffset;
		size_t willRead = itab->fatfs->clusterSize - clusterOffset;
		if (willRead > sizeLeft) willRead = sizeLeft;
		
		if (vfsPRead(itab->fatfs->fp, put, willRead, clusterPos) != willRead) return -1;
		sizeLeft -= willRead;
		put += willRead;
		offset += willRead;
	};
	
	semSignal(&itab->fatfs->lock);
	return 0;
};

static int fatfsTreeFlush(FileTree *ft, off_t offset, const void *buffer)
{
	const char *scan = (const char*) buffer;
	size_t sizeLeft = 0x1000;
	
	FATInodeTable *itab = (FATInodeTable*) ft->data;
	semWait(&itab->fatfs->lock);
	
	while (sizeLeft)
	{
		size_t clusterIndex = offset / itab->fatfs->clusterSize;
		size_t clusterOffset = offset % itab->fatfs->clusterSize;
		
		assert(itab->numClusters > clusterIndex);
		
		off_t clusterPos = itab->fatfs->clusterPos + (itab->clusters[clusterIndex]-2) * itab->fatfs->clusterSize
					+ clusterOffset;
		size_t willWrite = itab->fatfs->clusterSize - clusterOffset;
		if (willWrite > sizeLeft) willWrite = sizeLeft;
		
		if (vfsPWrite(itab->fatfs->fp, scan, willWrite, clusterPos) != willWrite) return -1;
		sizeLeft -= willWrite;
		scan += willWrite;
		offset += willWrite;
	};
	
	semSignal(&itab->fatfs->lock);
	return 0;
};

static void fatfsTreeUpdate(FileTree *ft)
{
	FATInodeTable *itab = (FATInodeTable*) ft->data;
	semWait(&itab->fatfs->lock);
	
	size_t neededClusters = (ft->size + itab->fatfs->clusterSize - 1) / itab->fatfs->clusterSize;
	while (itab->numClusters > neededClusters)
	{
		if (itab->numClusters == 1)
		{
			// zero it out
			char buffer[itab->fatfs->clusterSize];
			memset(buffer, 0, itab->fatfs->clusterSize);
			vfsPWrite(itab->fatfs->fp, buffer, itab->fatfs->clusterSize,
				itab->fatfs->clusterPos + itab->fatfs->clusterSize * (itab->clusters[0]-2));
			break;
		};
		
		shrinkClusterChain(itab);
	};
	
	semSignal(&itab->fatfs->lock);
};

static void fatfsSetTreeOps(FileTree *ft)
{
	ft->load = fatfsTreeLoad;
	ft->flush = fatfsTreeFlush;
	ft->update = fatfsTreeUpdate;
};

static int putdent(FATInodeTable *itab, size_t pos, FATDent *dent)
{
	size_t clusterIndex = pos / itab->fatfs->clusterSize;
	size_t offset = pos % itab->fatfs->clusterSize;
	
	while (clusterIndex >= itab->numClusters)
	{
		if (expandClusterChain(itab) != 0)
		{
			ERRNO = ENOSPC;
			return -1;
		};
	};
	
	size_t realpos = itab->fatfs->clusterPos + itab->fatfs->clusterSize * (itab->clusters[clusterIndex]-2) + offset;
	if (vfsPWrite(itab->fatfs->fp, dent, 32, realpos) != 32)
	{
		ERRNO = EIO;
		return -1;
	};
	
	return 0;
};

static int fatfsInodeFlush(Inode *inode)
{
	FATInodeTable *itab = (FATInodeTable*) inode->fsdata;
	
	semWait(&itab->fatfs->lock);
	if (itab->dentpos != 0 && inode->ft != NULL)
	{
		FATDent dent;
		if (vfsPRead(itab->fatfs->fp, &dent, 32, itab->dentpos) != 32)
		{
			semSignal(&itab->fatfs->lock);
			ERRNO = EIO;
			return -1;
		};
		
		dent.size = (uint32_t) inode->ft->size;
		if (vfsPWrite(itab->fatfs->fp, &dent, 32, itab->dentpos) != 32)
		{
			semSignal(&itab->fatfs->lock);
			ERRNO = EIO;
			return -1;
		};
	};
	
	if (inode->ft != NULL)
	{
		itab->csize = inode->ft->size;
	};
	
	if ((inode->mode & VFS_MODE_TYPEMASK) == VFS_MODE_DIRECTORY)
	{
		// start by writing the "." and ".." entries
		FATDent dot;
		memset(&dot, 0, 32);
		memcpy(dot.filename, ".          ", 11);
		dot.attr = FAT_ATTR_DIR;
		dot.clusterHigh = itab->clusters[0] >> 16;
		dot.clusterLow = itab->clusters[0];
		dot.size = 0;
		
		if (vfsPWrite(itab->fatfs->fp, &dot, 32, itab->fatfs->clusterPos + itab->fatfs->clusterSize * (itab->clusters[0]-2)) != 32)
		{
			semSignal(&itab->fatfs->lock);
			ERRNO = EIO;
			return -1;
		};
		
		memcpy(dot.filename, "..         ", 11);
		// TODO: perhaps actually set the cluster number correctly, based on parent ???
		
		if (vfsPWrite(itab->fatfs->fp, &dot, 32, itab->fatfs->clusterPos + itab->fatfs->clusterSize * (itab->clusters[0]-2) + 32) != 32)
		{
			semSignal(&itab->fatfs->lock);
			ERRNO = EIO;
			return -1;
		};
		
		// write other dentries
		size_t currentPos = 64;
		
		Dentry *dent;
		for (dent=inode->dents; dent!=NULL; dent=dent->next)
		{
			if ((dent->flags & VFS_DENTRY_TEMP) == 0)
			{
				// first we need to figure out the 8.3 filename
				char shortname[11];
				memset(shortname, ' ', 11);
				int putidx = 0;
			
				int seenDot = 0;
				const char *scan;
				for (scan=dent->name; *scan!=0; scan++)
				{
					char c = *scan;
				
					if (c == '.')
					{
						if (seenDot) continue;
					
						putidx = 8;
						seenDot = 1;
						continue;
					};
				
					if (c >= 'a' && c <= 'z')
					{
						c &= ~(1 << 5);
					};
					
					if ((c < 'A' || c > 'Z') && (c < '0' || c > '9') && (c != '_') && (c != '-'))
					{
						continue;
					};
				
					if (putidx == 8)
					{
						if (!seenDot) continue;
					};
				
					if (putidx == 11) break;
				
					shortname[putidx++] = c;
				};
			
				if (shortname[0] == ' ')
				{
					memcpy(shortname, "UNKNOWN DAT", 11);
				};
				
				scan = shortname;
				uint8_t sum = 0;
				int count = 11;
				
				while (count--)
				{
					sum = ((sum & 1) << 7) + (sum >> 1) + (uint8_t) (*scan++);
				};
				
				// make a long name out of UCS-2 characters terminated in '00 00 FF FF'.
				uint16_t longname[256];
				memset(longname, 0, 512);
				putidx = 0;
				
				for (scan=dent->name; *scan!=0; scan++)
				{
					if (putidx == 253) break;
					
					char c = *scan;
					if (c & 0x80)
					{
						longname[putidx++] = (uint16_t) '?';
					}
					else
					{
						longname[putidx++] = (uint16_t) c;
					};
				};
				
				longname[putidx++] = 0;
				longname[putidx++] = 0xFFFF;
				
				// 13 characters per entry; so see how many we need
				size_t numFullEnts = (size_t) putidx / 13;
				size_t leftover = (size_t) putidx % 13;
				
				int isFirst = 1;
				if (leftover != 0)
				{
					// write the "leftover" entry
					uint16_t bunch[13];
					memset(bunch, 0, 26);
					memcpy(bunch, &longname[numFullEnts * 13], 2 * leftover);
					
					FATLongEntry lent;
					lent.seqno = 0x40 | (uint8_t) (numFullEnts+1);
					memcpy(lent.firstChars, bunch, 10);
					lent.attr = 0x0F;
					lent.type = 0;
					lent.checksum = sum;
					memcpy(lent.nextChars, &bunch[5], 12);
					lent.zero = 0;
					memcpy(lent.finalChars, &bunch[11], 4);
					
					putdent(itab, currentPos, (FATDent*) &lent);
					currentPos += 32;
					
					isFirst = 0;
				};
				
				// write all other entries
				while (numFullEnts != 0)
				{
					uint16_t bunch[13];
					memcpy(bunch, &longname[(numFullEnts-1) * 13], 26);
					
					FATLongEntry lent;
					lent.seqno = (uint8_t) (numFullEnts);
					if (isFirst) lent.seqno |= 0x40;
					isFirst = 0;
					memcpy(lent.firstChars, bunch, 10);
					lent.attr = 0x0F;
					lent.type = 0;
					lent.checksum = sum;
					memcpy(lent.nextChars, &bunch[5], 12);
					lent.zero = 0;
					memcpy(lent.finalChars, &bunch[11], 4);
					
					putdent(itab, currentPos, (FATDent*) &lent);
					currentPos += 32;
					
					numFullEnts--;
				};
				
				// AND that brings us to the real dentry
				FATDent fent;
				memset(&fent, 0, sizeof(FATDent));
				memcpy(fent.filename, shortname, 11);
				
				FATInodeTable *targetItab = getInodeInfoByNo(itab->fatfs, dent->ino);
				assert(targetItab != NULL);
				
				fent.attr = targetItab->cattr;
				fent.clusterHigh = (uint16_t) (targetItab->clusters[0] >> 16);
				fent.clusterLow = (uint16_t) (targetItab->clusters[0]);
				fent.size = targetItab->csize;
				
				// update the position of the dent
				size_t clusterIndex = currentPos / itab->fatfs->clusterSize;
				size_t offset = currentPos % itab->fatfs->clusterSize;
				while (clusterIndex >= itab->numClusters)
				{
					if (expandClusterChain(itab) != 0)
					{
						ERRNO = ENOSPC;
						return -1;
					};
				};
				size_t realpos = itab->fatfs->clusterPos + itab->fatfs->clusterSize * (itab->clusters[clusterIndex]-2) + offset;
				targetItab->dentpos = realpos;
				
				putdent(itab, currentPos, &fent);
				currentPos += 32;
			};
		};
		
		// zero out the rest of the cluster
		size_t clusterIndex = currentPos / itab->fatfs->clusterSize;
		size_t offset = currentPos % itab->fatfs->clusterSize;
		while (clusterIndex >= itab->numClusters)
		{
			if (expandClusterChain(itab) != 0)
			{
				ERRNO = ENOSPC;
				return -1;
			};
		};
		size_t realpos = itab->fatfs->clusterPos + itab->fatfs->clusterSize * (itab->clusters[clusterIndex]-2) + offset;
		size_t sizeLeft = itab->fatfs->clusterSize - offset;
		
		char padding[sizeLeft];
		memset(padding, 0, sizeLeft);
		vfsPWrite(itab->fatfs->fp, padding, sizeLeft, realpos);
	};
	
	semSignal(&itab->fatfs->lock);
	return 0;
};

static void fatfsDropInode(Inode *inode)
{
	FATInodeTable *itab = (FATInodeTable*) inode->fsdata;
	
	if (itab->dentpos != 0)
	{
		FATDent dent;
		if (vfsPRead(itab->fatfs->fp, &dent, 32, itab->dentpos) != 32)
		{
			kprintf("fatfs: warning: failed to read dentry when dropping\n");
			semSignal(&itab->fatfs->lock);
			return;
		};
		
		dent.filename[0] = 0xE5;
		dent.clusterHigh = dent.clusterLow = 0;
		if (vfsPWrite(itab->fatfs->fp, &dent, 32, itab->dentpos) != 32)
		{
			kprintf("fatfs: warning: failed to write dentry when dropping\n");
			semSignal(&itab->fatfs->lock);
			return;
		};
		
		// free all the clusters
		__sync_fetch_and_add(&itab->fatfs->fs->freeBlocks, itab->numClusters);
		
		uint32_t freeval = 0;
		size_t i;
		for (i=0; i<itab->numClusters; i++)
		{
			vfsPWrite(itab->fatfs->fp, &freeval, 4, itab->fatfs->fatpos + 4 * itab->clusters[i]);
		};
		
		kfree(itab->clusters);
		itab->clusters = NULL;
		itab->numClusters = 0;
	};
	
	semSignal(&itab->fatfs->lock);
};

static void inodeSetOps(Inode *inode)
{
	inode->flush = fatfsInodeFlush;
	inode->drop = fatfsDropInode;
};

static int fatfsRegInode(FileSystem *fs, Inode *inode)
{
	Fatfs *fatfs = (Fatfs*) fs->fsdata;
	semWait(&fatfs->lock);
	
	uint32_t newCluster = getFreeCluster(fatfs);
	if (newCluster == 0)
	{
		semSignal(&fatfs->lock);
		ERRNO = ENOSPC;
		return -1;
	};
	
	char buffer[fatfs->clusterSize];
	memset(buffer, 0, fatfs->clusterSize);
	if (vfsPWrite(fatfs->fp, buffer, fatfs->clusterSize, fatfs->clusterPos + fatfs->clusterSize * (newCluster-2)) != fatfs->clusterSize)
	{
		semSignal(&fatfs->lock);
		ERRNO = EIO;
		return -1;
	};
	
	uint32_t endval = 0x0FFFFFFF;
	if (vfsPWrite(fatfs->fp, &endval, 4, fatfs->fatpos + 4 * newCluster) != 4)
	{
		semSignal(&fatfs->lock);
		ERRNO = EIO;
		return -1;
	};
	
	FATInodeTable *itab = NEW(FATInodeTable);
	memset(itab, 0, sizeof(FATInodeTable));
	
	itab->next = fatfs->itab;
	fatfs->itab = itab;
	
	itab->ino = __sync_fetch_and_add(&fatfs->nextIno, 1);
	inode->ino = itab->ino;
	itab->dentpos = 0;
	itab->clusters = (uint32_t*) kmalloc(4);
	itab->clusters[0] = newCluster;
	itab->numClusters = 1;
	itab->fatfs = fatfs;
	itab->csize = 0;
	itab->cattr = 0;
	
	if ((inode->mode & VFS_MODE_TYPEMASK) == VFS_MODE_DIRECTORY)
	{
		itab->cattr = FAT_ATTR_DIR;
	}
	else
	{
		FileTree *ft = ftCreate(0);
		inode->ft = ft;
		
		ft->data = itab;
		ft->size = 0;
		
		fatfsSetTreeOps(ft);
	};
	
	inode->fsdata = itab;
	inodeSetOps(inode);
	__sync_fetch_and_add(&fs->freeBlocks, -1);
	semSignal(&fatfs->lock);
	return 0;
};

static int fatfsLoadInode(FileSystem *fs, Inode *inode)
{
	Fatfs *fatfs = (Fatfs*) fs->fsdata;
	
	semWait(&fatfs->lock);
	FATInodeTable *itab = getInodeInfoByNo(fatfs, inode->ino);
	if (itab == NULL)
	{
		semSignal(&fatfs->lock);
		return -1;
	};
	
	int isDir = 0;
	if (itab->dentpos == 0)
	{
		// root directory
		inode->mode = 0755 | VFS_MODE_DIRECTORY;
		isDir = 1;
	}
	else
	{
		FATDent info;
		if (vfsPRead(fatfs->fp, &info, 32, itab->dentpos) != 32)
		{
			semSignal(&fatfs->lock);
			return -1;
		};
		
		inode->mode = 0755;
		
		if (info.attr & FAT_ATTR_DIR)
		{
			inode->mode |= VFS_MODE_DIRECTORY;
			isDir = 1;
		}
		else
		{
			FileTree *ft = ftCreate(0);
			inode->ft = ft;
			
			ft->data = itab;
			ft->size = info.size;
			
			fatfsSetTreeOps(ft);
		};
	};
	
	inode->numBlocks = itab->numClusters;
	inode->links = 1;
	
	if (isDir)
	{
		// fetch directory entries
		char longname[256];
		char *longput = &longname[255];
		*longput = 0;
		
		size_t dentsPerCluster = fatfs->clusterSize / 32;
		size_t i;
		for (i=0;; i++)
		{
			size_t clusterIndex = i / dentsPerCluster;
			size_t dentIndex = i % dentsPerCluster;
			
			if (clusterIndex >= itab->numClusters) break;
			off_t pos = fatfs->clusterPos + fatfs->clusterSize * (itab->clusters[clusterIndex]-2) + 32 * dentIndex;
			
			FATDent ent;
			if (vfsPRead(fatfs->fp, &ent, 32, pos) != 32) break;
			if (ent.filename[0] == 0) break;
			if ((uint8_t)ent.filename[0] == 0xE5)
			{
				longput = &longname[255];
				*longput = 0;
				continue;
			};
			if ((uint8_t)ent.filename[0] == 0x2E) continue;		// ignore "." entries
			if (ent.attr == FAT_ATTR_LFN)
			{
				FATLongEntry *longent = (FATLongEntry*) &ent;
				if (longent->seqno & 0x40)
				{
					longput = &longname[255];
					*longput = 0;
				};
				
				char convbuf[16];
				char *convput = convbuf;
				
				int j;
				for (j=0; j<5; j++)
				{
					uint16_t c = longent->firstChars[j];
					if (c == 0xFFFF) c = 0;
					if (c > 127) *convput++ = '?';
					else *convput++ = (char) c;
				};
				
				for (j=0; j<6; j++)
				{
					uint16_t c = longent->nextChars[j];
					if (c == 0xFFFF) c = 0;
					if (c > 127) *convput++ = '?';
					else *convput++ = (char) c;
				};

				for (j=0; j<2; j++)
				{
					uint16_t c = longent->finalChars[j];
					if (c == 0xFFFF) c = 0;
					if (c > 127) *convput++ = '?';
					else *convput++ = (char) c;
				};
				
				*convput = 0;
				if ((longput-longname) >= strlen(convbuf))
				{
					longput -= strlen(convbuf);
					memcpy(longput, convbuf, strlen(convbuf));
				};
			};
			if (ent.attr & FAT_ATTR_HIDDEN) continue;

			uint32_t cluster = ((uint32_t) ent.clusterHigh << 16) | ent.clusterLow;
			FATInodeTable *entInfo = getInodeInfoByCluster(fatfs, cluster);
			entInfo->dentpos = pos;
			entInfo->csize = ent.size;
			entInfo->cattr = ent.attr;
			
			if (*longput == 0)
			{
				// no long name
				char namebuf[13];
				char *put = namebuf;
				int j;
				int dotPending = 1;
				for (j=0; j<11; j++)
				{
					if (ent.filename[j] == ' ') continue;
				
					if (j >= 8 && dotPending)
					{
						*put++ = '.';
						dotPending = 0;
					};
				
					*put++ = ent.filename[j];
				};
			
				*put = 0;
			
				vfsAppendDentry(inode, namebuf, entInfo->ino);
			}
			else
			{
				vfsAppendDentry(inode, longput, entInfo->ino);
			};
			
			longput = &longname[255];
			*longput = 0;
		};
	};
	
	inode->fsdata = itab;
	inodeSetOps(inode);
	semSignal(&fatfs->lock);
	(void)isDir;
	return 0;
};

static void fatfsUnmount(FileSystem *fs)
{
	Fatfs *fatfs = (Fatfs*) fs->fsdata;
	while (fatfs->itab != NULL)
	{
		FATInodeTable *itab = fatfs->itab;
		fatfs->itab = itab->next;
		
		kfree(itab->clusters);
		kfree(itab);
	};
	
	vfsClose(fatfs->fp);
	kfree(fatfs);
};

static Inode* fatfsOpenRoot(const char *image, int flags, const void *options, size_t optlen, int *error)
{
	kprintf("fatfs: attempting to mount %s as FAT32\n", image);
	
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
	
	FATBootRecord vbr;
	assert(sizeof(FATBootRecord) == 512);
	assert(sizeof(FATDent) == 32);
	if (vfsPRead(fp, &vbr, 512, 0) != 512)
	{
		kprintf("fatfs: failed to read VBR\n");
		vfsClose(fp);
		*error = EINVAL;
		return NULL;
	};
	
	// windows and OS X check this apparently; i should also do it to avoid accidentally
	// mounting non-FAT32 drives.
	if ((vbr.jump[0] != 0xEB && vbr.jump[0] != 0xE9) || vbr.jump[2] != 0x90)
	{
		kprintf("fatfs: invalid jump header pattern (%02hhX %02hhX %02hhX)\n", vbr.jump[0], vbr.jump[1], vbr.jump[2]);
		vfsClose(fp);
		*error = EINVAL;
		return NULL;
	};
	
	if (vbr.bootsig != 0xAA55)
	{
		kprintf("fatfs: invalid boot signature (0x%04hX)\n", vbr.bootsig);
		vfsClose(fp);
		*error = EINVAL;
		return NULL;
	};
	
	if (vbr.ndents != 0)
	{
		kprintf("fatfs: 'directory entry count' is not zero\n");
		vfsClose(fp);
		*error = EINVAL;
		return NULL;
	};
	
	if (memcmp(vbr.systemID, "FAT32   ", 8) != 0)
	{
		kprintf("fatfs: FAT type label not FAT32\n");
		vfsClose(fp);
		*error = EINVAL;
		return NULL;
	};
	
	char oemid[9];
	memcpy(oemid, vbr.oemid, 8);
	oemid[8] = 0;
	
	char volumeLabel[12];
	memcpy(volumeLabel, vbr.volumeLabel, 11);
	volumeLabel[11] = 0;
	
	char sysid[9];
	memcpy(sysid, vbr.systemID, 8);
	sysid[8] = 0;
	
	kprintf("fatfs: OEM ID: `%s'\n", oemid);
	kprintf("fatfs: sector size: %hu\n", vbr.sectorSize);
	kprintf("fatfs: sectors per cluster: %hhu\n", vbr.sectorsPerCluster);
	kprintf("fatfs: reserved sectors: %hu\n", vbr.reservedSectors);
	kprintf("fatfs: FAT copies: %hhu\n", vbr.numFats);
	kprintf("fatfs: number of dents: %hu\n", vbr.ndents);
	kprintf("fatfs: sectors (small): %hu\n", vbr.sectorsSmall);
	kprintf("fatfs: sectors per FAT (old): %hu\n", vbr.sectorsPerFatOld);
	kprintf("fatfs: hidden sectors: %u\n", vbr.hiddenSectors);
	kprintf("fatfs: sectors (large): %u\n", vbr.sectorsLarge);
	kprintf("fatfs: sectors per FAT (correct): %u\n", vbr.sectorsPerFat);
	kprintf("fatfs: FAT version: 0x%04hX\n", vbr.version);
	kprintf("fatfs: root directory cluster: %u\n", vbr.rootCluster);
	kprintf("fatfs: fsinfo sector: %hu\n", vbr.infoSector);
	kprintf("fatfs: backup boot sector: %hu\n", vbr.backupBootSector);
	kprintf("fatfs: EBPB signature: 0x%02hhX\n", vbr.sig);
	kprintf("fatfs: serial number: 0x%08X\n", vbr.volumeSerial);
	kprintf("fatfs: volume label: `%s'\n", volumeLabel);
	kprintf("fatfs: system ID: `%s'\n", sysid);

	Fatfs *fatfs = NEW(Fatfs);
	fatfs->fp = fp;
	memcpy(&fatfs->vbr, &vbr, 512);
	fatfs->fatpos = vbr.reservedSectors * vbr.sectorSize;
	fatfs->nextIno = 3;				// 2 = root directory
	
	// initialize the virtual inode table by creating an entry for the root directory
	FATInodeTable *itab = NEW(FATInodeTable);
	memset(itab, 0, sizeof(FATInodeTable));
	itab->ino = 2;
	itab->fatfs = fatfs;
	readClusterChain(fatfs, vbr.rootCluster, &itab->clusters, &itab->numClusters);
	fatfs->itab = itab;
	
	semInit(&fatfs->lock);

	fatfs->clusterSize = vbr.sectorSize * vbr.sectorsPerCluster;
	fatfs->clusterPos = (vbr.reservedSectors + vbr.numFats * vbr.sectorsPerFat) * vbr.sectorSize;
	kprintf("fatfs: cluster offset: %ld\n", fatfs->clusterPos);
	kprintf("fatfs: cluster size: %lu\n", fatfs->clusterSize);
	fatfs->numClusters = ((size_t)vbr.sectorsLarge * (size_t)vbr.sectorSize - fatfs->clusterPos) / fatfs->clusterSize;
	
	FileSystem *fs = vfsCreateFileSystem("fatfs");
	fs->fsdata = fatfs;
	fs->loadInode = fatfsLoadInode;
	fs->regInode = fatfsRegInode;
	fs->unmount = fatfsUnmount;
	fs->blockSize = vbr.sectorSize * vbr.sectorsPerCluster;
	fs->blocks = fatfs->numClusters;
	memcpy(fs->bootid, "\0\0" "FAT32\0\0\0\0\0\0\x0F\xA7\xF5", 16);
	*((uint32_t*)&fs->bootid[8]) = vbr.volumeSerial;
	
	uint32_t *track = (uint32_t*) kmalloc(2 * 1024 * 1024);
	size_t currentTrack = (size_t) -1;
	
	kprintf("fatfs: number of clusters: %lu\n", fatfs->numClusters);
	
	size_t i;
	for (i=0; i<fatfs->numClusters; i++)
	{
		size_t trackno = i / (2 * 1024 * 1024 / 4);
		size_t offset = i % (2 * 1024 * 1024 / 4);
		
		if (currentTrack != trackno)
		{
			currentTrack = trackno;
			if (vfsPRead(fp, track, 2 * 1024 * 1024, fatfs->fatpos + currentTrack * 2 * 1024 * 1024) < 2 * 1024 * 1024)
			{
				break;
			};
		};
		
		fs->freeBlocks += !track[offset];
	};
	
	kfree(track);
	fatfs->fs = fs;
	
	Inode *root = vfsCreateInode(NULL, VFS_MODE_DIRECTORY);
	root->fs = fs;
	fs->imap = root;
	root->ino = 2;
	
	if (fatfsLoadInode(fs, root) != 0)
	{
		vfsDownrefInode(root);
		kfree(itab);
		kfree(fs);
		kfree(fatfs);
		vfsClose(fp);
		*error = EIO;
		return NULL;
	};
	
	return root;
};

static FSDriver fatfs = {
	.fsname = "fatfs",
	.openroot = fatfsOpenRoot,
};

MODULE_INIT()
{
	kprintf("fatfs: registering FAT32 driver\n");
	registerFSDriver(&fatfs);
	return MODINIT_OK;
};

MODULE_FINI()
{
	kprintf("fatfs: removing FAT32 driver\n");
	registerFSDriver(&fatfs);
	return 0;
};
