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

#include <glidix/ptr.h>
#include <glidix/memory.h>
#include <glidix/string.h>
#include <glidix/errno.h>
#include <glidix/sched.h>

static Semaphore semUpdate;
static Semaphore semLock;
static PtrState ptrState;

static ssize_t ptr_pread(Inode *inode, File *fp, void *buffer, size_t size, off_t off)
{
	if (size != sizeof(PtrState))
	{
		ERRNO = EINVAL;
		return -1;
	};
	
	int status = semWaitGen(&semUpdate, 1, SEM_W_FILE(fp->oflags), 0);
	if (status < 0)
	{
		ERRNO = -status;
		return -1;
	};
	
	semWaitGen(&semUpdate, -1, 0, 0);
	
	semWait(&semLock);
	memcpy(buffer, &ptrState, sizeof(PtrState));
	semSignal(&semLock);
	
	return sizeof(PtrState);
};

static ssize_t ptr_pwrite(Inode *inode, File *fp, const void *buffer, size_t size, off_t off)
{
	if (size != sizeof(PtrState))
	{
		ERRNO = EINVAL;
		return -1;
	};
	
	semWait(&semLock);
	memcpy(&ptrState, buffer, sizeof(PtrState));
	semSignal(&semLock);
	semSignal(&semUpdate);
	
	return sizeof(PtrState);
};

static void ptr_pollinfo(Inode *inode, File *fp, Semaphore **sems)
{
	sems[PEI_READ] = &semUpdate;
	sems[PEI_WRITE] = vfsGetConstSem();
};

void ptrInit()
{
	semInit(&semLock);
	semInit2(&semUpdate, 0);
	
	Inode *inode = vfsCreateInode(NULL, 0600);
	inode->pread = ptr_pread;
	inode->pwrite = ptr_pwrite;
	inode->pollinfo = ptr_pollinfo;
	
	if (devfsAdd("ptr", inode) != 0)
	{
		panic("failed to create /dev/ptr");
	};
	
	vfsDownrefInode(inode);
};

void ptrRelMotion(int deltaX, int deltaY)
{
	semWait(&semLock);
	if ((ptrState.width != 0) && (ptrState.height != 0))
	{
		int newX = ptrState.posX + deltaX;
		int newY = ptrState.posY + deltaY;
		
		if (newX < 0) newX = 0;
		if (newY < 0) newY = 0;
		
		if (newX >= ptrState.width) newX = ptrState.width - 1;
		if (newY >= ptrState.height) newY = ptrState.height - 1;
		
		ptrState.posX = newX;
		ptrState.posY = newY;
	};
	semSignal(&semLock);
	semSignal(&semUpdate);
};
