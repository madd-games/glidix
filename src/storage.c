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

#include <glidix/storage.h>
#include <glidix/memory.h>
#include <glidix/string.h>
#include <glidix/console.h>
#include <glidix/time.h>

static char nextDriveLetter = 'a';

typedef struct
{
	uint8_t							flags;
	uint8_t							sig1;			// 0x14
	uint16_t						startHigh;
	uint8_t							systemID;
	uint8_t							sig2;			// 0xEB
	uint16_t						lenHigh;
	uint32_t						startLow;
	uint32_t						lenLow;
} __attribute__ ((packed)) Partition;

typedef struct
{
	uint8_t							bootloader[446];
	Partition						parts[4];
	uint8_t							bootsig[2];
} __attribute__ ((packed)) MBR;

static int diskfile_open(void *_diskfile_void, File *fp, size_t szFile);

static Semaphore deleterLock;
static Semaphore deleterCounter;
static Device devsToDelete[4];

static void waitForDeleter()
{
	while (1)
	{
		semWait(&deleterLock);
		int i;
		int found = 0;
		for (i=0; i<4; i++)
		{
			if (devsToDelete[i] != NULL)
			{
				semSignal(&deleterLock);
				found = 1;
				break;
			};
		};
		if (found) continue;
		else break;
	};
};

static void signalDeleter()
{
	semSignal(&deleterLock);
};

static void sdThread(void *data)
{
	while (1)
	{
		semWait(&deleterCounter);
		semWait(&deleterLock);
		int i;
		for (i=0; i<4; i++)
		{
			Device dev = devsToDelete[i];
			if (dev != NULL)
			{
				DeleteDevice(dev);
				devsToDelete[i] = NULL;
			};
		};
		semSignal(&deleterLock);
	};
};

void sdInit()
{
	semInit(&deleterLock);
	semInit2(&deleterCounter, 0);
	
	int i;
	for (i=0; i<4; i++)
	{
		devsToDelete[i] = NULL;
	};
	KernelThreadParams sdPars;
	memset(&sdPars, 0, sizeof(KernelThreadParams));
	sdPars.stackSize = DEFAULT_STACK_SIZE;
	sdPars.name = "SDI control thread";
	CreateKernelThread(sdThread, &sdPars, NULL);
};

static void reloadPartitionTable(SDFile *sdfile)
{
	if (sdfile->sd->totalSize == 0) return;
	MBR *mbr = (MBR*) kmalloc(sdfile->sd->blockSize);
	sdRead(sdfile->sd, 0, mbr);
	//kprintf("sdi: reading partition table for /dev/sd%c\n", sdfile->letter);
	
	int i;
	for (i=0; i<4; i++)
	{
		Partition *part = &mbr->parts[i];
		//kprintf("SIG1: %a, SIG2: %a, SYSID: %a\n", (uint64_t)part->sig1, (uint64_t)part->sig2, (uint64_t)part->systemID);
		if ((part->sig1 == 0x14) && (part->sig2 == 0xEB) && (part->systemID != 0))
		{
			uint64_t offset = ((uint64_t)part->startHigh << 32) + (uint64_t)part->startLow;
			uint64_t limit = offset + ((uint64_t)part->lenHigh << 32) + (uint64_t)part->lenLow;
			//kprintf("sdi: partition %d, offset %a, size %a\n", i, offset, limit);

			SDFile *diskfile = (SDFile*) kmalloc(sizeof(SDFile));
			diskfile->sd = sdfile->sd;
			spinlockRelease(&diskfile->masterLock);
			diskfile->index = i;
			diskfile->master = sdfile;
			diskfile->offset = offset;
			diskfile->limit = limit;
			diskfile->letter = sdfile->letter;

			char name[5];
			strcpy(name, "sd__");
			name[2] = diskfile->letter;
			name[3] = (char)i + '0';

			sdfile->partdevs[i] = AddDevice(name, diskfile, diskfile_open, 0600);
		};
	};

	kfree(mbr);
};

#ifdef SD_CACHE_ENABLE
static void doCachedWrite(StorageDevice *sd, uint64_t block, const void *data)
{
	uint64_t page = block & ~(sd->blocksPerPage-1);
	uint64_t offset = (block & (sd->blocksPerPage-1)) * sd->blockSize;
	
	int index = sd->cacheIndex;
	
	int i;
	for (i=0; i<SD_CACHE_SIZE; i++)
	{
		if (index == 0) index = SD_CACHE_SIZE;
		index--;
		
		if (sd->cache[index].index == page)
		{
			memcpy(&sd->cache[index].data[offset], data, sd->blockSize);
		};
	};
	
	sdWrite(sd, block, data);
};
#endif

