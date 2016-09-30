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

#ifndef __glidix_storage_h
#define __glidix_storage_h

#include <glidix/common.h>
#include <glidix/semaphore.h>
#include <glidix/devfs.h>
#include <glidix/ioctl.h>
#include <glidix/mutex.h>
#include <glidix/sched.h>

#define	IOCTL_SDI_IDENTITY			IOCTL_ARG(SDParams, IOCTL_INT_SDI, 0)
#define	IOCTL_SDI_EJECT				IOCTL_NOARG(IOCTL_INT_SDI, 1)

/**
 * Storage device flags.
 */
#define	SD_READONLY				(1 << 0)
#define	SD_HANGUP				(1 << 1)

/**
 * The value for 'openParts' which indicates the master file is open.
 */
#define	SD_MASTER_OPEN				0xFFFFFFFFFFFFFFFF

/**
 * Size of a single cache page.
 */
#define	SD_PAGE_SIZE				0x1000

/**
 * Number of pages to cache per device.
 */
#define	SD_CACHE_PAGES				64

/**
 * Offset value indicating a cache page is unused.
 */
#define	SD_CACHE_NONE				0xFFFFFFFFFFFFFFFF

/**
 * Maximum allowed cache page importance.
 */
#define	SD_IMPORTANCE_MAX			32

/**
 * Command types.
 */
enum SDCommandType
{
	SD_CMD_READ,
	SD_CMD_WRITE,
	SD_CMD_EJECT,
	SD_CMD_GET_SIZE,
	SD_CMD_SIGNAL,
};

/**
 * MBR partition entry.
 */
typedef struct
{
	uint8_t					flags;
	uint8_t					startHead;
	uint16_t				startCylinderSector;
	uint8_t					systemID;
	uint8_t					endHead;
	uint16_t				endCylinderSector;
	uint32_t				lbaStart;
	uint32_t				numSectors;
} PACKED MBRPartition;

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
	 *
	 * For eject commands, it must be NULL.
	 *
	 * For getsize commands, points to an off_t where the size is to be stored.
	 */
	void*					block;

	/**
	 * Index of the block on disk (0 = first block). This is like an LBA address.
	 */
	uint64_t				index;

	/**
	 * Number of blocks to read/write.
	 */
	uint64_t				count;

	/**
	 * Semaphore used to decice when an operation is complete.
	 */
	Semaphore*				cmdlock;

	/**
	 * Status, set by default to 0 by the kernel, the driver sets this to -1 if it can't execute
	 * a command.
	 */
	int					status;
	
	/**
	 * For queues.
	 */
	struct _SDCommand*			next;
} SDCommand;

typedef struct
{
	/**
	 * Byte offset into the disk, where this cache page starts. Must be 4KB aligned.
	 * 
	 */
	uint64_t				offset;
	
	/**
	 * If nonzero, this page was modified since it was read from disk
	 * (and so must be flushed).
	 */
	int					dirty;
	
	/**
	 * The importance of this cache page.
	 */
	int					importance;
	
	/**
	 * The actual data.
	 */
	uint8_t					data[SD_PAGE_SIZE];
} SDCachePage;

typedef struct
{
	/**
	 * The mutex which controls access to this device.
	 */
	Mutex					lock;
	
	/**
	 * Reference counter. The storage device is deleted once this reaches zero.
	 * Here's how this works: each file under /dev that refers to this file counts
	 * as a reference; that is the master file as well as any partition files. The
	 * driver which manages this device does NOT count as a reference - the reason
	 * for this is that when the driver starts, the master file is created, and when
	 * the driver hangs up, the master file is deleted; so the master file represents
	 * a reference for the driver. Access this atomically (without the lock).
	 */
	int					refcount;
	
	/**
	 * Device flags.
	 */
	int					flags;
	
	/**
	 * The size of a single block (in bytes) and the total size of the device (in bytes).
	 */
	size_t					blockSize;
	size_t					totalSize;
	
	/**
	 * Device letter (a-z), needs to be freed when we hang up.
	 */
	char					letter;
	
	/**
	 * Master device file (which represents the whole disk).
	 */
	Device					devMaster;
	
	/**
	 * Subordinate device files (sub-files) which represent partitions, and the number of them.
	 */
	size_t					numSubs;
	Device*					devSubs;
	
	/**
	 * Command queue.
	 */
	SDCommand*				cmdq;
	
	/**
	 * The semaphore that counts the number of commands waiting to be executed.
	 */
	Semaphore				semCommands;
	
	/**
	 * A bitmap of which partitions have been opened; attempting to open an already-opened partition
	 * leads to the EBUSY error being returned. No bit is reserved for the master file; instead,
	 * opening the master file demands that ALL bits are CLEAR, and when the master is opened, all
	 * bits become set, and all partitions are actually deleted. Currently, up to 64 partitions are
	 * supported.
	 */
	uint64_t				openParts;
	
	/**
	 * The thread which flushes the drive every 10-20 seconds, and a semaphore which is signalled when
	 * the thread should terminate.
	 */
	Thread*					threadFlush;
	Semaphore				semFlush;
	
	/**
	 * This mutex protects the cache.
	 */
	Mutex					cacheLock;
	
	/**
	 * The cache.
	 */
	SDCachePage				cache[SD_CACHE_PAGES];
} StorageDevice;

typedef struct
{
	int					flags;
	size_t					blockSize;
	size_t					totalSize;
} SDParams;

typedef struct
{
	StorageDevice*				sd;
	size_t					offset;
	size_t					size;
	int					partIndex;		// -1 = master
} SDDeviceFile;

typedef struct
{
	StorageDevice*				sd;
	size_t					offset;
	size_t					size;
	size_t					pos;
	int					partIndex;
} SDHandle;

void		sdInit();
StorageDevice*	sdCreate(SDParams *params);
void		sdHangup(StorageDevice *sd);
SDCommand*	sdPop(StorageDevice *dev);
void		sdPostComplete(SDCommand *cmd);
void		sdSignal(StorageDevice *dev);

#endif
