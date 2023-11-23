/*
	Glidix Shell Utilities

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

#define _GNU_SOURCE
#define _GLIDIX_SOURCE

#include <sys/stat.h>
#include <dirent.h>
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <string.h>
#include <errno.h>
#include <time.h>
#include <fcntl.h>
#include <stdint.h>
#include <stddef.h>

/**
 * Given a disk image containing a GPT with a Glidix root partition, this program formats the root
 * partition to GXFS and then populates the root directory (recursively) from the specified directory.
 */

#define	GXFS_MAGIC				(*((const uint64_t*)"__GXFS__"))

#define	GXFS_FEATURE_BASE			(1 << 0)

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
	uint32_t rhType;
	uint32_t rhSize;
} GXFS_RecordHeader;

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

typedef struct
{
	uint8_t					type[16];
	uint8_t					partid[16];
	uint64_t				startLBA;
	uint64_t				endLBA;
	uint64_t				attr;
	uint16_t				partName[36];		/* UTF-16LE */
} GPTPartition;

typedef struct
{
	char					buffer[0x1000];
	uint64_t				inoCurrent;
	uint64_t				nextRecordOffset;
} InodeWriter;

typedef struct DirEntry
{
	struct DirEntry*			next;
	char*					name;
	uint64_t				ino;
	uint8_t					type;
} DirEntry;

typedef struct
{
	uint64_t				ino;
	DirEntry*				ents;
	GXFS_AttrRecord				ar;
} DirWriter;

typedef struct TreeNodeWriter
{
	struct TreeNodeWriter*			parent;
	uint64_t				block;
	int					numChildren;
	uint64_t				children[512];
} TreeNodeWriter;

typedef struct
{
	uint64_t				ino;
	GXFS_AttrRecord				ar;
	GXFS_TreeRecord				tr;
	TreeNodeWriter*				tnwHead;
} FileWriter;

int driveFD;
off_t rootPartOffset;
size_t rootPartSize;
const char *progName;
time_t formatTime;
char zeroes[0x1000];

union
{
	char bytes[0x1000];
	struct
	{
		GXFS_SuperblockHeader header;
		GXFS_SuperblockBody body;
	};
} superblock;

uint8_t rootPartType[16] = {0x9C, 0xAD, 0xC1, 0x81, 0xC4, 0xBD, 0x09, 0x48, 0x8D, 0x9F, 0xDC, 0xB2, 0xA9, 0xB8, 0x5D, 0x01};

ssize_t partWrite(const void *buffer, size_t size, off_t offset)
{
	return pwrite(driveFD, buffer, size, offset + rootPartOffset);
};

uint64_t blockAlloc()
{
	if (superblock.body.sbbUsedBlocks == superblock.body.sbbTotalBlocks)
	{
		fprintf(stderr, "%s: ran out of space in the image\n", progName);
		close(driveFD);
		exit(1);
	};

	return superblock.body.sbbUsedBlocks++;
};

void blockWrite(uint64_t block, const void *buffer)
{
	if (partWrite(buffer, 0x1000, 0x200000 + (block << 12)) != 0x1000)
	{
		fprintf(stderr, "%s: failed to write to block %lu\n", progName, block);
		close(driveFD);
		exit(1);
	};
};

void iwInit(InodeWriter *iw, uint64_t ino)
{
	memset(iw->buffer, 0, 0x1000);
	iw->inoCurrent = ino;
	iw->nextRecordOffset = 8;
};

void iwFlush(InodeWriter *iw)
{
	blockWrite(iw->inoCurrent, iw->buffer);
};

void iwAddRecord(InodeWriter *iw, const void *buffer, size_t size)
{
	const char *scan = (const char*) buffer;
	while (size != 0)
	{
		if (iw->nextRecordOffset == 0x1000)
		{
			uint64_t newBlock = blockAlloc();
			*((uint64_t*)iw->buffer) = newBlock;

			iwFlush(iw);
			iwInit(iw, newBlock);
		};

		size_t bytesToDo = 0x1000 - iw->nextRecordOffset;
		if (bytesToDo > size)
		{
			bytesToDo = size;
		};

		memcpy(iw->buffer + iw->nextRecordOffset, scan, bytesToDo);

		iw->nextRecordOffset += bytesToDo;
		scan += bytesToDo;
		size -= bytesToDo;
	};
};

