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

#include <glidix/procfs.h>
#include <glidix/memory.h>
#include <glidix/sched.h>
#include <glidix/string.h>

/**
 * The inode of each element in procfs is formatted such that:
 * Pid of relevant process = (inode / PFI_SIZE)
 * Which element = (inode % PFI_SIZE)
 * "which element" or element index is enumerated below.
 */

#define	PFI_SIZE				64

enum
{
	/**
	 * The directory of the process, e.g. /proc/5.
	 */
	PFI_DIRECTORY,

	/**
	 * Not applicable.
	 */
	PFI_NA,

	/**
	 * The file /proc/pid/status, from which you can read the ProcStatus structure.
	 */
	PFI_STATUS,

	/**
	 * The file /proc/pid/pages, from which an array of ProcPageInfo can be read to describe
	 * each memory segment, and with appropriate priviliges, mmap() may be called to map
	 * the memory into your own address space.
	 */
	PFI_PAGES,

	/**
	 * The file /proc/pid/exe, a symlink to the executable that started this process (which may now
	 * be broken).
	 */
	PFI_EXE,

	/**
	 * The file /proc/pid/execpars, a read-only (even to root!) file from which you may read the process'
	 * execpars.
	 */
	PFI_EXECPARS,

	/**
	 * The file /proc/pid/cwd, a symbol to the current working directory of the process.
	 */
	PFI_CWD,

	PFI_NUM
};

/**
 * Info about the files above (except 0 aka PFI_DIRECTORY, because that's the directory).
 */
typedef struct
{
	const char *name;
	mode_t mode;
} pfiInfo;
static pfiInfo pfiInfos[PFI_NUM] = {
	{"", 0},
	{"", 0},
	{"status", 0444},
	{"pages", 0444},
	{"exe", 0444 | VFS_MODE_LINK},
	{"execpars", 0444},
	{"cwd", 0755 | VFS_MODE_LINK}
};

static FileSystem *procfs;

typedef struct
{
	struct dirent ent;
	struct stat st;
} ProcfsRootDirent;

typedef struct
{
	ProcfsRootDirent *ents;
	int currentIndex;
	int count;
} ProcfsDir;

static void _pidstr(char *buffer, pid_t pid)
{
	char tmpbuf[22];
	char *putbuf = &tmpbuf[22];
	*--putbuf = 0;

	do
	{
		*--putbuf = '0'+(pid%10);
		pid /= 10;
	} while (pid != 0);

	strcpy(buffer, putbuf);
};

static void procfs_root_close(Dir *dir)
{
	ProcfsDir *pfdir = (ProcfsDir*) dir->fsdata;
	kfree(pfdir->ents);
	kfree(pfdir);
};

static int procfs_root_next(Dir *dir)
{
	ProcfsDir *pfdir = (ProcfsDir*) dir->fsdata;
	if ((++pfdir->currentIndex) == pfdir->count)
	{
		return -1;
	};

	memcpy(&dir->dirent, &pfdir->ents[pfdir->currentIndex].ent, sizeof(struct dirent));
	memcpy(&dir->stat, &pfdir->ents[pfdir->currentIndex].st, sizeof(struct stat));

	return 0;
};

static void procfs_dir_info(Dir *dir)
{
	ino_t ino = dir->dirent.d_ino % PFI_SIZE;
	strcpy(dir->dirent.d_name, pfiInfos[ino].name);
	dir->stat.st_ino = dir->dirent.d_ino;
	dir->stat.st_mode = pfiInfos[ino].mode;
	dir->stat.st_size = 0;

	if (ino == PFI_EXECPARS)
	{
		cli();
		lockSched();
		Thread *th = getThreadByPID(dir->dirent.d_ino / PFI_SIZE);
		if (th != NULL) dir->stat.st_size = th->szExecPars;
		unlockSched();
		sti();
	}
	else if (ino == PFI_EXE)
	{
		cli();
		lockSched();
		Thread *th = getThreadByPID(dir->dirent.d_ino / PFI_SIZE);
		if (th != NULL) dir->stat.st_size = strlen(th->execPars);
		unlockSched();
		sti();
	}
	else if (ino == PFI_CWD)
	{
		cli();
		lockSched();
		Thread *th = getThreadByPID(dir->dirent.d_ino / PFI_SIZE);
		if (th != NULL) dir->stat.st_size = strlen(th->cwd);
		unlockSched();
		sti();
	}
	else if (ino == PFI_STATUS)
	{
		dir->stat.st_size = sizeof(ProcStatus);
	};
};

static int procfs_dir_next(Dir *dir)
{
	if (((++dir->dirent.d_ino)%PFI_SIZE) == PFI_NUM)
	{
		return -1;
	};

	procfs_dir_info(dir);
	return 0;
};