static void diskfile_fsync(File *fp)
{
	DiskFile *data = (DiskFile*) fp->fsdata;
	if (data->dirty)
	{
#ifdef SD_CACHE_ENABLE
		doCachedWrite(data->file->sd, data->offset+data->bufCurrent, data->buf);
#else
		sdWrite(data->file->sd, data->offset+data->bufCurrent, data->buf);
#endif
		data->dirty = 0;
	};
};

static void diskfile_close(File *fp)
{
	DiskFile *data = (DiskFile*) fp->fsdata;

	diskfile_fsync(fp);
	int idx = data->file->index;
	if (idx != -1)
	{
		spinlockRelease(&data->file->master->locks[idx]);
	}
	else
	{
		reloadPartitionTable(data->file);
		spinlockRelease(&data->file->masterLock);
	};
	kfree(data);
};

static void diskfile_setblock(DiskFile *data, uint64_t block, int shouldRead)
{
#ifdef SD_CACHE_ENABLE
	block += data->offset;
	if (data->bufCurrent != SD_FILE_NO_BUF)
	{
		if (data->dirty)
		{
			doCachedWrite(data->file->sd, data->offset+data->bufCurrent, data->buf);
		};
	};

	uint64_t page = block & ~(data->file->sd->blocksPerPage-1);
	uint64_t offset = (block & (data->file->sd->blocksPerPage-1)) * data->file->sd->blockSize;
	int cacheLoaded = 0;
	char pagebuf[4096];
	
	int index = data->file->sd->cacheIndex;
	int it;
	for (it=0; it<SD_CACHE_SIZE; it++)
	{
		if (index == 0) index = SD_CACHE_SIZE;
		index--;
		
		if (data->file->sd->cache[index].index == page)
		{
			memcpy(pagebuf, data->file->sd->cache[index].data, 4096);
			cacheLoaded = 1;
			break;
		};
	};
	
	if (!cacheLoaded)
	{
		//sdRead(data->file->sd, block, data->buf);
		sdRead2(data->file->sd, page, pagebuf, data->file->sd->blocksPerPage);
	};
	
	data->file->sd->cache[data->file->sd->cacheIndex].index = page;
	memcpy(data->file->sd->cache[data->file->sd->cacheIndex].data, pagebuf, 4096);
	data->file->sd->cacheIndex = (data->file->sd->cacheIndex+1) % SD_CACHE_SIZE;
	
	memcpy(data->buf, pagebuf + offset, data->file->sd->blockSize);
	data->bufCurrent = block - data->offset;
#else
	// no caching
	if (data->bufCurrent != SD_FILE_NO_BUF)
	{
		if (data->dirty)
		{
			sdWrite(data->file->sd, data->bufCurrent + data->offset, data->buf);
		};
	};
	
	sdRead(data->file->sd, block + data->offset, data->buf);
	data->bufCurrent = block;
#endif
};

static ssize_t diskfile_read(File *fp, void *buffer, size_t size)
{
	DiskFile *data = (DiskFile*) fp->fsdata;
	//kprintf_debug("FP: %p, DATA: %p, BUFFER: %p, SIZE: %p, BUF: %p\n", fp, data, buffer, size, data->buf);
	
	ssize_t outsize = 0;
	uint8_t *put = (uint8_t*) buffer;
	while (size--)
	{
		uint64_t block = data->pos / data->file->sd->blockSize;
		uint64_t offset = data->pos % data->file->sd->blockSize;

		if (block >= data->limit)
		{
			break;
		};

		if (data->bufCurrent != block)
		{
			diskfile_setblock(data, block, 1);
		};

		*put++ = data->buf[offset];
		data->pos++;
		outsize++;
	};

	return outsize;
};

static ssize_t diskfile_write(File *fp, const void *buffer, size_t size)
{
	DiskFile *data = (DiskFile*) fp->fsdata;

	ssize_t outsize = 0;
	const uint8_t *scan = (const uint8_t*) buffer;
	while (size--)
	{
		uint64_t block = data->pos / data->file->sd->blockSize;
		uint64_t offset = data->pos % data->file->sd->blockSize;

		if (block >= data->limit)
		{
			break;
		};

		if (data->bufCurrent != block)
		{
			diskfile_setblock(data, block, (size+1)<data->file->sd->blockSize);
		};

		data->buf[offset] = *scan++;
		data->pos++;
		outsize++;
		data->dirty = 1;
	};

	return outsize;
};

