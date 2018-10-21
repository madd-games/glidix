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

#define	IOCTL_SDI_IDENTITY			IOCTL_ARG(SDIdentity, IOCTL_INT_SDI, 0)
#define	IOCTL_SDI_EJECT				IOCTL_NOARG(IOCTL_INT_SDI, 1)

/**
 * Storage device flags.
 */
#define	SD_READONLY				(1 << 0)	/* device is read-only */
#define	SD_HANGUP				(1 << 1)	/* device hanged up */
#define	SD_EJECTABLE				(1 << 2)	/* device is ejectable (memory can be replaced, such as CD-ROM) */
#define	SD_REMOVEABLE				(1 << 3)	/* the whole drive can be removed at runtime */

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
 * Storage device operations defined by drivers. 'readBlocks' and 'writeBlocks' MUST be defined,
 * and it is assumed; the other functions are allowed to be NULL. If a function pointer is beyond
 * the 'size', then it is assumed undefined (this is to ensure backwards compatibility with drivers
 * which may have used an earlier version of the structure).
 */
typedef struct
{
	/**
	 * Size of this structure (used to determine which functions are defined).
	 */
	size_t size;
	
	/**
	 * Read blocks from the storage device into 'buffer'. 'startBlock' is the LBA
	 * of the first block to be loaded; 'numBlocks' is the number of blocks.
	 * 'buffer' is assumed to be large enough to hold the returned blocks; the kernel
	 * allocates it based on block count, and the block size defined in the storage
	 * device parameter structure.
	 *
	 * Return 0 on success, or an error number on error (typically EIO). It is an error
	 * if the specified LBA and/or count are out of range of the disk.
	 *
	 * This function MUST NOT call any memory allocation functions, neither for physical
	 * not virtual memory. This is because the drive might be used to implement page
	 * swapping.
	 */
	int (*readBlocks)(void *drvdata, size_t startBlock, size_t numBlocks, void *buffer);
	
	/**
	 * Same as above, but write blocks from memory onto the disk instead. Same ban about
	 * memory allocation applies!
	 */
	int (*writeBlocks)(void *drvdata, size_t startBlock, size_t numBlocks, const void *buffer);
	
	/**
	 * Return the size of the disk. This is used if the disk is a removable medium and the size
	 * could thus change. Return 0 if there is no medium, or if an error occured.
	 */
	size_t (*getSize)(void *drvdata);
	
	/**
	 * Eject the medium if removeable. Return 0 on success, or an error number on error (typically
	 * EIO).
	 */
	int (*eject)(void *drvdata);
} SDOps;

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
	
	/**
	 * Name of the storage device.
	 */
	char					name[128];
	
	/**
	 * Driver operations, and driver-specified data.
	 */
	void*					drvdata;
	SDOps*					ops;
} StorageDevice;

typedef struct
{
	int					flags;
	size_t					blockSize;
	size_t					totalSize;
} SDParams;

typedef union
{
	struct
	{
		int				flags;
		int				partIndex;
		size_t				offset;
		size_t				size;
		char				name[128];
	};
	
	/* force the size to 256 bytes */
	char _size[256];
} SDIdentity;

typedef struct
{
	StorageDevice*				sd;
	size_t					offset;
	size_t					size;
	int					partIndex;		// -1 = master
	char*					guidPath;		// path to link under /dev/guid or NULL
	char					name[128];
} SDDeviceFile;

typedef struct
{
	StorageDevice*				sd;
	size_t					offset;
	size_t					size;
	int					partIndex;
	char					name[128];
} SDHandle;

/**
 * Initialize the storage device subsystem.
 */
void sdInit();

/**
 * Create a storage device and return a handle to it. 'params' points to the device parameters structure.
 * 'name' is the name of the storage device; 'ops' is a pointer to a driver operations structure which
 * implements operations on the device, and 'drvdata' is a pointer to driver-specific data which is passed
 * to all the SDOps functions on this device.
 */
StorageDevice* sdCreate(SDParams *params, const char *name, SDOps *ops, void *drvdata);

/**
 * Report a storage device as no longer available. This is called by the driver upon unloading, or the device
 * disconnecting; the driver MUST NOT use 'sd' after calling this function.
 */
void sdHangup(StorageDevice *sd);

/**
 * Flush all disk caches to memory.
 */
void sdSync();

/**
 * TODO
 */
uint64_t	sdFreeMemory();

/**
 * TODO
 */
void		sdDumpInfo();

#endif
