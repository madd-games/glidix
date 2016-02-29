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

#include <glidix/shmem.h>
#include <glidix/sched.h>
#include <glidix/errno.h>
#include <glidix/memory.h>
#include <glidix/semaphore.h>
#include <glidix/console.h>

typedef struct SharedMemory_
{
	struct SharedMemory_*			prev;
	struct SharedMemory_*			next;
	int					pidOwner;
	int					pidAssoc;
	int					protAssoc;
	int					protWorld;
	FrameList*				fl;
	uint64_t				id;
	uint64_t				size;
} SharedMemory;

static Semaphore semShmem;
static uint64_t nextShmemID;
static SharedMemory *shmemList;

void shmemInit()
{
	semInit(&semShmem);
	nextShmemID = 1;
	shmemList = NULL;
};

static void shmem_destroy(void *arg)
{
	SharedMemory *shmem = (SharedMemory*) arg;
	semWait(&semShmem);
	if (shmem->prev == NULL)
	{
		shmemList = shmem->next;
		if (shmem->next != NULL) shmem->next->prev = NULL;
	}
	else
	{
		shmem->prev->next = shmem->next;
		if (shmem->next != NULL) shmem->next->prev = shmem->prev;
	};
	
	kfree(shmem);
	semSignal(&semShmem);
};

uint64_t sys_shmalloc(uint64_t addr, uint64_t size, int assocPid, int protAssoc, int protWorld)
{
	if (addr & 0xFFF)
	{
		// not page-aligned
		ERRNO = EINVAL;
		return 0;
	};
	
	if (size & 0xFFF)
	{
		// size not page-size-multiple
		ERRNO = EINVAL;
		return 0;
	};
	
	int protAll = PROT_READ | PROT_WRITE;
	if ((protAssoc & protAll) != protAssoc)
	{
		ERRNO = EINVAL;
		return 0;
	};
	
	if ((protWorld & protAll) != protWorld)
	{
		ERRNO = EINVAL;
		return 0;
	};
	
	semWait(&semShmem);
	SharedMemory *shmem = NEW(SharedMemory);
	shmem->prev = NULL;
	shmem->next = NULL;
	shmem->pidOwner = getCurrentThread()->pid;
	shmem->pidAssoc = assocPid;
	shmem->protAssoc = protAssoc;
	shmem->protWorld = protWorld;
	shmem->size = size;
	shmem->fl = palloc(size/0x1000);
	shmem->fl->flags = FL_SHARED;
	shmem->fl->on_destroy = shmem_destroy;
	shmem->fl->on_destroy_arg = shmem;

	if (AddSegment(getCurrentThread()->pm, addr/0x1000, shmem->fl, PROT_READ | PROT_WRITE) != 0)
	{
		pdownref(shmem->fl);
		kfree(shmem);
		semSignal(&semShmem);
		ERRNO = ENOMEM;
		return 0;
	};
	
	uint64_t id = __sync_fetch_and_add(&nextShmemID, 1);
	shmem->id = id;
	
	// add to list
	if (shmemList == NULL)
	{
		shmemList = shmem;
	}
	else
	{
		shmem->next = shmemList;
		shmemList->prev = shmem;
		shmemList = shmem;
	};
	
	semSignal(&semShmem);
	
	// AddSegment() has upreffed the frame list
	pdownref(shmem->fl);
	
	return id;
};

int sys_shmap(uint64_t addr, uint64_t size, uint64_t id, int prot)
{
	if (prot == 0)
	{
		ERRNO = EINVAL;
		return -1;
	};
	
	if (addr & 0xFFF)
	{
		ERRNO = EINVAL;
		return -1;
	};
	
	semWait(&semShmem);
	SharedMemory *shmem;
	for (shmem=shmemList; shmem!=NULL; shmem=shmem->next)
	{
		if (shmem->id == id) break;
	};
	
	if (shmem == NULL)
	{
		semSignal(&semShmem);
		ERRNO = ENOENT;
		return -1;
	};
	
	if (pupref(shmem->fl) == 1)
	{
		// the shared memory is already in the process of being destroyed
		shmem->fl->refcount = 0;
		semSignal(&semShmem);
		ERRNO = ENOENT;
		return -1;
	};
	
	if (shmem->size != size)
	{
		semSignal(&semShmem);
		pdownref(shmem->fl);
		ERRNO = EINVAL;
		return -1;
	};
	
	int maxProt;
	if (getCurrentThread()->pid == shmem->pidAssoc)
	{
		maxProt = shmem->protAssoc;
	}
	else
	{
		maxProt = shmem->protWorld;
	};
	
	if ((maxProt & prot) != prot)
	{
		semSignal(&semShmem);
		pdownref(shmem->fl);
		ERRNO = EPERM;
		return -1;
	};
	
	if (AddSegment(getCurrentThread()->pm, addr/0x1000, shmem->fl, prot) != 0)
	{
		semSignal(&semShmem);
		pdownref(shmem->fl);
		ERRNO = ENOMEM;
		return -1;
	};
	
	semSignal(&semShmem);
	pdownref(shmem->fl);		// upreffed by AddSegment()
	return 0;
};
