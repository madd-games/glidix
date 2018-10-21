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
#include <glidix/storage/storage.h>
#include <glidix/util/memory.h>
#include <glidix/util/string.h>
#include <glidix/display/console.h>
#include <glidix/hw/physmem.h>

/**
 * Bitmap of used drive letters (for /dev/sdX). Bit n represents letter 'a'+n,
 * for example bit 0 represents 'a', bit 1 represents 'b', etc. Note that there
 * are only 26 possible letters.
 */
static uint32_t sdLetters;

/**
 * Maps drive letters (index 0 being 'a', index 1 being 'b', etc) to respective storage devices.
 */
Mutex mtxList;
static StorageDevice* sdList[26];

static void reloadPartTable(StorageDevice *sd);

static char sdAllocLetter()
{
	int i;
	for (i=0; i<26; i++)
	{
		if (atomic_test_and_set(&sdLetters, i) == 0)
		{
			return 'a'+i;
		};
	};
	
	return 0;
};

static void sdDownref(StorageDevice *sd)
{
	if (__sync_add_and_fetch(&sd->refcount, -1) == 0)
	{	
		kfree(sd);
	};
};

static void sdUpref(StorageDevice *sd)
{
	__sync_fetch_and_add(&sd->refcount, 1);
};

static void sdFreeLetter(char c)
{
	uint32_t mask = ~(1 << (c-'a'));
	__sync_fetch_and_and(&sdLetters, mask);
};

void sdInit()
{
	sdLetters = 0;
	memset(sdList, 0, sizeof(void*)*26);
	mutexInit(&mtxList);
};

static int sdfile_ioctl(Inode *inode, File *fp, uint64_t cmd, void *params)
{
	if (cmd == IOCTL_SDI_IDENTITY)
	{
		SDHandle *data = (SDHandle*) fp->filedata;
		mutexLock(&data->sd->lock);
		
		SDIdentity *id = (SDIdentity*) params;
		id->flags = data->sd->flags;
		id->partIndex = data->partIndex;
		id->offset = data->offset;
		id->size = data->size;
		strcpy(id->name, data->name);
		
		mutexUnlock(&data->sd->lock);
		return 0;
	}
	else if (cmd == IOCTL_SDI_EJECT)
	{
		SDHandle *data = (SDHandle*) fp->filedata;
		if (data->sd->totalSize != 0)
		{
			// non-removable
			ERRNO = EINVAL;
			return -1;
		};
		
		if (!IMPLEMENTS(data->sd->ops, eject))
		{
			// driver does not implement the 'eject' operation
			ERRNO = ENODEV;
			return -1;
		};
		
		int status = data->sd->ops->eject(data->sd->drvdata);
		if (status != 0)
		{
			ERRNO = status;
			return -1;
		};
	
		return 0;
	};

	ERRNO = ENODEV;
	return -1;
};

static int sdfile_pathctl(Inode *inode, uint64_t cmd, void *params)
{
	if (cmd == IOCTL_SDI_IDENTITY)
	{
		SDDeviceFile *data = (SDDeviceFile*) inode->fsdata;
		mutexLock(&data->sd->lock);
		
		SDIdentity *id = (SDIdentity*) params;
		id->flags = data->sd->flags;
		id->partIndex = data->partIndex;
		id->offset = data->offset;
		id->size = data->size;
		strcpy(id->name, data->name);
		
		mutexUnlock(&data->sd->lock);
		return 0;
	};

	ERRNO = ENODEV;
	return -1;
};

static void sdFlushTree(StorageDevice *sd, BlockTreeNode *node, int level, uint64_t pos)
{
	uint64_t i;
	for (i=0; i<128; i++)
	{
		if (node->entries[i] & SD_BLOCK_DIRTY)
		{
			node->entries[i] &= ~SD_BLOCK_DIRTY;
			if (level == 6)
			{
				uint64_t canaddr = (node->entries[i] & 0xFFFFFFFFFFFF) | 0xFFFF800000000000;
				size_t bytepos = ((pos << 7) | i) << 15;
				size_t startBlock = bytepos / sd->blockSize;
				size_t numBlocks = SD_TRACK_SIZE / sd->blockSize;
				sd->ops->writeBlocks(sd->drvdata, startBlock, numBlocks, (const void*) canaddr);
			}
			else
			{
				uint64_t canaddr = (node->entries[i] & 0xFFFFFFFFFFFF) | 0xFFFF800000000000;
				sdFlushTree(sd, (BlockTreeNode*)canaddr, level+1, (pos << 7) | i);
			};
		};
	};
};