static off_t diskfile_seek(File *fp, off_t offset, int whence)
{
	DiskFile *data = (DiskFile*) fp->fsdata;
	off_t end = data->limit * data->file->sd->blockSize;

	switch (whence)
	{
	case SEEK_SET:
		data->pos = offset;
		break;
	case SEEK_CUR:
		data->pos += offset;
		break;
	case SEEK_END:
		data->pos = end + offset;
		break;
	};

	if (data->pos < 0) data->pos = 0;
	if (data->pos > end) data->pos = end;

	return data->pos;
};

static int diskfile_ioctl(File *fp, uint64_t cmd, void *params)
{
	if (cmd == IOCTL_SDI_IDENTITY)
	{
		DiskFile *data = (DiskFile*) fp->fsdata;
		SDParams *sdpars = (SDParams*) params;
		sdpars->flags = data->file->sd->flags;
		sdpars->blockSize = data->file->sd->blockSize;
		sdpars->totalSize = (data->file->limit - data->file->offset) * sdpars->blockSize;
		return 0;
	}
	else if (cmd == IOCTL_SDI_EJECT)
	{
		DiskFile *data = (DiskFile*) fp->fsdata;
		if (data->file->sd->totalSize != 0)
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

		sdPush(data->file->sd, cmd);
		semWait(&lock);				// wait for the eject operation to finish
	
		// (cmd was freed by the driver)
		return 0;
	};

	return -1;
};

int diskfile_fstat(File *fp, struct stat *st)
{
	DiskFile *data = (DiskFile*) fp->fsdata;
	st->st_dev = 0;
	st->st_ino = 0;
	st->st_mode = 0;
	st->st_nlink = 1;
	st->st_uid = 0;
	st->st_gid = 0;
	st->st_rdev = 0;
	st->st_size = (data->file->limit - data->file->offset) * data->file->sd->blockSize;
	st->st_blksize = data->file->sd->blockSize;
	st->st_blocks = st->st_size / st->st_blksize;
	st->st_atime = 0;
	st->st_mtime = 0;
	st->st_ctime = 0;
	
	if (data->file->sd->blockSize == 0)
	{
		Semaphore lock;
		semInit2(&lock, 0);

		SDCommand *cmd = (SDCommand*) kmalloc(sizeof(SDCommand));
		cmd->type = SD_CMD_GET_SIZE;
		cmd->block = &st->st_size;
		cmd->cmdlock = &lock;

		sdPush(data->file->sd, cmd);
		semWait(&lock);		// cmd freed by the driver
	};
	
	return 0;
};

static int diskfile_open(void *_diskfile_void, File *fp, size_t szFile)
{
	SDFile *diskfile = (SDFile*) _diskfile_void;
	if (diskfile->index != -1)
	{
		if (spinlockTry(&diskfile->master->locks[diskfile->index]))
		{
			return VFS_BUSY;
		};
	}
	else
	{
		if (spinlockTry(&diskfile->masterLock))
		{
			return VFS_BUSY;
		};

		int i;
		waitForDeleter();
		for (i=0; i<4; i++)
		{
			spinlockAcquire(&diskfile->master->locks[i]);
			if (diskfile->partdevs[i] != NULL)
			{
				devsToDelete[i] = diskfile->partdevs[i];
				diskfile->partdevs[i] = NULL;
			}
			else
			{
				devsToDelete[i] = NULL;
			};
			spinlockRelease(&diskfile->master->locks[i]);
		};
		signalDeleter();
		semSignal(&deleterCounter);
	};

	DiskFile *data = (DiskFile*) kmalloc(sizeof(DiskFile)+diskfile->sd->blockSize);
	data->file = diskfile;
	data->offset = diskfile->offset;
	data->limit = diskfile->limit;
	if (diskfile->sd->totalSize == 0)
	{
		off_t totalSize;
		Semaphore lock;
		semInit2(&lock, 0);

		SDCommand *cmd = (SDCommand*) kmalloc(sizeof(SDCommand));
		cmd->type = SD_CMD_GET_SIZE;
		cmd->block = &totalSize;
		cmd->cmdlock = &lock;

		sdPush(diskfile->sd, cmd);
		semWait(&lock);		// cmd freed by the driver
		data->limit = (uint64_t) totalSize / diskfile->sd->blockSize;
	};
	data->buf = (uint8_t*) &data[1];
	data->bufCurrent = SD_FILE_NO_BUF;
	data->pos = 0;
	data->dirty = 0;

	fp->fsdata = data;
	fp->read = diskfile_read;
	if ((diskfile->sd->flags & SD_READONLY) == 0) fp->write = diskfile_write;
	fp->seek = diskfile_seek;
	fp->close = diskfile_close;
	fp->ioctl = diskfile_ioctl;
	fp->fsync = diskfile_fsync;
	fp->fstat = diskfile_fstat;

	return 0;
};

