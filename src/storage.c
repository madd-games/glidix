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

#include <glidix/common.h>
#include <glidix/storage.h>
#include <glidix/memory.h>
#include <glidix/string.h>
#include <glidix/console.h>

/**
 * Bitmap of used drive letters (for /dev/sdX). Bit n represents letter 'a'+n,
 * for example bit 0 represents 'a', bit 1 represents 'b', etc. Note that there
 * are only 26 possible letters.
 */
static uint32_t sdLetters;

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
		while (sd->cmdq != NULL)
		{
			SDCommand *cmd = sd->cmdq;
			sd->cmdq = cmd->next;
			kfree(cmd);
		};
		
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

static void sdPush(StorageDevice *sd, SDCommand *cmd)
{
	// sd must already be locked!
	cmd->next = NULL;
	if (sd->cmdq == NULL)
	{
		sd->cmdq = cmd;
	}
	else
	{
		SDCommand *last = sd->cmdq;
		while (last->next != NULL) last = last->next;
		last->next = cmd;
	};
	
	semSignal(&sd->semCommands);
};

void sdInit()
{
	sdLetters = 0;
};

static int sdfile_ioctl(File *fp, uint64_t cmd, void *params)
{
	if (cmd == IOCTL_SDI_IDENTITY)
	{
		SDHandle *data = (SDHandle*) fp->fsdata;
		mutexLock(&data->sd->lock);
		SDParams *sdpars = (SDParams*) params;
		sdpars->flags = data->sd->flags;
		sdpars->blockSize = data->sd->blockSize;
		sdpars->totalSize = data->size;
		mutexUnlock(&data->sd->lock);
		return 0;
	}
	else if (cmd == IOCTL_SDI_EJECT)
	{
		SDHandle *data = (SDHandle*) fp->fsdata;
		if (data->sd->totalSize != 0)
		{
			// non-removable
			return -1;
		};
		
		Semaphore lock;
		semInit2(&lock, 0);

		SDCommand *cmd = (SDCommand*) kmalloc(sizeof(SDCommand));
		cmd->type = SD_CMD_EJECT;
		cmd->block = NULL;
		cmd->cmdlock = &lock;

		sdPush(data->sd, cmd);
		semWait(&lock);				// wait for the eject operation to finish
	
		// (cmd was freed by the driver)
		return 0;
	};

	return -1;
};

int sdfile_fstat(File *fp, struct stat *st)
{
	SDHandle *data = (SDHandle*) fp->fsdata;
	mutexLock(&data->sd->lock);
	st->st_dev = 0;
	st->st_ino = 0;
	st->st_mode = 0;
	st->st_nlink = data->sd->refcount;
	st->st_uid = 0;
	st->st_gid = 0;
	st->st_rdev = 0;
	st->st_size = data->sd->totalSize;
	st->st_blksize = data->sd->blockSize;
	st->st_blocks = st->st_size / st->st_blksize;
	st->st_atime = 0;
	st->st_mtime = 0;
	st->st_ctime = 0;
	
	if (data->sd->totalSize == 0)
	{
		Semaphore lock;
		semInit2(&lock, 0);

		SDCommand *cmd = (SDCommand*) kmalloc(sizeof(SDCommand));
		cmd->type = SD_CMD_GET_SIZE;
		cmd->block = &st->st_size;
		cmd->cmdlock = &lock;

		sdPush(data->sd, cmd);
		mutexUnlock(&data->sd->lock);
		semWait(&lock);		// cmd freed by the driver
	}
	else
	{
		mutexUnlock(&data->sd->lock);
	};
	
	return 0;
};