static void sdFlush(StorageDevice *sd)
{
	// call this only when the cache is locked
	sdFlushTree(sd, &sd->cacheTop, 0, 0);
};

static int sdfile_flush(Inode *inode)
{
	SDDeviceFile *fdev = (SDDeviceFile*) inode->fsdata;
	mutexLock(&fdev->sd->cacheLock);
	sdFlush(fdev->sd);
	mutexUnlock(&fdev->sd->cacheLock);
	
	return 0;
};

static void sdfile_close(Inode *inode, void *filedata)
{
	sdfile_flush(inode);
	
	SDHandle *handle = (SDHandle*) filedata;
	mutexLock(&handle->sd->lock);
	
	if (handle->partIndex == -1)
	{
		handle->sd->openParts = 0;
		mutexUnlock(&handle->sd->lock);
		reloadPartTable(handle->sd);
	}
	else
	{
		uint64_t mask = 1UL << (uint64_t)handle->partIndex;
		handle->sd->openParts &= ~mask;
		mutexUnlock(&handle->sd->lock);
	};

	sdDownref(handle->sd);
	kfree(handle);
};

/**
 * Return a pointer to the specified cache track. If 'make' is 0, and the track does not exist,
 * NULL is returned (if 'make' is 1, the track is read from disk on a cache miss). If 'dirty' is
 * 1, the track is marked dirty. Call this ONLY while the cacheLock is locked.
 *
 * NULL can also be returned on error, in whcih case *error is set to the errno.
 *
 * If 'make' is zero and the track was not found, NULL is returned, and *error set to EAGAIN.
 */
static void* sdGetCache(StorageDevice *sd, uint64_t pos, int make, int dirty, int *error)
{
	uint64_t i;
	BlockTreeNode *node = &sd->cacheTop;
	for (i=0; i<6; i++)
	{
		uint64_t sub = (pos >> (15 + 7 * (6 - i))) & 0x7F;
		uint64_t entry = node->entries[sub];
		
		if (entry == 0)
		{
			if (!make)
			{
				*error = EAGAIN;
				return NULL;
			};
			
			getCurrentThread()->sdMissNow = 1;
			BlockTreeNode *nextNode = NEW(BlockTreeNode);
			memset(nextNode, 0, sizeof(BlockTreeNode));
			getCurrentThread()->sdMissNow = 0;
			
			// bottom 48 bits of address, set usage counter to 1, dirty if needed
			node->entries[sub] = ((uint64_t) nextNode & 0xFFFFFFFFFFFF) | (1UL << 56);
			if (dirty) node->entries[sub] |= SD_BLOCK_DIRTY;
			
			node = nextNode;
		}
		else
		{
			// increment usage counter
			uint64_t ucnt = entry >> 56;
			if (ucnt != 255)
			{
				node->entries[sub] += (1UL << 56);
			};
			if (dirty) node->entries[sub] |= SD_BLOCK_DIRTY;
			
			// get canonical address
			uint64_t canaddr = (node->entries[sub] & 0xFFFFFFFFFFFF) | 0xFFFF800000000000;
			
			// follow
			node = (BlockTreeNode*) canaddr;
		};
	};
	
	uint64_t track = (pos >> 15) & 0x7F;
	uint64_t trackAddr;
	if (node->entries[track] == 0)
	{
		if (!make)
		{
			*error = EAGAIN;
			return NULL;
		};
		
		getCurrentThread()->sdMissNow = 1;
		uint64_t frames[8];
		int k;
		for (k=0; k<8; k++)
		{
			frames[k] = phmAllocFrame();
			if (frames[k] == 0)
			{
				int l;
				for (l=0; l<k; l++)
				{
					phmFreeFrame(frames[l]);
				};
				
				getCurrentThread()->sdMissNow = 0;
				*error = ENOMEM;
				return NULL;
			};
		};
		
		void *vptr = mapPhysMemoryList(frames, 8);
		getCurrentThread()->sdMissNow = 0;
		int status = sd->ops->readBlocks(sd->drvdata, (pos & ~0x7FFFUL) / sd->blockSize,
							SD_TRACK_SIZE / sd->blockSize,
							vptr);
		if (status != 0)
		{
			unmapPhysMemory(vptr, 0x8000);
			for (k=0; k<8; k++) phmFreeFrame(frames[k]);
			
			*error = status;
			return NULL;
		};
		
		__sync_fetch_and_add(&phmCachedFrames, 8);
		
		node->entries[track] = ((uint64_t) vptr & 0xFFFFFFFFFFFF) | (1UL << 56);
		if (dirty) node->entries[track] |= SD_BLOCK_DIRTY;
		trackAddr = (uint64_t) vptr;
	}
	else
	{
		trackAddr = (node->entries[track] & 0xFFFFFFFFFFFF) | 0xFFFF800000000000;
		if (dirty) node->entries[track] |= SD_BLOCK_DIRTY;
	};
	
	return (void*) trackAddr;
};

