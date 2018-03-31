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

#ifndef ISOFS_H_
#define ISOFS_H_

#include <glidix/util/common.h>
#include <glidix/fs/vfs.h>

/**
 * If set in mount options, do not use Rock Ridge names.
 */
#define	ISOFS_MNT_NORRNAME			(1 << 0)

typedef struct
{
	char					year[4];
	char					month[2];			// 1-12
	char					day[2];				// 1-31
	char					hour[2];			// 0-23
	char					minute[2];			// 0-59
	char					second[2];			// 0-59
	char					centiseconds[2];		// 0-99		(ignored really)
	uint8_t					timezone;			// from 0=GMT-12, to 100=GMT+13, in 15-minute intervals
} PACKED ISOPrimaryDateTime;

typedef struct
{
	uint8_t					type;				// == 1
	char					magic[5];			// "CD001"
	uint8_t					version;			// == 1
	uint8_t					unused;				// == 0
	char					bootsysname[32];		// ignore this
	char					volumeID[32];
	uint8_t					zeroes[8];			// why? -_-
	uint32_t				volumeBlockCount;
	uint32_t				ignore1;
	uint8_t					ignore2[32];
	uint16_t				volumeCount;
	uint16_t				ignore3;
	uint16_t				volumeIndex;
	uint16_t				ignore4;
	uint16_t				blockSize;
	uint16_t				ignore5;
	uint8_t					ignore6[24];			// path table, we don't care
	uint8_t					rootDir[34];			// cast contents to ISODirentHeader.
	char					volumeSetID[128];
	char					publisherID[128];
	char					dataPreparerID[128];
	char					appID[128];
	char					copyrightFile[38];
	char					abstractFile[36];
	char					biblioFile[37];
	ISOPrimaryDateTime			dtCreation;
	ISOPrimaryDateTime			dtModification;
	ISOPrimaryDateTime			dtObsolete;
	ISOPrimaryDateTime			dtCanBeUsed;
	uint8_t					fileStructVersion;		// == 1
	uint8_t					ignore7;
} PACKED ISOPrimaryVolumeDescriptor;

typedef struct
{
	uint8_t					size;
	uint8_t					xattrSize;
	uint32_t				startLBA;
	uint32_t				ignore1;
	uint32_t				fileSize;
	uint32_t				ignore2;
	uint8_t					year;				// since 1990
	uint8_t					month;				// 1-12
	uint8_t					day;				// 1-31
	uint8_t					hour;				// 0-23
	uint8_t					minute;				// 0-59
	uint8_t					second;				// 0-59
	uint8_t					timezone;
	uint8_t					flags;
	uint8_t					zeroes[2];
	uint32_t				ignore3;
	uint8_t					filenameLen;
} PACKED ISODirentHeader;

typedef struct
{
	uint16_t				type;
	uint8_t					len;
	uint8_t					ver;
} PACKED SUSPHeader;

typedef struct
{
	uint16_t				type;				// "SP"
	uint8_t					len;
	uint8_t					ver;
	uint16_t				check;
	uint8_t					bskip;
} PACKED SUSP_SP;

/**
 * Mount options for isofs.
 */
typedef struct
{
	/**
	 * isofs mount flags (ISOFS_MNT_*).
	 */
	int flags;
	
	/**
	 * The user and group to which inodes shall be assigned (root by default for both).
	 */
	uid_t uid;
	gid_t gid;
	
	/**
	 * Permission bits to assign to each inode (0755 by default).
	 */
	mode_t mode;
	
	/**
	 * Executable permissions. "Own" permissions default to all zeroes, others to all ones.
	 */
	uint64_t ixperm, oxperm, dxperm;
} IsofsOptions;

/**
 * isofs-specific data (in FileSystem).
 */
typedef struct
{
	/**
	 * Filesystem image handle.
	 */
	File *fp;
	
	/**
	 * Filesystem options.
	 */
	IsofsOptions opt;
	
	/**
	 * Block size.
	 */
	uint64_t blockSize;
	
	/**
	 * System Use Area stuff.
	 */
	int foundSP;
	uint8_t bytesSkipped;
} Isofs;

#endif