static void sdFlushPage(StorageDevice *sd, uint64_t pos, const void *buffer, int wait)
{	
	Semaphore semFinish;
	semInit2(&semFinish, 0);
	
	SDCommand *cmd = (SDCommand*) kmalloc(sizeof(SDCommand) + SD_PAGE_SIZE);
	cmd->type = SD_CMD_WRITE;
	cmd->block = &cmd[1];
	cmd->index = pos / sd->blockSize;
	cmd->count = SD_PAGE_SIZE / sd->blockSize;
	cmd->cmdlock = NULL;
	if (wait) cmd->cmdlock = &semFinish;
	cmd->next = NULL;
	
	memcpy(&cmd[1], buffer, SD_PAGE_SIZE);
	mutexLock(&sd->lock);
	sdPush(sd, cmd);
	mutexUnlock(&sd->lock);
	
	if (wait) semWait(&semFinish);
};

static void sdfile_fsync(File *fp)
{
	SDHandle *handle = (SDHandle*) fp->fsdata;
	mutexLock(&handle->sd->cacheLock);
	
	uint64_t i;
	for (i=0; i<SD_CACHE_PAGES; i++)
	{
		if ((handle->sd->cache[i].dirty) && (handle->sd->cache[i].offset != SD_CACHE_NONE))
		{
			sdFlushPage(handle->sd, handle->sd->cache[i].offset, handle->sd->cache[i].data, 1);
			handle->sd->cache[i].dirty = 0;
		};
	};
	
	mutexUnlock(&handle->sd->cacheLock);
};