static ssize_t sdRead(StorageDevice *sd, uint64_t pos, void *buf, size_t size)
{
	if (sd->flags & SD_HANGUP)
	{
		ERRNO = ENXIO;
		return -1;
	};
	
	uint8_t *put = (uint8_t*) buf;
	ssize_t sizeRead = 0;
	
	while (size > 0)
	{
		uint64_t offsetIntoPage = pos & (SD_TRACK_SIZE-1);
		uint64_t toRead = SD_TRACK_SIZE - offsetIntoPage;
		
		if (toRead > size)
		{
			toRead = size;
		};
		
		// see if this page is in the cache; otherwise load it
		mutexLock(&sd->cacheLock);
		
		int error;
		uint64_t trackAddr = (uint64_t) sdGetCache(sd, pos, !getCurrentThread()->allocFromCacheNow, 0, &error);
		if (trackAddr == 0)
		{
			int fixed = 0;
			if (error == EAGAIN || error == ENOMEM)
			{
				// cache miss but allocations banned
				char tmp[SD_TRACK_SIZE];
				int status = sd->ops->readBlocks(sd->drvdata, (pos & ~0x7FFFUL) / sd->blockSize,
									SD_TRACK_SIZE / sd->blockSize,
									tmp);
				if (status == 0) fixed = 1;
				else error = status;
				
				trackAddr = (uint64_t) tmp;
			};
			
			if (!fixed)
			{	
				mutexUnlock(&sd->cacheLock);
				
				if (sizeRead == 0)
				{
					ERRNO = error;
					return -1;
				};
				
				return sizeRead;
			};
		};
		
		memcpy(put, (void*)(trackAddr + offsetIntoPage), toRead);
		put += toRead;
		sizeRead += toRead;
		pos += toRead;
		size -= toRead;
		
		mutexUnlock(&sd->cacheLock);
	};
	
	return sizeRead;
};

static ssize_t sdWrite(StorageDevice *sd, uint64_t pos, const void *buf, size_t size)
{
	if (sd->flags & SD_HANGUP)
	{
		ERRNO = ENXIO;
		return -1;
	};
	
	const uint8_t *scan = (const uint8_t*) buf;
	ssize_t sizeWritten = 0;
	
	while (size > 0)
	{
		uint64_t offsetIntoPage = pos & (SD_TRACK_SIZE-1UL);
		uint64_t toWrite = SD_TRACK_SIZE - offsetIntoPage;
		
		if (toWrite > size)
		{
			toWrite = size;
		};
		
		// see if this page is in the cache; otherwise load it
		mutexLock(&sd->cacheLock);
		
		int immediateFlush = 0;
		int error;
		uint64_t trackAddr = (uint64_t) sdGetCache(sd, pos, !getCurrentThread()->allocFromCacheNow, 1, &error);
		if (trackAddr == 0)
		{
			int fixed = 0;
			if (error == EAGAIN || error == ENOMEM)
			{
				// cache miss but allocations banned
				char tmp[SD_TRACK_SIZE];
				int status = sd->ops->readBlocks(sd->drvdata, (pos & ~0x7FFFUL) / sd->blockSize,
									SD_TRACK_SIZE / sd->blockSize,
									tmp);
				if (status == 0) fixed = 1;
				else error = status;
				
				trackAddr = (uint64_t) tmp;
				immediateFlush = 1;
			};
			
			if (!fixed)
			{	
				mutexUnlock(&sd->cacheLock);
				
				if (sizeWritten == 0)
				{
					ERRNO = error;
					return -1;
				};
				
				return sizeWritten;
			};
		};
		
		size_t orgpos = pos;
		
		memcpy((void*)(trackAddr + offsetIntoPage), scan, toWrite);
		scan += toWrite;
		sizeWritten += toWrite;
		pos += toWrite;
		size -= toWrite;
		
		if (immediateFlush)
		{
			sd->ops->readBlocks(sd->drvdata, (orgpos & ~0x7FFFUL) / sd->blockSize,
						SD_TRACK_SIZE / sd->blockSize,
						(void*)trackAddr);
		};
		
		mutexUnlock(&sd->cacheLock);
	};

	return sizeWritten;
};

