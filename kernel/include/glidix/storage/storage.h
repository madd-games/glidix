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

#ifndef __glidix_storage_h
#define __glidix_storage_h

#include <glidix/util/common.h>
#include <glidix/thread/semaphore.h>
#include <glidix/fs/devfs.h>
#include <glidix/fs/ioctl.h>
#include <glidix/thread/mutex.h>
#include <glidix/thread/sched.h>

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
#define	SD_MASTER_OPEN				0xFFFFFFFFFFFFFFFFUL

/**
 * The size of a track (32KB).
 */
#define	SD_TRACK_SIZE				0x8000UL

/**
 * Cache flags.
 */
#define	SD_BLOCK_DIRTY				(1UL << 48)

/**
 * Command flags.
 */
#define	SD_CMD_NOFREE				(1 << 0)		/* do not free structure after completion */

/**
 * Command types.
 */
enum SDCommandType
{
	SD_CMD_READ_TRACK,
	SD_CMD_WRITE_TRACK,
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

/**
 * GPT header.
 */
typedef struct
{
	uint64_t				sig;
	uint32_t				rev;
	uint32_t				headerSize;
	uint32_t				headerCRC32;
	uint32_t				zero;
	uint64_t				myLBA;
	uint64_t				altLBA;
	uint64_t				firstUseableLBA;
	uint64_t				lastUseableLBA;
	uint8_t					diskGUID[16];
	uint64_t				partListLBA;
	uint32_t				numPartEnts;
	uint32_t				partEntSize;
	uint32_t				partArrayCRC32;
} GPTHeader;

/**
 * GPT Partition.
 */
typedef struct
{
	uint8_t					type[16];
	uint8_t					partid[16];
	uint64_t				startLBA;
	uint64_t				endLBA;
	uint64_t				attr;
	uint16_t				partName[36];		/* UTF-16LE */
} GPTPartition;

typedef struct _SDCommand
{
	enum SDCommandType			type;

	/**
	 * For read/write commands, this is the PHYSICAL address to read into/write from,
	 * and must be 32KB in size (one track).
	 *
	 * For eject commands, it must be NULL.
	 *
	 * For getsize commands, points to an off_t where the size is to be stored.
	 */
	void*					block;

	/**
	 * Byte position within the track to read to/write from. Always track-aligned.
	 */
	uint64_t				pos;

	/**
	 * Semaphore used to decice when an operation is complete.
	 */
	Semaphore*				cmdlock;

	/**
	 * Status, set by default to 0 by the kernel, the driver sets this to -1 if it can't execute
	 * a command. If NULL, the driver should ignore errors.
	 */
	int*					status;
	
	/**
	 * Flags.
	 */
	int					flags;
	
	/**
	 * For queues.
	 */
	struct _SDCommand*			next;
} SDCommand;

/**
 * Represents a node of the block tree. The block tree is used for caching; it is a tree with
 * 7 levels, each with 128 entries, indexed by groups of 7 bits from left to right of an offset
 * into the device; the last 15 bits are the offsets into the track. We cache in tracks, which are
 * units of 32KB, for performance reasons.
 */
typedef struct
{
	/**
	 * The bottom 48 bits are the address of the next level; the higher 8 bits are flags (described above).
	 * The high 8 bits contain the number of times this page was accessed.
	 */
	uint64_t				entries[128];
} BlockTreeNode;

/**
 * Represents a storage device.
 */
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
	 * Number of subordinate device files (sub-files) which represent partitions.
	 */
	size_t					numSubs;

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
	 * Top level of the block tree.
	 */
	BlockTreeNode				cacheTop;
	
	/**
	 * Path to the GUID link or NULL.
	 */
	char*					guidPath;
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
	char*					guidPath;		// path to link under /dev/guid or NULL
} SDDeviceFile;

typedef struct
{
	StorageDevice*				sd;
	size_t					offset;
	size_t					size;
	int					partIndex;
} SDHandle;

void		sdInit();
StorageDevice*	sdCreate(SDParams *params);
void		sdHangup(StorageDevice *sd);
SDCommand*	sdPop(StorageDevice *dev);
void		sdPostComplete(SDCommand *cmd);
void		sdSignal(StorageDevice *dev);
void		sdSync();
int		sdFreeMemory();

#endif