static void sdfile_close(File *fp)
{
	sdfile_fsync(fp);
	
	SDHandle *handle = (SDHandle*) fp->fsdata;
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
		uint64_t page = pos & ~(SD_PAGE_SIZE-1);
		uint64_t offsetIntoPage = pos & (SD_PAGE_SIZE-1);
		uint64_t toRead = SD_PAGE_SIZE - offsetIntoPage;
		
		if (toRead > size)
		{
			toRead = size;
		};
		
		// see if this page is in the cache; otherwise load it
		mutexLock(&sd->cacheLock);
		
		uint64_t i;
		int found = 0;
		
		for (i=0; i<SD_CACHE_PAGES; i++)
		{
			if (sd->cache[i].offset == page)
			{
				if (sd->cache[i].importance != SD_IMPORTANCE_MAX)
				{
					sd->cache[i].importance++;
				};
				
				memcpy(put, &sd->cache[i].data[offsetIntoPage], toRead);
				found = 1;
				
				size -= toRead;
				put += toRead;
				pos += toRead;
				sizeRead += toRead;
			};
		};
		
		if (!found)
		{
			uint64_t leastImportant = 0;
			int lowestImportance = sd->cache[0].importance;
			
			// we need to read it from the disk; find the least important page
			for (i=1; i<SD_CACHE_PAGES; i++)
			{
				if (sd->cache[i].importance < lowestImportance)
				{
					leastImportant = i;
					lowestImportance = sd->cache[i].importance;
				};
			};
			
			if ((sd->cache[leastImportant].dirty) && (sd->cache[leastImportant].offset != SD_CACHE_NONE))
			{
				sdFlushPage(sd, sd->cache[leastImportant].offset, sd->cache[leastImportant].data, 0);
			};
			
			sd->cache[leastImportant].offset = page;
			sd->cache[leastImportant].dirty = 0;
			sd->cache[leastImportant].importance = 1;
			
			Semaphore semRead;
			semInit2(&semRead, 0);
			
			mutexLock(&sd->lock);
			SDCommand *cmd = (SDCommand*) kmalloc(sizeof(SDCommand));
			cmd->type = SD_CMD_READ;
			cmd->block = sd->cache[leastImportant].data;
			cmd->index = page / sd->blockSize;
			cmd->count = SD_PAGE_SIZE / sd->blockSize;
			cmd->cmdlock = &semRead;
			
			sdPush(sd, cmd);
			mutexUnlock(&sd->lock);
			
			semWait(&semRead);
		
			memcpy(put, &sd->cache[leastImportant].data[offsetIntoPage], toRead);
			
			size -= toRead;
			put += toRead;
			pos += toRead;
			sizeRead += toRead;
		};
		
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
		uint64_t page = pos & ~(SD_PAGE_SIZE-1);
		uint64_t offsetIntoPage = pos & (SD_PAGE_SIZE-1);
		uint64_t toWrite = SD_PAGE_SIZE - offsetIntoPage;
		
		if (toWrite > size)
		{
			toWrite = size;
		};
		
		// see if this page is in the cache; otherwise load it
		mutexLock(&sd->cacheLock);
		
		uint64_t i;
		int found = 0;
		
		for (i=0; i<SD_CACHE_PAGES; i++)
		{
			if (sd->cache[i].offset == page)
			{
				if (sd->cache[i].importance != SD_IMPORTANCE_MAX)
				{
					sd->cache[i].importance++;
				};
				
				sd->cache[i].dirty = 1;
				memcpy(&sd->cache[i].data[offsetIntoPage], scan, toWrite);
				found = 1;
				
				size -= toWrite;
				scan += toWrite;
				pos += toWrite;
				sizeWritten += toWrite;
			};
		};
		
		if (!found)
		{
			uint64_t leastImportant = 0;
			int lowestImportance = sd->cache[0].importance;
			
			// we need to read it from the disk; find the least important page
			for (i=1; i<SD_CACHE_PAGES; i++)
			{
				if (sd->cache[i].importance < lowestImportance)
				{
					leastImportant = i;
					lowestImportance = sd->cache[i].importance;
				};
			};
			
			if ((sd->cache[leastImportant].dirty) && (sd->cache[leastImportant].offset != SD_CACHE_NONE))
			{
				sdFlushPage(sd, sd->cache[leastImportant].offset, sd->cache[leastImportant].data, 0);
			};
			
			sd->cache[leastImportant].offset = page;
			sd->cache[leastImportant].dirty = 1;
			sd->cache[leastImportant].importance = 1;

			Semaphore semRead;
			semInit2(&semRead, 0);
			
			mutexLock(&sd->lock);
			SDCommand *cmd = (SDCommand*) kmalloc(sizeof(SDCommand));
			cmd->type = SD_CMD_READ;
			cmd->block = sd->cache[leastImportant].data;
			cmd->index = page / sd->blockSize;
			cmd->count = SD_PAGE_SIZE / sd->blockSize;
			cmd->cmdlock = &semRead;
			
			sdPush(sd, cmd);
			mutexUnlock(&sd->lock);
			semWait(&semRead);

			memcpy(&sd->cache[leastImportant].data[offsetIntoPage], scan, toWrite);
			
			size -= toWrite;
			scan += toWrite;
			pos += toWrite;
			sizeWritten += toWrite;
		};
		
		mutexUnlock(&sd->cacheLock);
	};

	return sizeWritten;
};

static ssize_t sdfile_pread(File *fp, void *buf, size_t size, off_t offset)
{
	SDHandle *handle = (SDHandle*) fp->fsdata;
	uint64_t actualStart = handle->offset + (uint64_t) offset;
	if (handle->size != 0)
	{
		if ((actualStart+size) > handle->size)
		{
			size = handle->size - actualStart;
		};
	};
	return sdRead(handle->sd, actualStart, buf, size);
};

static ssize_t sdfile_read(File *fp, void *buf, size_t size)
{
	SDHandle *handle = (SDHandle*) fp->fsdata;
	
	off_t offset = (off_t) handle->pos;
	ssize_t rsize = sdfile_pread(fp, buf, size, offset);
	if (rsize == -1) return -1;
	handle->pos = (size_t) (offset + rsize);
	return rsize;
};

static ssize_t sdfile_pwrite(File *fp, const void *buf, size_t size, off_t offset)
{
	SDHandle *handle = (SDHandle*) fp->fsdata;
	uint64_t actualStart = handle->offset + (uint64_t) offset;
	if (handle->size != 0)
	{
		if ((actualStart+size) > handle->size)
		{
			size = handle->size - actualStart;
		};
	};
	return sdWrite(handle->sd, actualStart, buf, size);
};