static ssize_t sdfile_pread(Inode *inode, File *fp, void *buf, size_t size, off_t offset)
{
	SDHandle *handle = (SDHandle*) fp->filedata;
	uint64_t actualStart = handle->offset + (uint64_t) offset;
	if (handle->size != 0)
	{
		if (offset >= handle->size)
		{
			return 0;
		};
		
		if ((offset+size) > handle->size)
		{
			size = handle->size - offset;
		};
	};
	return sdRead(handle->sd, actualStart, buf, size);
};

static ssize_t sdfile_pwrite(Inode *inode, File *fp, const void *buf, size_t size, off_t offset)
{
	SDHandle *handle = (SDHandle*) fp->filedata;
	uint64_t actualStart = (uint64_t) handle->offset + (uint64_t) offset;
	if (handle->size != 0)
	{
		if (offset >= handle->size)
		{
			return 0;
		};
		
		if ((offset+size) > handle->size)
		{
			size = handle->size - offset;
		};
	};
	return sdWrite(handle->sd, actualStart, buf, size);
};

static void* sdfile_open(Inode *inode, int oflags)
{
	SDHandle *handle = NEW(SDHandle);
	if (handle == NULL)
	{
		ERRNO = ENOMEM;
		return NULL;
	};
	
	SDDeviceFile *fdev = (SDDeviceFile*) inode->fsdata;
	mutexLock(&fdev->sd->lock);
	if (fdev->partIndex == -1)
	{
		if (fdev->sd->openParts != 0)
		{
			mutexUnlock(&fdev->sd->lock);
			ERRNO = EBUSY;
			return NULL;
		}
		else
		{
			fdev->sd->openParts = SD_MASTER_OPEN;
		};
	}
	else
	{
		uint64_t mask = 1UL << (uint64_t)fdev->partIndex;
		if (fdev->sd->openParts & mask)
		{
			mutexUnlock(&fdev->sd->lock);
			ERRNO = EBUSY;
			return NULL;
		}
		else
		{
			fdev->sd->openParts |= mask;
		};
	};
	mutexUnlock(&fdev->sd->lock);
	
	sdUpref(fdev->sd);
	
	handle->sd = fdev->sd;
	handle->offset = fdev->offset;
	handle->size = fdev->size;
	handle->partIndex = fdev->partIndex;
	strcpy(handle->name, fdev->name);
	
	return handle;
};

static size_t sdfile_getsize(Inode *inode)
{
	SDDeviceFile *fdev = (SDDeviceFile*) inode->fsdata;
	mutexLock(&fdev->sd->lock);
	
	size_t size = fdev->sd->totalSize;
	if (fdev->size != 0)
	{
		size = fdev->size;
	}
	else
	{
		if (size == 0)
		{
			if (IMPLEMENTS(fdev->sd->ops, getSize))
			{
				size = fdev->sd->ops->getSize(fdev->sd->drvdata);
			};
		};
	};
	
	mutexUnlock(&fdev->sd->lock);
	return size;
};

// TODO: this should be called whenever the device is removed from under /dev,
// not necessarily all the way once its RELEASED.
static void sdfile_free(Inode *inode)
{
	SDDeviceFile *fdev = (SDDeviceFile*) inode->fsdata;
	if (fdev->guidPath != NULL) devfsRemove(fdev->guidPath);
	kfree(fdev->guidPath);
	kfree(fdev);
};

