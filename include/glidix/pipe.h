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

#ifndef __glidix_pipe_h
#define __glidix_pipe_h

#include <glidix/common.h>
#include <glidix/vfs.h>
#include <glidix/semaphore.h>

#define	SIDE_READ			1
#define	SIDE_WRITE			2

#define	PIPE_BUFFER_SIZE		1024

typedef struct
{
	/**
	 * The semaphore which protects the pipe.
	 */
	Semaphore			sem;
	
	/**
	 * The semaphore which counts the number of bytes in the pipe.
	 */
	Semaphore			counter;
	
	/**
	 * This semaphore gets signalled when the pipe is terminated.
	 */
	Semaphore			semHangup;
	
	/**
	 * Buffer and read/write pointers.
	 */
	char				buffer[PIPE_BUFFER_SIZE];
	off_t				roff;
	off_t				woff;
	
	/**
	 * Which sides are open (OR of SIDE_READ and/or SIDE_WRITE or 0).
	 */
	int				sides;
} Pipe;

int sys_pipe(int *pipefd);

#endif