StorageDevice *sdCreate(SDParams *params)
{
	StorageDevice *sd = (StorageDevice*) kmalloc(sizeof(StorageDevice));
	semInit(&sd->sem);
	semInit2(&sd->cmdCounter, 0);
	
	sd->flags = params->flags;
	sd->blockSize = params->blockSize;
	sd->totalSize = params->totalSize;
	sd->cmdq = NULL;
	sd->thread = NULL;

	SDFile *diskfile = (SDFile*) kmalloc(sizeof(SDFile));
	diskfile->sd = sd;
	spinlockRelease(&diskfile->masterLock);
	int i;
	for (i=0; i<4; i++)
	{
		spinlockRelease(&diskfile->locks[i]);
		diskfile->partdevs[i] = NULL;
	};

	diskfile->index = -1;
	diskfile->master = diskfile;
	diskfile->offset = 0;
	diskfile->limit = sd->totalSize / sd->blockSize;
	diskfile->letter = nextDriveLetter++;

	memset(sd->cache, 0xFF, sizeof(SDCachedPage)*SD_CACHE_SIZE);		// set all to SD_NOT_CACHED (all 0xFF)
	sd->cacheIndex = 0;
	sd->blocksPerPage = 4096 / sd->blockSize;

	char name[4] = "sd_";
	name[2] = diskfile->letter;
	sd->diskfile = AddDevice(name, diskfile, diskfile_open, 0600);
	
	return sd;
};

void sdPush(StorageDevice *dev, SDCommand *cmd)
{
	cmd->next = NULL;
	semWait(&dev->sem);

	if (dev->cmdq == NULL)
	{
		dev->cmdq = cmd;
	}
	else
	{
		SDCommand *last = dev->cmdq;
		while (last->next != NULL) last = last->next;
		last->next = cmd;
	};

	semSignal(&dev->cmdCounter);
	semSignal(&dev->sem);
};

SDCommand* sdPop(StorageDevice *sd)
{
	semWait(&sd->cmdCounter);
	
	semWait(&sd->sem);
	SDCommand *cmd = sd->cmdq;
	sd->cmdq = cmd->next;
	semSignal(&sd->sem);
	
	cmd->next = NULL;
	return cmd;
};

void sdPostComplete(SDCommand *cmd)
{
	if (cmd->cmdlock != NULL) semSignal(cmd->cmdlock);
	kfree(cmd);
};

int sdRead2(StorageDevice *dev, uint64_t block, void *buffer, uint64_t count)
{
	Semaphore lock;
	semInit2(&lock, 0);

	SDCommand *cmd = (SDCommand*) kmalloc(sizeof(SDCommand));
	cmd->type = SD_CMD_READ;
	cmd->block = buffer;
	cmd->index = block;
	cmd->count = count;
	cmd->cmdlock = &lock;

	sdPush(dev, cmd);
	semWait(&lock);				// wait for the read operation to finish
	
	// (cmd was freed by the driver)
	return 0;
};

int sdRead(StorageDevice *dev, uint64_t block, void *buffer)
{
	return sdRead2(dev, block, buffer, 1);
};

int sdWrite(StorageDevice *dev, uint64_t block, const void *buffer)
{
	if (dev->blockSize*(block+1) > dev->totalSize) return -1;
	if (dev->flags & SD_READONLY) return -1;

	//if (block == 0) kprintf("sdWrite %p\n", block);
	
	Semaphore lock;
	semInit2(&lock, 0);
	
	SDCommand *cmd = (SDCommand*) kmalloc(sizeof(SDCommand) + dev->blockSize);
	cmd->type = SD_CMD_WRITE;
	cmd->block = &cmd[1];			// the block will be stored right after the struct.
	cmd->index = block;
	cmd->count = 1;
	cmd->cmdlock = &lock;
	memcpy(&cmd[1], buffer, dev->blockSize);

	sdPush(dev, cmd);
	semWait(&lock);
	return 0;
};