static Inode* sdCreateInode(SDDeviceFile *fdev)
{
	Inode *inode = vfsCreateInode(NULL, VFS_MODE_BLKDEV | 0600);
	inode->fsdata = fdev;
	inode->open = sdfile_open;
	inode->close = sdfile_close;
	inode->pread = sdfile_pread;
	inode->pwrite = sdfile_pwrite;
	inode->flush = sdfile_flush;
	inode->ioctl = sdfile_ioctl;
	inode->pathctl = sdfile_pathctl;
	inode->getsize = sdfile_getsize;
	inode->free = sdfile_free;
	return inode;
};

#define CRCPOLY2 0xEDB88320UL  /* left-right reversal */

static uint32_t crc32(const void *data, size_t n)
{
	const uint8_t *c = (const uint8_t*) data;
	int i, j;
	uint32_t r;

	r = 0xFFFFFFFFUL;
	for (i = 0; i < n; i++)
	{
		r ^= c[i];
		for (j = 0; j < 8; j++)
			if (r & 1) r = (r >> 1) ^ CRCPOLY2;
			else       r >>= 1;
	}
	return r ^ 0xFFFFFFFFUL;
}

typedef struct
{
	uint32_t		a;
	uint16_t		b;
	uint16_t		c;
	uint8_t			d[2];
	uint8_t			e[6];
} GuidDisect;

static char* makeGuidLink(uint8_t *guid, const char *target)
{
	GuidDisect *disect = (GuidDisect*) guid;
	
	char linkpath[256];
	strformat(linkpath, 256, "guid/%8-%4-%4-%2%2-%2%2%2%2%2%2",
			(uint64_t) disect->a,
			(uint64_t) disect->b,
			(uint64_t) disect->c,
			(uint64_t) disect->d[0],
			(uint64_t) disect->d[1],
			(uint64_t) disect->e[0],
			(uint64_t) disect->e[1],
			(uint64_t) disect->e[2],
			(uint64_t) disect->e[3],
			(uint64_t) disect->e[4],
			(uint64_t) disect->e[5]
	);
	
	Inode *inode = vfsCreateInode(NULL, 0777 | VFS_MODE_LINK);
	inode->target = strdup(target);
	
	devfsRemove(linkpath);
	
	if (devfsAdd(linkpath, inode) != 0)
	{
		return NULL;
	}
	else
	{
		return strdup(linkpath);
	};
};

static void reloadGPT(StorageDevice *sd)
{
	GPTHeader header;
	sdRead(sd, 0x200, &header, sizeof(GPTHeader));
	
	if (header.sig != (*((const uint64_t*)"EFI PART")))
	{
		kprintf("gpt: invalid signature\n");
		return;
	};
	
	if (header.headerSize != 92)
	{
		kprintf("gpt: unsupported GPT header size\n");
		return;
	};
	
	uint32_t expectedCRC32 = header.headerCRC32;
	header.headerCRC32 = 0;
	uint32_t actualCRC32 = crc32(&header, header.headerSize);
	
	if (expectedCRC32 != actualCRC32)
	{
		kprintf("gpt: GPT header checksum error\n");
		return;
	};
	
	if (header.partEntSize != 128)
	{
		kprintf("gpt: unsupported partition entry size\n");
		return;
	};
	
	// create a link
	char devpath[256];
	strformat(devpath, 256, "/dev/sd%c", sd->letter);
	sd->guidPath = makeGuidLink(header.diskGUID, devpath);
	
	size_t tableSize = header.partEntSize * header.numPartEnts;
	GPTPartition *table = (GPTPartition*) kmalloc(tableSize);
	sdRead(sd, 512 * header.partListLBA, table, tableSize);
	
	if (crc32(table, tableSize) != header.partArrayCRC32)
	{
		kprintf("gpt: partition table checksum error\n");
		return;
	};
	
	int nextPartIndex = 0;
	unsigned int i;
	
	static uint8_t typeNone[16] = {0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0};
	for (i=0; i<header.numPartEnts; i++)
	{
		if (nextPartIndex == 32) break;
		
		if (memcmp(table[i].type, typeNone, 16) != 0)
		{
			SDDeviceFile *fdev = NEW(SDDeviceFile);
			memset(fdev, 0, sizeof(SDDeviceFile));
			fdev->sd = sd;
			fdev->offset = (uint64_t) table[i].startLBA * 512;
			fdev->size = (uint64_t) (table[i].endLBA - table[i].startLBA + 1) * 512;
			fdev->partIndex = nextPartIndex;
			
			sdUpref(sd);

			char devName[16];
			char devPath[256];
			strformat(devName, 16, "sd%c%d", sd->letter, nextPartIndex);
			strformat(devPath, 256, "/dev/sd%c%d", sd->letter, nextPartIndex);
			
			fdev->guidPath = makeGuidLink(table[i].partid, devPath);
			
			char *put = fdev->name;
			uint16_t *scan = table[i].partName;
			size_t count = 36;
			while (count--)
			{
				if (*scan == 0) break;
				
				*put++ = (char) (*scan++);
			};
			
			*put = 0;
			
			mutexLock(&sd->lock);
			
			Inode *inode = sdCreateInode(fdev);
			
			if (devfsAdd(devName, inode) != 0)
			{
				kfree(fdev);
			}
			else
			{
				nextPartIndex++;
			};
			
			mutexUnlock(&sd->lock);
		};
	};
	
	sd->numSubs = (size_t) nextPartIndex;
};