void dwInit(DirWriter *dw, uint64_t ino)
{
	dw->ino = ino;
	dw->ents = NULL;

	memset(&dw->ar, 0, sizeof(GXFS_AttrRecord));
	dw->ar.arType = (*((const uint32_t*)"ATTR"));
	dw->ar.arRecordSize = sizeof(GXFS_AttrRecord);
	dw->ar.arLinks = 1;
	dw->ar.arFlags = 0x1000 | 0755;
	dw->ar.arOwner = 0;
	dw->ar.arGroup = 0;
	dw->ar.arSize = 0;
	dw->ar.arATime = dw->ar.arMTime = dw->ar.arCTime = dw->ar.arBTime = formatTime;
};

void dwAddEntry(DirWriter *dw, const char *name, uint64_t ino, uint8_t type)
{
	DirEntry *ent = (DirEntry*) malloc(sizeof(DirEntry));
	ent->next = dw->ents;
	ent->name = strdup(name);
	ent->ino = ino;
	ent->type = type;

	dw->ents = ent;
	dw->ar.arLinks++;
};

void dwFinish(DirWriter *dw)
{
	InodeWriter iw;
	iwInit(&iw, dw->ino);
	iwAddRecord(&iw, &dw->ar, sizeof(GXFS_AttrRecord));

	while (dw->ents != NULL)
	{
		DirEntry *ent = dw->ents;
		dw->ents = ent->next;

		size_t nameOffset = offsetof(GXFS_DentRecord, drName);

		GXFS_DentRecord dr;
		dr.drType = (*((const uint32_t*)"DENT"));
		dr.drRecordSize = (nameOffset + strlen(ent->name) + 7) & ~7;
		dr.drInode = ent->ino;
		dr.drInoType = ent->type;
		
		iwAddRecord(&iw, &dr, nameOffset);
		iwAddRecord(&iw, ent->name, strlen(ent->name));
		iwAddRecord(&iw, zeroes, dr.drRecordSize - strlen(ent->name) - nameOffset);
	};

	iwFlush(&iw);
};

TreeNodeWriter* tnwCreate(uint64_t firstChild)
{
	TreeNodeWriter *tnw = (TreeNodeWriter*) calloc(sizeof(TreeNodeWriter), 1);
	tnw->block = blockAlloc();
	tnw->children[0] = firstChild;
	tnw->numChildren = 1;
	return tnw;
};

void tnwFlush(TreeNodeWriter *tnw)
{
	blockWrite(tnw->block, tnw->children);
};

void tnwPush(FileWriter *fw, TreeNodeWriter *tnw, uint64_t block)
{
	if (tnw->numChildren == 512)
	{
		tnwFlush(tnw);

		if (tnw->parent == NULL)
		{
			if (fw->tr.trDepth == 5)
			{
				fprintf(stderr, "%s: file size limit exceeded\n", progName);
				close(driveFD);
				exit(1);
			};

			tnw->parent = tnwCreate(tnw->block);
			fw->tr.trDepth++;
			fw->tr.trHead = tnw->parent->block;
		};

		tnw->block = blockAlloc();
		tnw->numChildren = 0;
		memset(tnw->children, 0, sizeof(tnw->children));

		tnwPush(fw, tnw->parent, tnw->block);
	};

	tnw->children[tnw->numChildren++] = block;
};

