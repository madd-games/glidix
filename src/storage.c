/*
	Glidix kernel

	Copyright (c) 2014, Madd Games.
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

static char nextDriveLetter = 'a';

void sdInit()
{
	// NOP
};

static void diskfile_close(File *fp)
{
	DiskFile *data = (DiskFile*) fp->fsdata;
	if (data->dirty)
	{
		sdWrite(data->file->sd, data->offset+data->bufCurrent, data->buf);
	};
	spinlockRelease(&data->file->lock);
	kfree(data);
};

static void diskfile_setblock(DiskFile *data, uint64_t block)
{
	if (data->bufCurrent != SD_FILE_NO_BUF)
	{
		if (data->dirty)
		{
			sdWrite(data->file->sd, data->offset+data->bufCurrent, data->buf);
		};
	};

	data->bufCurrent = block;
	sdRead(data->file->sd, block, data->buf);
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
			diskfile_setblock(data, block);
		};

		*put++ = data->buf[offset];
		data->pos++;
		outsize++;
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

static int diskfile_open(void *_diskfile_void, File *fp, size_t szFile)
{
	SDFile *diskfile = (SDFile*) _diskfile_void;
	if (spinlockTry(&diskfile->lock))
	{
		return VFS_BUSY;
	};

	DiskFile *data = (DiskFile*) kmalloc(sizeof(DiskFile)+diskfile->sd->blockSize);
	data->file = diskfile;
	data->offset = 0;
	data->limit = diskfile->sd->totalSize / diskfile->sd->blockSize;
	data->buf = (uint8_t*) &data[1];
	data->bufCurrent = SD_FILE_NO_BUF;
	data->pos = 0;
	data->dirty = 0;

	fp->fsdata = data;
	fp->read = diskfile_read;
	fp->seek = diskfile_seek;
	fp->close = diskfile_close;

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

	SDFile *diskfile = (SDFile*) kmalloc(sizeof(SDFile));
	diskfile->sd = sd;
	spinlockRelease(&diskfile->lock);

	char name[4] = "sd_";
	name[2] = nextDriveLetter++;
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

	semSignal(&dev->sem);
};

SDCommand* sdTryPop(StorageDevice *dev)
{
	semWait(&dev->sem);
	if (dev->cmdq == NULL)
	{
		semSignal(&dev->sem);
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