static void reloadPartTable(StorageDevice *sd)
{
	// delete current device files
	mutexLock(&sd->lock);
	size_t numRefs = sd->numSubs;
	
	int i;
	for (i=0; i<sd->numSubs; i++)
	{
		char subname[256];
		strformat(subname, 256, "sd%c%d", sd->letter, (int) i);
		devfsRemove(subname);
	};
	
	sd->numSubs = 0;

	while (numRefs--)
	{
		sdDownref(sd);
	};

	if (sd->guidPath != NULL) devfsRemove(sd->guidPath);
	kfree(sd->guidPath);
	sd->guidPath = NULL;
	mutexUnlock(&sd->lock);

	// load the new partition table
	MBRPartition mbrParts[4];
	sdRead(sd, 0x1BE, mbrParts, 64);
	
	// make sure the boot signature is there (indicating an MBR)
	uint16_t sig;
	sdRead(sd, 0x1FE, &sig, 2);
	if (sig != 0xAA55)
	{
		// not an MBR
		return;
	};
	
	// check for GPT
	int haveGPT = 0;
	for (i=0; i<4; i++)
	{
		if (mbrParts[i].systemID == 0xEE)
		{
			haveGPT = 1;
			break;
		};
	};
	
	if (haveGPT)
	{
		reloadGPT(sd);
		return;
	};
	
	mutexLock(&sd->lock);
	sd->numSubs = 0;
	mutexUnlock(&sd->lock);
	
	int nextSubIndex = 0;
	for (i=0; i<4; i++)
	{
		if (mbrParts[i].systemID != 0)
		{
			SDDeviceFile *fdev = NEW(SDDeviceFile);
			memset(fdev, 0, sizeof(SDDeviceFile));
			fdev->sd = sd;
			fdev->offset = (uint64_t) mbrParts[i].lbaStart * 512;
			fdev->size = (uint64_t) mbrParts[i].numSectors * 512;
			fdev->partIndex = i;
			
			sdUpref(sd);

			char devName[16];
			strformat(devName, 16, "sd%c%d", sd->letter, nextSubIndex);
			strformat(fdev->name, 16, "sd%c%d", sd->letter, nextSubIndex);
			
			mutexLock(&sd->lock);
			
			Inode *inode = sdCreateInode(fdev);
			
			if (devfsAdd(devName, inode) != 0)
			{
				kfree(fdev);
			}
			else
			{
				nextSubIndex++;
			};
			
			mutexUnlock(&sd->lock);
		};
	}; 
	
	sd->numSubs = (size_t) nextSubIndex;
};

static void sdFlushThread(void *context)
{
	// already upreffed for us
	StorageDevice *sd = (StorageDevice*) context;
	
	while (1)
	{
		uint64_t nanotimeout = NT_SECS(120);
		int status = semWaitGen(&sd->semFlush, 1, 0, nanotimeout);
		
		if (status == 1)
		{
			break;
		}
		else if (status == -ETIMEDOUT)
		{
			mutexLock(&sd->cacheLock);
			sdFlush(sd);
			mutexUnlock(&sd->cacheLock);
		};
	};
};

