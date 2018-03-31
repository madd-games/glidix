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

#include <glidix/display/console.h>
#include <glidix/module/module.h>
#include <glidix/fs/fsdriver.h>
#include <glidix/fs/vfs.h>
#include <glidix/util/common.h>
#include <glidix/util/memory.h>
#include <glidix/util/string.h>
#include <glidix/thread/sched.h>
#include <glidix/util/errno.h>

#include "isofs.h"

static int checkPVD(ISOPrimaryVolumeDescriptor *pvd)
{
	if (pvd->type != 1)
	{
		return 0;
	};

	if (memcmp(pvd->magic, "CD001", 5) != 0)
	{
		return 0;
	};

	if (pvd->version != 1)
	{
		return 0;
	};

	return 1;
};

static int isoLoad(FileTree *ft, off_t pos, void *buffer)
{
	Inode *inode = (Inode*) ft->data;
	Isofs *isofs = (Isofs*) inode->fs->fsdata;
	
	ISODirentHeader head;
	if (vfsPRead(isofs->fp, &head, sizeof(ISODirentHeader), (off_t) inode->ino) != sizeof(ISODirentHeader))
	{
		return -1;
	};
	
	uint64_t start = (uint64_t) head.startLBA * isofs->blockSize;
	uint64_t size = 0x1000;
	if ((pos+size) > head.fileSize)
	{
		size = head.fileSize - pos;
	};
	
	if (vfsPRead(isofs->fp, buffer, size, start+pos) != size)
	{
		return -1;
	};
	
	return 0;
};

static int isoLoadInode(FileSystem *fs, Inode *inode)
{
	Isofs *isofs = (Isofs*) fs->fsdata;
	inode->links = 1;
	
	ISODirentHeader head;
	if (vfsPRead(isofs->fp, &head, sizeof(ISODirentHeader), (off_t) inode->ino) != sizeof(ISODirentHeader))
	{
		ERRNO = EIO;
		return -1;
	};
	
	if (head.flags & 2)
	{
		inode->mode = isofs->opt.mode | VFS_MODE_DIRECTORY;
		
		// load directory entries
		uint64_t pos = (uint64_t) head.startLBA * isofs->blockSize;
		uint64_t end = pos + head.fileSize;
		
		while (pos < end)
		{
			ino_t ino = (ino_t) pos;
			ISODirentHeader ent;
			if (vfsPRead(isofs->fp, &ent, sizeof(ISODirentHeader), pos) != sizeof(ISODirentHeader))
			{
				kprintf("isofs: failed to read dirent header\n");
				break;
			};
			
			if (ent.size == 0)
			{
				pos++;
				continue;
			};
			
			char namebuf[256];
			if (vfsPRead(isofs->fp, namebuf, ent.filenameLen, pos + sizeof(ISODirentHeader)) != ent.filenameLen)
			{
				kprintf("isofs: failed to read entry name\n");
				break;
			};
			
			namebuf[ent.filenameLen] = 0;
			char *semicolon = strstr(namebuf, ";");
			if (semicolon != NULL) *semicolon = 0;		// remove the version stuff

			if (namebuf[0] == 0)
			{
				uint64_t spPos = pos + sizeof(ISODirentHeader) + ent.filenameLen;
				spPos = (spPos + 1) & ~1;
				
				SUSP_SP sp;
				if (vfsPRead(isofs->fp, &sp, sizeof(SUSP_SP), spPos) == sizeof(SUSP_SP))
				{
					if (sp.type == (*((const uint16_t*)"SP")) && sp.len == 7 && sp.ver == 1
						&& sp.check == 0xEFBE)
					{
						isofs->foundSP = 1;
						isofs->bytesSkipped = sp.bskip;
					};
				};
			};
			
			if ((ent.flags & 1) == 0 && namebuf[0] != 0 && namebuf[0] != 1)
			{
				if (isofs->foundSP)
				{
					uint64_t headpos = pos + sizeof(ISODirentHeader) + ent.filenameLen;
					headpos = (headpos + 1) & ~1;
					headpos += isofs->bytesSkipped;
					
					while (headpos < (pos + ent.size))
					{
						SUSPHeader susp;
						if (vfsPRead(isofs->fp, &susp, sizeof(SUSPHeader), headpos) != sizeof(SUSPHeader))
						{
							break;
						};
						
						if (susp.len == 0)
						{
							break;
						};
						
						if (susp.type == (*((const uint16_t*)"ST")))
						{
							break;
						};
						
						if (susp.type == (*((const uint16_t*)"NM")))
						{
							memset(namebuf, 0, 256);
							if (vfsPRead(isofs->fp, namebuf, susp.len-5, headpos+5) != susp.len-5)
							{
								break;
							};
						};

						headpos += susp.len;
					};
				};
				
				vfsAppendDentry(inode, namebuf, ino);
			};

			pos += ent.size;
		};
	}
	else
	{
		inode->mode = isofs->opt.mode;
		inode->ft = ftCreate(FT_READONLY);
		ftDown(inode->ft);
		inode->ft->data = inode;
		inode->ft->load = isoLoad;
		inode->ft->size = head.fileSize;
	};
	
	inode->uid = isofs->opt.uid;
	inode->gid = isofs->opt.gid;
	inode->ixperm = isofs->opt.ixperm;
	inode->oxperm = isofs->opt.oxperm;
	inode->dxperm = isofs->opt.dxperm;
	
	return 0;
};

