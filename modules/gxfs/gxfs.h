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

#ifndef __gxfs_h
#define __gxfs_h

#include <glidix/common.h>

#define	GXFS_SBH_MAGIC					(*((const uint64_t*)"__GXFS__"))

/* features */
#define	GXFS_FEATURE_BASE				(1 << 0)

/* position of the SBB on disk */
#define	GXFS_SBB_OFFSET					(0x200000 + sizeof(GXFS_SuperblockHeader))

/* runtime flags */
#define	GXFS_RF_DIRTY					(1 << 0)

/* record types */
#define	GXFS_RT(s)					(*((const uint32_t*)s))

typedef struct
{
	uint64_t sbhMagic;
	uint8_t  sbhBootID[16];
	uint64_t sbhFormatTime;
	uint64_t sbhWriteFeatures;
	uint64_t sbhReadFeatures;
	uint64_t sbhOptionalFeatures;
	uint64_t sbhResv[2];
	uint64_t sbhChecksum;
} GXFS_SuperblockHeader;

typedef struct
{
	uint64_t sbbResvBlocks;
	uint64_t sbbUsedBlocks;
	uint64_t sbbTotalBlocks;
	uint64_t sbbFreeHead;
	uint64_t sbbLastMountTime;
	uint64_t sbbLastCheckTime;
	uint64_t sbbRuntimeFlags;
} GXFS_SuperblockBody;

typedef struct
{
	uint64_t ihNext;
} GXFS_InodeHeader;

typedef struct
{
	uint32_t rhType;
	uint32_t rhSize;
} GXFS_RecordHeader;

typedef struct
{
	uint32_t arType;	/* "ATTR" */
	uint32_t arRecordSize;	/* sizeof(GXFS_AttrRecord) */
	uint64_t arLinks;
	uint32_t arFlags;
	uint16_t arOwner;
	uint16_t arGroup;
	uint64_t arSize;
	uint64_t arATime;
	uint64_t arMTime;
	uint64_t arCTime;
	uint64_t arBTime;
	uint32_t arANano;
	uint32_t arMNano;
	uint32_t arCNano;
	uint32_t arBNano;
	uint64_t arIXPerm;
	uint64_t arOXPerm;
	uint64_t arDXPerm;
} GXFS_AttrRecord;

typedef struct
{
	uint32_t drType;	/* "DENT" */
	uint32_t drRecordSize;	/* sizeof(GXFS_DentRecord) + strlen(filename), 8-byte-aligned */
	uint64_t drInode;
	uint8_t drInoType;
	char drName[];
} GXFS_DentRecord;

typedef struct
{
	uint32_t trType;	/* "TREE" */
	uint32_t trSize;	/* sizeof(GXFS_TreeRecord) */
	uint64_t trDepth;
	uint64_t trHead;
} GXFS_TreeRecord;

typedef struct
{
	uint32_t crType;	/* "_ACL" */
	uint32_t crSize;	/* sizeof(GXFS_AclRecord) */
	AccessControlEntry acl[VFS_ACL_SIZE];
} GXFS_AclRecord;

/**
 * GXFS driver data.
 */
typedef struct
{
	/**
	 * The file handle.
	 */
	File *fp;
	
	/**
	 * Mount flags.
	 */
	int flags;
	
	/**
	 * The superblock body.
	 */
	GXFS_SuperblockBody sbb;
	
	/**
	 * Lock for updating the superblock etc.
	 */
	Semaphore lock;
} GXFS;

/**
 * GXFS inode data.
 */
typedef struct
{
	/**
	 * An array of block numbers storing the inode data, terminated with '0'.
	 */
	uint64_t *blocks;
} GXFS_Inode;

/**
 * Represents file tree data.
 */
typedef struct
{
	/**
	 * The GXFS filesystem instance that this tree belongs to.
	 */
	FileSystem *fs;
	
	/**
	 * Depth and head block.
	 */
	uint64_t depth;
	uint64_t head;
} GXFS_Tree;

#endif