StorageDevice* sdCreate(SDParams *params, const char *name, SDOps *ops, void *drvdata)
{
	char letter = sdAllocLetter();
	if (letter == 0)
	{
		return NULL;
	};
	
	StorageDevice *sd = NEW(StorageDevice);
	if (sd == NULL)
	{
		sdFreeLetter(letter);
		return NULL;
	};
	
	sd->ops = ops;
	sd->drvdata = drvdata;
	
	mutexInit(&sd->lock);
	sd->refcount = 1;
	sd->flags = params->flags;
	sd->blockSize = params->blockSize;
	sd->totalSize = params->totalSize;
	sd->letter = letter;
	sd->numSubs = 0;
	sd->openParts = 0;
	semInit2(&sd->semFlush, 0);
	
	if (strlen(name) > 127)
	{
		memcpy(sd->name, name, 127);
		sd->name[127] = 0;
	}
	else
	{
		strcpy(sd->name, name);
	};
	
	sdUpref(sd);				// for the flush thread
	KernelThreadParams pars;
	memset(&pars, 0, sizeof(KernelThreadParams));
	pars.stackSize = DEFAULT_STACK_SIZE;
	pars.name = "SDI Flush Thread";
	sd->threadFlush = CreateKernelThread(sdFlushThread, &pars, sd);
	
	mutexInit(&sd->cacheLock);
	memset(&sd->cacheTop, 0, sizeof(BlockTreeNode));
	
	// master device file
	SDDeviceFile *fdev = NEW(SDDeviceFile);
	if (fdev == NULL)
	{
		sdFreeLetter(letter);
		kfree(sd);
		return NULL;
	};
	
	memset(fdev, 0, sizeof(SDDeviceFile));
	fdev->sd = sd;
	fdev->offset = 0;
	fdev->size = sd->totalSize;
	fdev->partIndex = -1;
	strcpy(fdev->name, sd->name);
	
	char masterName[16];
	strformat(masterName, 16, "sd%c", letter);
	
	mutexLock(&sd->lock);
	mutexLock(&sd->cacheLock);
	
	Inode *inode = sdCreateInode(fdev);
	
	if (devfsAdd(masterName, inode) != 0)
	{
		kfree(sd);
		kfree(fdev);
		sdFreeLetter(letter);
		vfsDownrefInode(inode);
		return NULL;
	};

	mutexUnlock(&sd->cacheLock);
	mutexUnlock(&sd->lock);
	
	mutexLock(&mtxList);
	sdList[letter-'a'] = sd;
	mutexUnlock(&mtxList);
	
	sd->guidPath = NULL;
	
	return sd;
};

void sdHangup(StorageDevice *sd)
{	
	mutexLock(&sd->lock);
	
	char masterName[256];
	strformat(masterName, 256, "sd%c", sd->letter);
	
	//DeleteDevice(sd->devMaster);
	devfsRemove(masterName);
	size_t numRefs = 1 + sd->numSubs;
	
	size_t i;
	for (i=0; i<sd->numSubs; i++)
	{
		//DeleteDevice(sd->devSubs[i]);
		
		char subname[256];
		strformat(subname, 256, "sd%c%d", sd->letter, (int) i);
		
		devfsRemove(subname);
	};
	
	//kfree(sd->devSubs);
	//sd->devSubs = NULL;
	sd->numSubs = 0;
	//sd->devMaster = NULL;
	sdFreeLetter(sd->letter);
	sd->letter = 0;

	sd->flags |= SD_HANGUP;
	semSignal(&sd->semFlush);
	ReleaseKernelThread(sd->threadFlush);
	mutexUnlock(&sd->lock);
	
	mutexLock(&mtxList);
	sdList[sd->letter-'a'] = NULL;
	mutexUnlock(&mtxList);
	
	while (numRefs--)
	{
		sdDownref(sd);
	};
};

void sdSync()
{
	mutexLock(&mtxList);
	
	size_t i;
	for (i=0; i<26; i++)
	{
		if (sdList[i] != NULL)
		{
			StorageDevice *sd = sdList[i];
			mutexLock(&sd->cacheLock);
			sdFlush(sd);
			mutexUnlock(&sd->cacheLock);
		};
	};
	
	mutexUnlock(&mtxList);
};