static void isoUnmount(FileSystem *fs)
{
	Isofs *isofs = (Isofs*) fs->fsdata;
	vfsClose(isofs->fp);
	kfree(isofs);
};

static Inode* isofs_openroot(const char *image, int flags, const void *options, size_t optlen, int *error)
{
	File *fp = vfsOpen(VFS_NULL_IREF, image, O_RDONLY, 0, error);
	if (fp == NULL)
	{
		kprintf("isofs: failed to open %s: errno %d\n", image, *error);
		return NULL;
	};
	
	ISOPrimaryVolumeDescriptor pvd;
	if (vfsPRead(fp, &pvd, sizeof(ISOPrimaryVolumeDescriptor), 0x8000) != sizeof(ISOPrimaryVolumeDescriptor))
	{
		kprintf("isofs: failed to read the whole PVD into memory\n");
		vfsClose(fp);
		*error = EINVAL;
		return NULL;
	};
	
	if (!checkPVD(&pvd))
	{
		kprintf("isofs: invalid PVD\n");
		vfsClose(fp);
		*error = EINVAL;
		return NULL;
	};
	
	Isofs *isofs = NEW(Isofs);
	if (isofs == NULL)
	{
		vfsClose(fp);
		*error = ENOMEM;
		return NULL;
	};
	
	isofs->fp = fp;
	
	// default options
	isofs->opt.flags = 0;
	isofs->opt.uid = 0;
	isofs->opt.gid = 0;
	isofs->opt.mode = 0755;
	isofs->opt.ixperm = XP_ALL;
	isofs->opt.oxperm = 0;
	isofs->opt.dxperm = XP_ALL;
	
	// copy as much of the user options as possible
	if (optlen > sizeof(IsofsOptions)) optlen = sizeof(IsofsOptions);
	memcpy(&isofs->opt, options, optlen);
	
	// sanitize
	isofs->opt.mode &= 0xFFF;
	
	isofs->blockSize = pvd.blockSize;
	
	isofs->foundSP = 0;

	// create the filesystem
	FileSystem *fs = vfsCreateFileSystem("isofs");
	fs->fsdata = isofs;
	fs->loadInode = isoLoadInode;
	fs->unmount = isoUnmount;
	fs->flags = VFS_ST_RDONLY;
	fs->blockSize = isofs->blockSize;
	memcpy(fs->bootid, "\0\0" "ISOBOOT" "\0\0\0\0\0\xF0\x0D", 16);
	
	// load the root inode
	Inode *root = vfsCreateInode(NULL, isofs->opt.mode | VFS_MODE_DIRECTORY);
	root->fs = fs;
	fs->imap = root;
	root->ino = (ino_t) (0x8000 + 156);	// the root dirent in the PVD
	if (isoLoadInode(fs, root) != 0)
	{
		vfsDownrefInode(root);
		kfree(fs);
		kfree(isofs);
		vfsClose(fp);
		*error = EIO;
		return NULL;
	};
	
	return root;
};

static FSDriver isofs = {
	.fsname = "isofs",
	.openroot = isofs_openroot,
};

MODULE_INIT()
{
	kprintf("isofs: registering isofs driver\n");
	registerFSDriver(&isofs);
	return MODINIT_OK;
};

MODULE_FINI()
{
	kprintf("isofs: removing isofs driver\n");
	unregisterFSDriver(&isofs);
	return 0;
};
