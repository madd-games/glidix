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

#ifndef __glidix_shmem_h
#define __glidix_shmem_h

/**
 * Glidix shared memory.
 * A shared memory object is identified by a 64-bit identifier, has an owner pid,
 * an associated pid and access flags. A process may allocate a shared memory object,
 * thus becoming its owner. It may set different permissions for the associated pid
 * and different permissions for the world (every other process). The memory is freed
 * once all processes unmap the memory.
 */

#include <glidix/common.h>
#include <glidix/procmem.h>

void shmemInit();

/**
 * The system call which creates shared memory. The associated pid may be zero if you don't
 * want to associate any specific pid with this shared memory. Returns 0 on error, or the
 * shared memory ID on success.
 */
uint64_t sys_shmalloc(uint64_t addr, uint64_t size, int assocPid, int protAssoc, int protWorld);

/**
 * The system call which maps shared memory created by another process. The sizes must match
 * exactly, but the virtual address into which you map may be different in each process.
 * Requesting more permissions than were delegated to you results in EPERM. Returns 0 on success,
 * -1 on error.
 */
int sys_shmap(uint64_t addr, uint64_t size, uint64_t id, int prot);

#endif
