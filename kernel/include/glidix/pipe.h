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

#ifndef __glidix_pipe_h
#define __glidix_pipe_h

#include <glidix/common.h>
#include <glidix/vfs.h>
#include <glidix/semaphore.h>

#define	PIPE_READ			((void*)1)
#define	PIPE_WRITE			((void*)2)

#define	PIPE_BUFFER_SIZE		1024

/**
 * Describes a pipe.
 */
typedef struct
{
	/**
	 * These semaphores are signalled when the pipe/FIFO is opened for read or write, respectively.
	 * They are never waited (only polled) and are used to block opens until the pipe is ready.
	 */
	Semaphore semReadOpen;
	Semaphore semWriteOpen;
	
	/**
	 * Semaphores counting the number of unread bytes, and the number of free bytes, in the pipe buffer,
	 * respectively.
	 */
	Semaphore semBufferRead;
	Semaphore semBufferFree;
	
	/**
	 * Pipe buffer, and offsets.
	 */
	char buffer[PIPE_BUFFER_SIZE];
	off_t offRead;
	off_t offWrite;
	
	/**
	 * Atomic counters for the number of times the pipe was opened in read or write mode.
	 */
	int cntRead;
	int cntWrite;
	
	/**
	 * Signalled when all writers of the pipe have been closed.
	 */
	Semaphore semHangup;
	
	/**
	 * Lock to protect the pipe.
	 */
	Semaphore lock;
} Pipe;

int sys_pipe(int *pipefd);
int sys_pipe2(int *pipefd, int flags);
int sys_mkfifo(const char *upath, mode_t mode);

#endif