static uint64_t sdTryFree(StorageDevice *sd, BlockTreeNode *node, int level, uint64_t addr)
{
	while (1)
	{
		uint64_t i;
	
		uint64_t lowestUsage;
		uint64_t lowestIndex;
		int foundAny = 0;
	
		for (i=0; i<128; i++)
		{
			if (node->entries[i] != 0)
			{
				if (!foundAny)
				{
					lowestUsage = node->entries[i] >> 56;
					lowestIndex = i;
					foundAny = 1;
				}
				else if ((node->entries[i] >> 56) < lowestUsage)
				{
					lowestUsage = node->entries[i] >> 56;
					lowestIndex = i;
				};
			};
		};
	
		// we cached nothing at this level, report failure
		if (!foundAny)
		{
			return 0;
		};
	
		if (level == 6)
		{
			if (node->entries[lowestIndex] & SD_BLOCK_DIRTY)
			{
				uint64_t canaddr = (node->entries[lowestIndex] & 0xFFFFFFFFFFFF) | 0xFFFF800000000000;
				uint64_t bytepos = ((addr << 7) | lowestIndex) << 15;
				uint64_t startBlock = bytepos / sd->blockSize;
				uint64_t numBlocks = SD_TRACK_SIZE / sd->blockSize;
				sd->ops->writeBlocks(sd->drvdata, startBlock, numBlocks, (const void*) canaddr);
			};
		
			uint64_t canaddr = (node->entries[lowestIndex] & 0xFFFFFFFFFFFF) | 0xFFFF800000000000;

			node->entries[lowestIndex] = 0;
			
			uint64_t frames[8];
			unmapPhysMemoryAndGet((void*)canaddr, 0x8000, frames);
			
			int k;
			for (k=1; k<8; k++)
			{
				phmFreeFrame(frames[k]);
			};
			
			__sync_fetch_and_add(&phmCachedFrames, -8);
			
			return frames[0];
		}
		else
		{
			uint64_t canaddr = (node->entries[lowestIndex] & 0xFFFFFFFFFFFF) | 0xFFFF800000000000;
			uint64_t result = sdTryFree(sd, (BlockTreeNode*) canaddr, level+1, (addr << 7) | lowestIndex);
			
			if (result != 0)
			{
				return result;
			}
			else
			{
				kfree((void*)canaddr);
				node->entries[lowestIndex] = 0;
				// and try again
			};
		};
	};
};

static void sdDump(StorageDevice *sd, BlockTreeNode *node, int level, uint64_t addr)
{
	char indent[32];
	memset(indent, 0, 32);
	memset(indent, ' ', level+1);
	
	uint64_t i;
	for (i=0; i<128; i++)
	{
		if (level == 6)
		{
			kprintf("%sTRACK_ADDR[0x%016lX] = 0x%016lX ", indent, ((addr << 7) | i) << 15, node->entries[i]);
		}
		else
		{
			kprintf("%sEntry[%lu] = 0x%016lX ", indent, i, node->entries[i]);
		};

		if (level == 6)
		{
			if (node->entries[i] & SD_BLOCK_DIRTY)
			{
				kprintf("[DIRTY] ");
			};
			
			kprintf("\n");
		}
		else
		{
			kprintf("\n");
			
			if (node->entries[i] != 0)
			{
				uint64_t canaddr = (node->entries[i] & 0xFFFFFFFFFFFF) | 0xFFFF800000000000;
				sdDump(sd, (BlockTreeNode*) canaddr, level+1, (addr << 7) | i);
			};
		};
	};
};

uint64_t sdFreeMemory()
{
	mutexLock(&mtxList);
	uint64_t result = 0;
	
	int i;
	for (i=0; i<26; i++)
	{
		StorageDevice *sd = sdList[i];
		if (sd != NULL)
		{
			mutexLock(&sd->cacheLock);
			result = sdTryFree(sd, &sd->cacheTop, 0, 0);
			mutexUnlock(&sd->cacheLock);
		
			if (result != 0) break;
		};
	};
	
	mutexUnlock(&mtxList);
	return result;
};

void sdDumpInfo()
{
	kprintf("--- BLOCK DEVICE CACHE DUMP ---\n");
	int i;
	for (i=0; i<26; i++)
	{
		StorageDevice *sd = sdList[i];
		if (sd == NULL)
		{
			kprintf("Device 'sd%c' not mapped\n", (char)i+'a');
		}
		else
		{
			kprintf("Device 'sd%c' = %p; dumping:\n", (char)i+'a', sdList[i]);
			sdDump(sd, &sd->cacheTop, 0, 0);
		};
	};
};
