/*
	Glidix Runtime
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

#include <fcntl.h>
#include <unistd.h>
#include <string.h>
#include <errno.h>

int lockf(int fd, int cmd, off_t len)
{
	struct flock lock;
	memset(&lock, 0, sizeof(struct flock));
	lock.l_whence = SEEK_CUR;
	lock.l_start = 0;
	lock.l_len = len;
	lock.l_type = F_WRLCK;
	
	if (cmd == F_LOCK)
	{
		return fcntl(fd, F_SETLKW, &lock);
	}
	else if (cmd == F_TLOCK)
	{
		return fcntl(fd, F_SETLK, &lock);
	}
	else if (cmd == F_ULOCK)
	{
		lock.l_type = F_UNLCK;
		return fcntl(fd, F_SETLK, &lock);
	}
	else if (cmd == F_TEST)
	{
		if (fcntl(fd, F_GETLK, &lock) != 0)
		{
			return -1;
		};
		
		if (lock.l_type == F_UNLCK)
		{
			return 0;
		};
		
		if (lock.l_pid == getpid())
		{
			return 0;
		};
		
		errno = EAGAIN;
		return -1;
	}
	else
	{
		errno = EINVAL;
		return -1;
	};
};