void fwInit(FileWriter *fw, uint64_t ino, struct stat *st)
{
	fw->ino = ino;

	memset(&fw->ar, 0, sizeof(GXFS_AttrRecord));
	fw->ar.arType = (*((const uint32_t*)"ATTR"));
	fw->ar.arRecordSize = sizeof(GXFS_AttrRecord);
	fw->ar.arLinks = 1;
	fw->ar.arFlags = st->st_mode & 0777;
	fw->ar.arOwner = 0;
	fw->ar.arGroup = 0;
	fw->ar.arSize = st->st_size;
	fw->ar.arATime = fw->ar.arMTime = fw->ar.arCTime = fw->ar.arBTime = formatTime;

	fw->tr.trType = (*((const uint32_t*)"TREE"));
	fw->tr.trSize = sizeof(GXFS_TreeRecord);
	fw->tr.trDepth = 0;
	fw->tr.trHead = 0;

	fw->tnwHead = NULL;
};

void fwAppendDataBlock(FileWriter *fw, uint64_t block)
{
	if (fw->tr.trHead == 0)
	{
		fw->tr.trHead = block;
		return;
	};

	if (fw->tnwHead == NULL)
	{
		fw->tnwHead = tnwCreate(fw->tr.trHead);
		fw->tr.trHead = fw->tnwHead->block;
		fw->tr.trDepth = 1;
	};

	tnwPush(fw, fw->tnwHead, block);
};

void fwWriteBlock(FileWriter *fw, const void *data)
{
	uint64_t block = blockAlloc();
	blockWrite(block, data);
	fwAppendDataBlock(fw, block);
};

void fwFinish(FileWriter *fw)
{
	InodeWriter iw;
	iwInit(&iw, fw->ino);

	while (fw->tnwHead != NULL)
	{
		TreeNodeWriter *tnw = fw->tnwHead;
		fw->tnwHead = tnw->parent;

		tnwFlush(tnw);
		free(tnw);
	};

	iwAddRecord(&iw, &fw->ar, sizeof(GXFS_AttrRecord));
	if (fw->tr.trHead != 0)
	{
		iwAddRecord(&iw, &fw->tr, sizeof(GXFS_TreeRecord));
	};

	iwFlush(&iw);
};

void writeFile(const char *filename, uint64_t ino, struct stat *st)
{
	FileWriter fw;
	fwInit(&fw, ino, st);

	int fd = open(filename, O_RDONLY);
	if (fd == -1)
	{
		fprintf(stderr, "%s: failed to open %s: %s\n", progName, filename, strerror(errno));
		close(driveFD);
		exit(1);
	};

	char buffer[0x1000];
	for (;;)
	{
		memset(buffer, 0, 0x1000);
		ssize_t size = read(fd, buffer, 0x1000);

		if (size == -1)
		{
			fprintf(stderr, "%s: error while reading %s: %s\n", progName, filename, strerror(errno));
			close(driveFD);
			close(fd);
			exit(1);
		};

		if (size == 0)
		{
			break;
		};

		fwWriteBlock(&fw, buffer);
		if (size < 0x1000)
		{
			break;
		};
	};

	fwFinish(&fw);
};

void writeLink(const char *destpath, size_t size, uint64_t ino)
{
	InodeWriter iw;
	iwInit(&iw, ino);

	GXFS_AttrRecord ar;
	memset(&ar, 0, sizeof(GXFS_AttrRecord));
	ar.arType = (*((const uint32_t*)"ATTR"));
	ar.arRecordSize = sizeof(GXFS_AttrRecord);
	ar.arLinks = 1;
	ar.arFlags = 0x5000 | 0755;
	ar.arOwner = 0;
	ar.arGroup = 0;
	ar.arSize = 0;
	ar.arATime = ar.arMTime = ar.arCTime = ar.arBTime = formatTime;

	iwAddRecord(&iw, &ar, sizeof(GXFS_AttrRecord));

	GXFS_RecordHeader rh;
	rh.rhType = (*((const uint32_t*)"LINK"));
	rh.rhSize = sizeof(GXFS_RecordHeader) + size;

	iwAddRecord(&iw, &rh, rh.rhSize);
	iwAddRecord(&iw, destpath, size);

	iwFlush(&iw);
};

