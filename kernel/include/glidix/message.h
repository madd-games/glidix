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

#ifndef __glidix_message_h
#define __glidix_message_h

/**
 * Glidix message queues.
 * A message queue is identified by a pid and file descriptor.
 * There are 2 types of queues: a 'server queue' and a 'client queue'.
 * A server queue just listens to messages from any process, and may
 * respond to them. A client queue must be connected to a specific pid-fd
 * pair. The server is informed whenever a new client connects, and also
 * when one hangs up.
 */

#include <glidix/common.h>

#define	MQ_CONNECT			0
#define	MQ_INCOMING			1
#define	MQ_HANGUP			2

/**
 * Describes an arriving message.
 */
typedef struct
{
	/**
	 * The type of message:
	 * MQ_CONNECT - the specified pid-fd has just connected and may be sending messages.
	 * MQ_INCOMING - the specified pid-fd has just sent a message.
	 * MQ_HANGUP - the specified pid-fd has just disconnected (closed its queue).
	 */
	int				type;
	
	/**
	 * The pid and fd of the other side.
	 */
	int				pid;
	int				fd;
	
	/**
	 * The UID and GID that sent this message.
	 */
	uid_t				uid;
	gid_t				gid;
} MessageInfo;

/**
 * System call that creates a server queue. Upon success, returns the queue file descriptor,
 * which may be coupled with the result of getpid() to uniquely identify the queue in the system.
 * Upon failure, returns -1 and sets errno appropriately.
 */
int sys_mqserver();

/**
 * System call that creates a client queue. The arguments specify the pid-fd of the server. Upon
 * success, a client queue file descriptor is returned. Upon failure, -1 is returned and errno
 * set appropriately.
 */
int sys_mqclient(int pid, int fd);

/**
 * Sends a message from the specified fd (and its creator pid), to the specified pid-fd. If this is
 * a client queue, the destination pid-fd must be the same as the one passed to _glidix_mqclient().
 * Returns 0 on success, or -1 on error and sets errno appropriately.
 */
int sys_mqsend(int fd, int targetPid, int targetFD, const void *msg, size_t msgsize);

/**
 * Wait for a message to arrive and store it in the specified buffer. Also returns the information
 * into the given buffer. Returns the message size on success, -1 on error and sets errno appropriately.
 */
ssize_t sys_mqrecv(int fd, MessageInfo *info, void *buffer, size_t bufsize);

#endif
