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

#ifndef __glidix_ptty_h
#define __glidix_ptty_h

/**
 * Pseudo-terminal support.
 */

#include <glidix/common.h>
#include <glidix/term.h>
#include <glidix/semaphore.h>
#include <glidix/devfs.h>

#define	TERM_BUFFER_SIZE			4096

typedef struct
{
	/**
	 * The semaphore which protects the terminal.
	 */
	Semaphore				sem;
	
	/**
	 * The semaphores which count the number of bytes waiting on the master and
	 * slave handles.
	 */
	Semaphore				masterCounter;
	Semaphore				slaveCounter;
	
	/**
	 * Terminal state.
	 */
	struct termios				state;
	
	/**
	 * Master input buffer.
	 */
	char					masterBuffer[TERM_BUFFER_SIZE];
	int					masterPut;
	int					masterFetch;
	
	/**
	 * Slave input buffer.
	 */
	char					slaveBuffer[TERM_BUFFER_SIZE];
	int					slavePut;
	int					slaveFetch;
	
	/**
	 * Line buffer in canonical mode.
	 */
	char					lineBuffer[TERM_BUFFER_SIZE];
	int					lineSize;
	
	/**
	 * Session ID and foreground process group ID.
	 */
	int					sid;
	int					pgid;
	
	/**
	 * Device file handle representing the slave.
	 */
	Device					devSlave;
	
	/**
	 * Path to the slave.
	 */
	char					slaveName[256];
	
	/**
	 * Reference count.
	 */
	int					refcount;
} PseudoTerm;

void pttyInit();

#endif