void walkDir(int level, const char *dirname, uint64_t ino)
{
	if (level == 64)
	{
		fprintf(stderr, "%s: recursion limit exceeded\n", progName);
		close(driveFD);
		exit(1);
	};

	char margin[128];
	memset(margin, ' ', 2*level);
	margin[2*level] = 0;

	DirWriter dw;
	dwInit(&dw, ino);

	DIR *dirp = opendir(dirname);
	if (dirp == NULL)
	{
		fprintf(stderr, "%s: failed to open directory %s: %s\n", progName, dirname, strerror(errno));
		close(driveFD);
		exit(1);
	};

	struct dirent *ent;
	while ((ent = readdir(dirp)) != NULL)
	{
		if (strcmp(ent->d_name, ".") == 0)
		{
			continue;
		};

		if (strcmp(ent->d_name, "..") == 0)
		{
			continue;
		};

		char *fullpath;
		asprintf(&fullpath, "%s/%s", dirname, ent->d_name);

		struct stat st;
		if (lstat(fullpath, &st) != 0)
		{
			fprintf(stderr, "%s: failed to stat %s: %s\n", progName, fullpath, strerror(errno));
			close(driveFD);
			exit(1);
		};

		uint64_t childIno = blockAlloc();

		uint8_t type;
		if (S_ISDIR(st.st_mode))
		{
			printf("%s%s/ [inode %lu]\n", margin, ent->d_name, childIno);
			walkDir(level + 1, fullpath, childIno);
			type = 1;
		}
		else if (S_ISREG(st.st_mode))
		{
			printf("%s%s (%luK) [inode %lu]\n", margin, ent->d_name, st.st_size/1024, childIno);
			writeFile(fullpath, childIno, &st);
			type = 0;
		}
		else if (S_ISLNK(st.st_mode))
		{
			char destpath[256];
			memset(destpath, 0, 256);
			ssize_t destsize = readlink(fullpath, destpath, 256);
			if (destsize == -1)
			{
				fprintf(stderr, "%s: failed to readlink %s: %s\n", progName, fullpath, strerror(errno));
				close(driveFD);
				exit(1);
			};

			printf("%s%s -> %s [inode %lu]\n", margin, ent->d_name, destpath, childIno);
			writeLink(destpath, 256, childIno);

			type = 5;
		}
		else
		{
			fprintf(stderr, "%s: %s has an inode type which cannot be transferred\n", progName, fullpath);
			close(driveFD);
			exit(1);
		};

		dwAddEntry(&dw, ent->d_name, childIno, type);
		free(fullpath);
	};

	closedir(dirp);

	dwFinish(&dw);
};

void generateMGSID(uint8_t *buffer)
{
	uint64_t timestamp = (uint64_t) time(NULL);
	uint64_t counter = (uint64_t) clock();
	
	uint8_t *stampref = (uint8_t*) &timestamp;
	uint8_t *countref = (uint8_t*) &counter;
	
	uint8_t stampSize = 8;
	uint8_t countSize = 8;
	
	int i;
	for (i=7; i>=0; i--)
	{
		if (stampref[i] != 0)
		{
			break;
		};
		
		stampSize--;
	};
	
	for (i=7; i>=0; i--)
	{
		if (countref[i] != 0)
		{
			break;
		};
		
		countSize--;
	};
	
	if ((stampSize + countSize) > 14)
	{
		countSize = 14 - stampSize;
	};
	
	buffer[0] = (stampSize << 4) | countSize;
	buffer[1] = 0x01;
	
	uint8_t *put = &buffer[2];
	memcpy(put, stampref, stampSize);
	put += stampSize;
	memcpy(put, countref, countSize);
	put += countSize;
	
	uint64_t padding = 14 - stampSize - countSize;
	if (padding != 0)
	{
		int fd = open("/dev/urandom", O_RDONLY);
		if (fd != -1)
		{
			read(fd, put, padding);
			close(fd);
		};
	};
};

void doChecksum(uint64_t *ptr)
{
	size_t count = 9;		/* 9 quadwords before the sbhChecksum field */
	uint64_t state = 0xF00D1234BEEFCAFEUL;
	
	while (count--)
	{
		state = (state << 1) ^ (*ptr++);
	};
	
	*ptr = state;
};