static ssize_t sdfile_write(File *fp, const void *buf, size_t size)
{
	SDHandle *handle = (SDHandle*) fp->fsdata;
	
	off_t offset = (off_t) handle->pos;
	ssize_t rsize = sdfile_pwrite(fp, buf, size, offset);
	if (rsize == -1) return -1;
	handle->pos = (size_t) (offset + rsize);
	return rsize;
};

static off_t sdfile_seek(File *fp, off_t off, int whence)
{
	SDHandle *handle = (SDHandle*) fp->fsdata;
	if (whence == SEEK_SET)
	{
		handle->pos = (size_t) off;
	}
	else if (whence == SEEK_CUR)
	{
		handle->pos += (size_t) off;
	}
	else if (whence == SEEK_END)
	{
		mutexLock(&handle->sd->lock);
		size_t end = handle->size;
		if (end == 0)
		{
			end = handle->sd->totalSize;
		};
		mutexUnlock(&handle->sd->lock);
		handle->pos = (size_t) end + off;
	};
	
	return (off_t) handle->pos;
};

static int sdfile_open(void *data, File *fp, size_t szfile)
{
	SDHandle *handle = NEW(SDHandle);
	if (handle == NULL)
	{
		return VFS_NO_MEMORY;
	};
	
	SDDeviceFile *fdev = (SDDeviceFile*) data;
	mutexLock(&fdev->sd->lock);
	if (fdev->partIndex == -1)
	{
		if (fdev->sd->openParts != 0)
		{
			mutexUnlock(&fdev->sd->lock);
			return VFS_BUSY;
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
			return VFS_BUSY;
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
	handle->pos = 0;
	handle->partIndex = fdev->partIndex;
	
	fp->fsdata = handle;
	fp->ioctl = sdfile_ioctl;
	fp->fstat = sdfile_fstat;
	fp->pread = sdfile_pread;
	fp->read = sdfile_read;
	fp->seek = sdfile_seek;
	fp->pwrite = sdfile_pwrite;
	fp->write = sdfile_write;
	fp->fsync = sdfile_fsync;
	fp->close = sdfile_close;
	
	return 0;
};

static void reloadPartTable(StorageDevice *sd)
{
	// this function is called while the parition files don't exist (so there is no need to free sd->devSubs).
	MBRPartition mbrParts[4];
	sdRead(sd, 0x1BE, mbrParts, 64);
	
	// we preallocate an array of 4 partition descriptions, even if they won't all be used
	mutexLock(&sd->lock);
	sd->devSubs = (Device*) kmalloc(sizeof(Device)*4);
	sd->numSubs = 0;
	mutexUnlock(&sd->lock);
	
	int i;
	int nextSubIndex = 0;
	for (i=0; i<4; i++)
	{
		if (mbrParts[i].systemID != 0)
		{
			SDDeviceFile *fdev = NEW(SDDeviceFile);
			fdev->sd = sd;
			fdev->offset = (uint64_t) mbrParts[i].lbaStart * 512;
			fdev->size = (uint64_t) mbrParts[i].numSectors * 512;
			fdev->partIndex = i;
			
			sdUpref(sd);

			char devName[16];
			strformat(devName, 16, "sd%c%d", sd->letter, nextSubIndex);
	
			mutexLock(&sd->lock);
			sd->devSubs[nextSubIndex] = AddDevice(devName, fdev, sdfile_open, 0600);
			if (sd->devMaster == NULL)
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
		uint64_t nanotimeout = NT_SECS(10);
		int status = semWaitGen(&sd->semFlush, 1, 0, nanotimeout);
		
		if (status == 0)
		{
			break;
		}
		else if (status == -ETIMEDOUT)
		{
			mutexLock(&sd->cacheLock);
	
			uint64_t i;
			for (i=0; i<SD_CACHE_PAGES; i++)
			{
				if ((sd->cache[i].dirty) && (sd->cache[i].offset != SD_CACHE_NONE))
				{
					sdFlushPage(sd, sd->cache[i].offset, sd->cache[i].data, 0);
					sd->cache[i].dirty = 0;
				};
				
				if (sd->cache[i].importance != 0)
				{
					sd->cache[i].importance--;
				};
			};
	
			mutexUnlock(&sd->cacheLock);
		};
	};
};

StorageDevice* sdCreate(SDParams *params)
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
	
	mutexInit(&sd->lock);
	sd->refcount = 1;
	sd->flags = params->flags;
	sd->blockSize = params->blockSize;
	sd->totalSize = params->totalSize;
	sd->letter = letter;
	sd->numSubs = 0;
	sd->devSubs = NULL;
	sd->cmdq = NULL;
	semInit2(&sd->semCommands, 0);
	sd->openParts = 0;
	semInit2(&sd->semFlush, 0);
	
	sdUpref(sd);				// for the flush thread
	KernelThreadParams pars;
	memset(&pars, 0, sizeof(KernelThreadParams));
	pars.stackSize = DEFAULT_STACK_SIZE;
	pars.name = "SDI Flush Thread";
	sd->threadFlush = CreateKernelThread(sdFlushThread, &pars, sd);
	
	mutexInit(&sd->cacheLock);
	
	uint64_t i;
	for (i=0; i<SD_CACHE_PAGES; i++)
	{
		sd->cache[i].offset = SD_CACHE_NONE;
		sd->cache[i].dirty = 0;
		sd->cache[i].importance = 0;
	};
	
	// master device file
	SDDeviceFile *fdev = NEW(SDDeviceFile);
	if (fdev == NULL)
	{
		sdFreeLetter(letter);
		kfree(sd);
		return NULL;
	};
	
	fdev->sd = sd;
	fdev->offset = 0;
	fdev->size = sd->totalSize;
	fdev->partIndex = -1;
	
	char masterName[16];
	strformat(masterName, 16, "sd%c", letter);
	
	mutexLock(&sd->lock);
	mutexLock(&sd->cacheLock);
	sd->devMaster = AddDevice(masterName, fdev, sdfile_open, 0600);
	if (sd->devMaster == NULL)
	{
		kfree(sd);
		kfree(fdev);
		sdFreeLetter(letter);
		return NULL;
	};

	mutexUnlock(&sd->cacheLock);
	mutexUnlock(&sd->lock);
	return sd;
};

void sdHangup(StorageDevice *sd)
{	
	mutexLock(&sd->lock);
	
	DeleteDevice(sd->devMaster);
	size_t numRefs = 1 + sd->numSubs;
	
	size_t i;
	for (i=0; i<sd->numSubs; i++)
	{
		DeleteDevice(sd->devSubs[i]);
	};
	
	kfree(sd->devSubs);
	sd->devSubs = NULL;
	sd->numSubs = 0;
	sd->devMaster = NULL;
	sdFreeLetter(sd->letter);
	sd->letter = 0;
	
	sd->flags |= SD_HANGUP;
	semSignal(&sd->semFlush);
	ReleaseKernelThread(sd->threadFlush);
	mutexUnlock(&sd->lock);
	
	while (numRefs--)
	{
		sdDownref(sd);
	};
};

SDCommand* sdPop(StorageDevice *sd)
{
	semWait(&sd->semCommands);
	
	mutexLock(&sd->lock);
	SDCommand *cmd = sd->cmdq;
	sd->cmdq = cmd->next;
	cmd->next = NULL;
	mutexUnlock(&sd->lock);
	
	return cmd;
};

void sdPostComplete(SDCommand *cmd)
{
	if (cmd->cmdlock != NULL) semSignal(cmd->cmdlock);
	kfree(cmd);
};
