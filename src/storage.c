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

#include <glidix/storage.h>
#include <glidix/memory.h>
#include <glidix/string.h>
#include <glidix/console.h>

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
	int i;
	for (i=0; i<4; i++)
	{
		devsToDelete[i] = NULL;
	};
	KernelThreadParams sdPars;
	sdPars.stackSize = DEFAULT_STACK_SIZE;
	sdPars.name = "SDI control daemon";
	CreateKernelThread(sdThread, &sdPars, NULL);
};

static void reloadPartitionTable(SDFile *sdfile)
{
	MBR *mbr = (MBR*) kmalloc(sdfile->sd->blockSize);
	sdRead(sdfile->sd, 0, mbr);

	int i;
	for (i=0; i<4; i++)
	{
		Partition *part = &mbr->parts[i];
		if ((part->flags & 1) && (part->sig1 == 0x14) && (part->sig2 == 0xEB) && (part->systemID != 0))
		{
			uint64_t offset = ((uint64_t)part->startHigh << 32) + (uint64_t)part->startLow;
			uint64_t limit = offset + ((uint64_t)part->lenHigh << 32) + (uint64_t)part->lenLow;
			kprintf_debug("sdi: partition %d, offset %a, size %a\n", i, offset, limit);

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

			sdfile->partdevs[i] = AddDevice(name, diskfile, diskfile_open, 0);
		};
	};

	kfree(mbr);
};

static void diskfile_close(File *fp)
{
	DiskFile *data = (DiskFile*) fp->fsdata;
	if (data->dirty)
	{
		sdWrite(data->file->sd, data->offset+data->bufCurrent, data->buf);
	};
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

static void diskfile_fsync(File *fp)
{
	DiskFile *data = (DiskFile*) fp->fsdata;
	if (data->dirty)
	{
		sdWrite(data->file->sd, data->offset+data->bufCurrent, data->buf);
		data->dirty = 0;
	};
};

static void diskfile_setblock(DiskFile *data, uint64_t block, int shouldRead)
{
	if (data->bufCurrent != SD_FILE_NO_BUF)
	{
		if (data->dirty)
		{
			sdWrite(data->file->sd, data->offset+data->bufCurrent, data->buf);
		};
	};

	data->bufCurrent = block;
	// TODO: the condition for shouldRead causes it to fail and corrupt disk data if the writing starts
	// on a weird boundary.
	/*if (shouldRead)*/ sdRead(data->file->sd, data->offset+block, data->buf);
};

static ssize_t diskfile_read(File *fp, void *buffer, size_t size)
{
	DiskFile *data = (DiskFile*) fp->fsdata;

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
	};

	return -1;
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
	};

	DiskFile *data = (DiskFile*) kmalloc(sizeof(DiskFile)+diskfile->sd->blockSize);
	data->file = diskfile;
	//data->offset = 0;
	//data->limit = diskfile->sd->totalSize / diskfile->sd->blockSize;
	data->offset = diskfile->offset;
	data->limit = diskfile->limit;
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

	return 0;
};

StorageDevice *sdCreate(SDParams *params)
{
	StorageDevice *sd = (StorageDevice*) kmalloc(sizeof(StorageDevice));
	semInit(&sd->sem);
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

	char name[4] = "sd_";
	name[2] = diskfile->letter;
	sd->diskfile = AddDevice(name, diskfile, diskfile_open, 0);
	
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

	if (dev->thread != NULL)
	{
		signalThread(dev->thread);
		dev->thread = NULL;
	};

	semSignal(&dev->sem);
};

SDCommand* sdTryPop(StorageDevice *dev)
{
	semWait(&dev->sem);
	if (dev->cmdq == NULL)
	{
		dev->thread = getCurrentThread();
		semSignalAndWait(&dev->sem);
		return NULL;
	};

	SDCommand *cmd = dev->cmdq;
	dev->cmdq = cmd->next;
	semSignal(&dev->sem);

	cmd->next = NULL;
	return cmd;
};

SDCommand* sdPop(StorageDevice *dev)
{
	while (1)
	{
		SDCommand *cmd = sdTryPop(dev);
		if (cmd != NULL) return cmd;
	};

	// never reached, but the compiler would still moan.
	return NULL;
};

void sdPostComplete(SDCommand *cmd)
{
	if (cmd->cmdlock != NULL) spinlockRelease(cmd->cmdlock);
	kfree(cmd);
};

int sdRead(StorageDevice *dev, uint64_t block, void *buffer)
{
	if (dev->blockSize*(block+1) > dev->totalSize) return -1;

	Spinlock lock;
	spinlockRelease(&lock);
	spinlockAcquire(&lock);

	SDCommand *cmd = (SDCommand*) kmalloc(sizeof(SDCommand));
	cmd->type = SD_CMD_READ;
	cmd->block = buffer;
	cmd->index = block;
	cmd->cmdlock = &lock;

	sdPush(dev, cmd);
	spinlockAcquire(&lock);				// wait for the read operation to finish

	// (cmd was freed by the driver)
	return 0;
};

int sdWrite(StorageDevice *dev, uint64_t block, const void *buffer)
{
	if (dev->blockSize*(block+1) > dev->totalSize) return -1;
	if (dev->flags & SD_READONLY) return -1;

	SDCommand *cmd = (SDCommand*) kmalloc(sizeof(SDCommand) + dev->blockSize);
	cmd->type = SD_CMD_WRITE;
	cmd->block = &cmd[1];			// the block will be stored right after the struct.
	cmd->index = block;
	cmd->cmdlock = NULL;
	memcpy(&cmd[1], buffer, dev->blockSize);

	sdPush(dev, cmd);
	return 0;
};