int main(int argc, char *argv[])
{
	progName = argv[0];
	const char *filename = NULL;
	const char *rootdir = NULL;

	int n;
	for (n=1; n<argc; n++)
	{
		if (argv[n][0] != '-')
		{
			if (filename == NULL)
			{
				filename = argv[n];
			}
			else if (rootdir == NULL)
			{
				rootdir = argv[n];
			}
			else
			{
				fprintf(stderr, "%s: too many arguments\n", argv[0]);
				return 1;
			};
		}
		else
		{
			fprintf(stderr, "%s: unknown command-line switch: %s\n", argv[0], argv[n]);
			return 1;
		};
	};
	
	if (filename == NULL || rootdir == NULL)
	{
		fprintf(stderr, "USAGE:\t%s <imgfile> <rootdir>\n", argv[0]);
		fprintf(stderr, "\tThe supplied image file is expected to have a GPT, containing\n");
		fprintf(stderr, "\texactly 1 Glidix root partition. This program will create a GXFS\n");
		fprintf(stderr, "\tfilesystem on the root partition, and will populate it with a copy\n");
		fprintf(stderr, "\tof the 'rootdir'.\n");
		fprintf(stderr, "\tWARNING: This will delete all data on the Glidix root partition of the image!\n");
		return 1;
	};
	
	driveFD = open(filename, O_RDWR);
	if (driveFD == -1)
	{
		fprintf(stderr, "%s: cannot open %s: %s\n", argv[0], filename, strerror(errno));
		return 1;
	};

	GPTHeader gptHeader;
	if (pread(driveFD, &gptHeader, sizeof(GPTHeader), 512) != sizeof(GPTHeader))
	{
		fprintf(stderr, "%s: failed to read the GPT header from %s\n", argv[0], filename);
		close(driveFD);
		return 1;
	};

	if (gptHeader.partEntSize != sizeof(GPTPartition))
	{
		fprintf(stderr, "%s: the file %s has an invalid partition table entry size\n", argv[0], filename);
		close(driveFD);
		return 1;
	};

	size_t partTableSize = sizeof(GPTPartition) * gptHeader.numPartEnts;
	GPTPartition *parts = (GPTPartition*) malloc(partTableSize);

	if (pread(driveFD, parts, partTableSize, gptHeader.partListLBA * 512) != partTableSize)
	{
		fprintf(stderr, "%s: failed to read the partition table from %s\n", argv[0], filename);
		close(driveFD);
		return 1;
	};

	int rootPartIndex;
	for (rootPartIndex=0; rootPartIndex<gptHeader.numPartEnts; rootPartIndex++)
	{
		GPTPartition *part = &parts[rootPartIndex];
		if (memcmp(part->type, rootPartType, 16) == 0)
		{
			rootPartOffset = part->startLBA * 512;
			rootPartSize = (part->endLBA - part->startLBA + 1) * 512;
			break;
		};
	};

	if (rootPartIndex == gptHeader.numPartEnts)
	{
		fprintf(stderr, "%s: there is no glidix root partition on %s\n", argv[0], filename);
		close(driveFD);
		return 1;
	};

	if (rootPartSize < 0x1000000)
	{
		fprintf(stderr, "%s: the root partition is below the minimum size of 16MB\n", argv[0]);
		close(driveFD);
		return 1;
	};

	printf("%s: found the glidix root partition on %s, creating the image...\n", argv[0], filename);

	// Put the "int 0x18" instruction in the VBR, making it report that it's not bootable.
	// After formatting, you should install the bootloader if that is necessary.
	partWrite("\xCD\x18", 2, 0);
	
	// Initialize the superblock.
	GXFS_SuperblockHeader *sbh = &superblock.header;
	GXFS_SuperblockBody *sbb = &superblock.body;
	
	formatTime = time(NULL);
	sbh->sbhMagic = GXFS_MAGIC;
	generateMGSID(sbh->sbhBootID);
	sbh->sbhFormatTime = formatTime;
	sbh->sbhWriteFeatures = GXFS_FEATURE_BASE;
	sbh->sbhReadFeatures = GXFS_FEATURE_BASE;
	sbh->sbhOptionalFeatures = 0;
	doChecksum((uint64_t*) sbh);

	sbb->sbbResvBlocks = 8;
	sbb->sbbUsedBlocks = 9;			/* reserved + /boot (inode 8) */
	sbb->sbbTotalBlocks = (rootPartSize - 0x200000) >> 12;
	sbb->sbbFreeHead = 0;
	sbb->sbbLastMountTime = formatTime;
	sbb->sbbLastCheckTime = formatTime;
	sbb->sbbRuntimeFlags = 0;
	
	// create the "bad blocks" inode
	char block[0x1000];
	memset(block, 0, 4096);
	
	// (skip over the inode header, ihNext=0)
	GXFS_AttrRecord *ar = (GXFS_AttrRecord*) &block[8];
	ar->arType = (*((const uint32_t*)"ATTR"));
	ar->arRecordSize = sizeof(GXFS_AttrRecord);
	ar->arLinks = 1;
	ar->arFlags = 0600;	// regular file, only accessible to root
	ar->arOwner = 0;
	ar->arGroup = 0;
	ar->arSize = 0;
	ar->arATime = ar->arMTime = ar->arCTime = ar->arBTime = formatTime;
	
	partWrite(block, 4096, 0x201000);

	// The root directory inode is always on block 2; traverse recursively.
	printf("/\n");
	walkDir(1, rootdir, 2);

#if 0
	// create the root directory inode (again, skip inode header, ihNext=0)
	// ar already set to the right place
	// also create a "/boot" entry
	ar->arFlags = 0x1000 | 01755;	// directory, sticky, mode 755
	GXFS_DentRecord *dr = (GXFS_DentRecord*) &ar[1];
	dr->drType = (*((const uint32_t*)"DENT"));
	dr->drRecordSize = 24;
	dr->drInode = 8;		/* use inode 8 for the /boot directory */
	dr->drInoType = 1;		/* type = directory */
	strcpy(dr->drName, "boot");
	partWrite(block, 4096, 0x202000);
	
	// inode 8 (/boot) is almost the same
	ar->arFlags = 0x1000 | 0755;
	// size the same
	dr->drInode = 3;
	dr->drInoType = 3;		/* "block device" */
	strcpy(dr->drName, "loader");
	partWrite(block, 4096, 0x208000);
	
	// now onto inode 3
	memset(block, 0, 4096);
	ar->arType = (*((const uint32_t*)"ATTR"));
	ar->arRecordSize = sizeof(GXFS_AttrRecord);
	ar->arLinks = 1;
	ar->arFlags = 0x13000 | 0600;	// fixed size, block device, only accessible to root
	ar->arOwner = 0;
	ar->arGroup = 0;
	ar->arSize = 2 * 1024 * 1024;	// 2MB
	ar->arATime = ar->arMTime = ar->arCTime = ar->arBTime = formatTime;
	
	GXFS_TreeRecord *tr = (GXFS_TreeRecord*) &ar[1];
	tr->trType = (*((const uint32_t*)"TREE"));
	tr->trSize = sizeof(GXFS_TreeRecord);
	tr->trDepth = 1;
	tr->trHead = 4;
	
	partWrite(block, 4096, 0x203000);
	
	// finally the pointer block (4)
	uint64_t *table = (uint64_t*) block;
	int i;
	for (i=0; i<512; i++)
	{
		table[i] = (uint64_t) (i-512);
	};
	
	partWrite(block, 4096, 0x204000);
#endif

	// Write the final superblock.
	partWrite(superblock.bytes, 4096, 0x200000);

	// thanks
	close(driveFD);

	printf(
		"%s: root filesystem installed on %s with %luM / %luM used (%lu%%)\n",
		argv[0],
		filename,
		superblock.body.sbbUsedBlocks / 512,
		superblock.body.sbbTotalBlocks / 512,
		100 * superblock.body.sbbUsedBlocks / superblock.body.sbbTotalBlocks
	);

	return 0;
};