static ssize_t procfs_dir_readlink(Dir *dir, char *buffer)
{
	ino_t ino = dir->dirent.d_ino % PFI_SIZE;
	cli();
	lockSched();
	Thread *th = getThreadByPID(dir->dirent.d_ino / PFI_SIZE);
	ssize_t out = -1;
	if (th != NULL)
	{
		switch (ino)
		{
		case PFI_EXE:
			strcpy(buffer, th->name);
			out = strlen(th->name);
			break;
		case PFI_CWD:
			strcpy(buffer, th->cwd);
			out = strlen(th->name);
			break;
		};
	};
	unlockSched();
	sti();

	return out;
};

static int procfs_root_opendir(Dir *me, Dir *dir, size_t szdir)
{
	pid_t pid = me->dirent.d_ino / PFI_SIZE;
	if (me->dirent.d_ino == 1)
	{
		pid = getCurrentThread()->creds->pid;
	};

	dir->dirent.d_ino = pid * PFI_SIZE + PFI_STATUS;
	dir->stat.st_nlink = 1;
	dir->stat.st_uid = me->stat.st_uid;
	dir->stat.st_gid = me->stat.st_gid;
	dir->stat.st_rdev = pid;
	dir->stat.st_size = 0;
	dir->stat.st_blksize = 1;
	dir->stat.st_blocks = 1;
	dir->stat.st_atime = 0;
	dir->stat.st_ctime = 0;
	dir->stat.st_mtime = 0;
	procfs_dir_info(dir);

	dir->next = procfs_dir_next;
	dir->readlink = procfs_dir_readlink;

	return 0;
};

static int procfs_openroot(FileSystem *fs, Dir *dir, size_t szdir)
{
	ProcfsDir *pfdir = (ProcfsDir*) kmalloc(sizeof(ProcfsDir));
	dir->fsdata = pfdir;

	pfdir->currentIndex = 0;
	pfdir->count = 1;			// "self"

	//pfdir->ents = (ProcfsRootDirent*) kmalloc(pfdir->count*sizeof(ProcfsRootDirent));

	cli();
	lockSched();
	Thread *th = getCurrentThread();
	Thread *ct = getCurrentThread();

	do
	{
		pfdir->count++;
		do
		{
			th = th->next;
		} while (th->creds == NULL);
	} while (th != ct);

	// the scheduler is locked and interrupts are off, so we cannot call kmalloc().
	// allocate the array on our stack, then copy it onto the heap later
	ProcfsRootDirent *ents = (ProcfsRootDirent*) kalloca(pfdir->count * sizeof(ProcfsRootDirent));
	
	int i = 0;
	th = ct;
	do
	{
		if (th->creds != NULL)
		{
			_pidstr(ents[i].ent.d_name, th->creds->pid);
			ents[i].ent.d_ino = th->creds->pid * PFI_SIZE;
			ents[i].st.st_ino = th->creds->pid * PFI_SIZE;
			ents[i].st.st_mode = 0700 | VFS_MODE_DIRECTORY;
			ents[i].st.st_nlink = 1;
			ents[i].st.st_uid = th->creds->ruid;
			ents[i].st.st_gid = th->creds->rgid;
			ents[i].st.st_rdev = th->creds->pid;
			ents[i].st.st_size = PFI_NUM;
			ents[i].st.st_blksize = 1;
			ents[i].st.st_blocks = 1;
			ents[i].st.st_atime = 0;
			ents[i].st.st_ctime = 0;
			ents[i].st.st_mtime = 0;
			i++;
		};
		
		th = th->next;
	} while (th != ct);

	unlockSched();
	sti();

	// now we can allocate on the heap
	pfdir->ents = (ProcfsRootDirent*) kmalloc(pfdir->count * sizeof(ProcfsRootDirent));
	memcpy(pfdir->ents, ents, pfdir->count * sizeof(ProcfsRootDirent));
	
	memcpy(&pfdir->ents[i], &pfdir->ents[0], sizeof(ProcfsRootDirent));
	strcpy(pfdir->ents[i].ent.d_name, "self");
	pfdir->ents[i].ent.d_ino = 1;
	pfdir->ents[i].st.st_ino = 1;

	memcpy(&dir->dirent, &pfdir->ents[0].ent, sizeof(struct dirent));
	memcpy(&dir->stat, &pfdir->ents[0].st, sizeof(struct stat));

	dir->close = procfs_root_close;
	dir->opendir = procfs_root_opendir;
	dir->next = procfs_root_next;

	return 0;
};

void initProcfs()
{
	procfs = (FileSystem*) kmalloc(sizeof(FileSystem));
	memset(procfs, 0, sizeof(FileSystem));

	procfs->fsname = "procfs";
	procfs->openroot = procfs_openroot;
};

FileSystem *getProcfs()
{
	return procfs;
};
