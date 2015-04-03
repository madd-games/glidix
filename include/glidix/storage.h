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

#ifndef __glidix_storage_h
#define __glidix_storage_h

#include <glidix/common.h>
#include <glidix/semaphore.h>
#include <glidix/spinlock.h>
#include <glidix/devfs.h>
#include <glidix/ioctl.h>

#define	SD_FILE_NO_BUF				0xFFFFFFFFFFFFFFFF

#define	IOCTL_SDI_IDENTITY			IOCTL_ARG(SDParams, IOCTL_INT_SDI, 0)

/**
 * Storage device flags.
 */
#define	SD_READONLY				(1 << 0)

/**
 * Command types.
 */
enum SDCommandType
{
	SD_CMD_READ,
	SD_CMD_WRITE
};

typedef struct _SDCommand
{
	enum SDCommandType			type;

	/**
	 * For write commands, the block pointer shall point to the area straight after
	 * the command structure, such that when kfree() is called on it, the block is
	 * also freed.
	 *
	 * For read commands, it points to an arbitrary area in memory to which the block
	 * will be stored once read from disk.
	 */
	void					*block;

	/**
	 * Index of the block on disk (0 = first block). This is like an LBA address.
	 */
	uint64_t				index;

	/**
	 * This is used to report when a read operation has been completed. To send a read command,
	 * this spinlock is first acquired by the sender. Next, the operations is pushed to the queue.
	 * Then, the sender acquires the lock again, effectively blocking himself. When the SDI driver
	 * completes the command, it shall release this lock, causing the sender to be unlocked.
	 */
	Spinlock*				cmdlock;

	/**
	 * For queues.
	 */
	struct _SDCommand*			next;
} SDCommand;

typedef struct
{
	/**
	 * The semaphore which controls access to this device.
	 */
	Semaphore				sem;

	/**
	 * Device flags.
	 */
	int					flags;

	/**
	 * Size of a block.
	 */
	size_t					blockSize;

	/**
	 * Total size of the drive, in bytes.
	 */
	size_t					totalSize;

	/**
	 * The devfs file associated with this storage.
	 */
	Device					diskfile;

	/**
	 * The kernel thread waiting for commands on this device.
	 */
	Thread*					thread;
	/**
	 * Command queue.
	 */
	SDCommand*				cmdq;
} StorageDevice;

typedef struct
{
	int					flags;
	size_t					blockSize;
	size_t					totalSize;
} SDParams;

/**
 * Represents a storage device in devfs.
 */
typedef struct _SDFile
{
	StorageDevice*				sd;
	Spinlock				masterLock;
	Spinlock				locks[4];
	int					index;		// partition index, -1 = whole drive
	Device					partdevs[4];	// devfs files belonging to partiions (or NULLs).
	struct _SDFile*				master;
	uint64_t				offset;
	uint64_t				limit;
	char					letter;
} SDFile;

/**
 * fsdata when a disk file is opened.
 */
typedef struct
{
	SDFile*					file;
	uint64_t				offset;		// in blocks
	uint64_t				limit;		// first block that CANNOT be accessed (relative to offset).
	uint8_t*				buf;		// stored right after the struct, kfreed() in one go.
	uint64_t				bufCurrent;	// if SD_FILE_NO_BUF, then no block has been buffered yet.
	off_t					pos;		// for seek.
	int					dirty;		// if 1 then the block must be flushed to disk later.
} DiskFile;

void		sdInit();
StorageDevice*	sdCreate(SDParams *params);
void		sdPush(StorageDevice *dev, SDCommand *cmd);
SDCommand*	sdTryPop(StorageDevice *dev);
SDCommand*	sdPop(StorageDevice *dev);
void		sdPostComplete(SDCommand *cmd);
int		sdRead(StorageDevice *dev, uint64_t block, void *buffer);
int		sdWrite(StorageDevice *dev, uint64_t block, const void *buffer);

#endif
