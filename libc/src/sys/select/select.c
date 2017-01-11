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

#include <sys/glidix.h>
#include <sys/select.h>
#include <fcntl.h>
#include <poll.h>
#include <unistd.h>
#include <errno.h>

int select(int nfds, fd_set *readfds, fd_set *writefds, fd_set *exceptfds, struct timeval *timeout)
{
	if ((nfds < 0) || (nfds > FD_SETSIZE))
	{
		errno = EINVAL;
		return -1;
	};
	
	int flags;
	uint64_t nanotimeout;
	
	if (timeout == NULL)
	{
		flags = 0;
		nanotimeout = 0;
	}
	else if ((timeout->tv_sec == 0) && (timeout->tv_usec == 0))
	{
		flags = O_NONBLOCK;
		nanotimeout = 0;
	}
	else
	{
		flags = 0;
		nanotimeout = (uint64_t)timeout->tv_sec * 1000000000UL + (uint64_t)timeout->tv_usec * 1000UL;
	};
	
	int maxFiles = sysconf(_SC_OPEN_MAX);
	uint8_t bitmap[maxFiles];
	memset(bitmap, 0, maxFiles);
	
	int i;
	for (i=0; i<nfds; i++)
	{
		if (readfds != NULL)
		{
			if (FD_ISSET(i, readfds))
			{
				bitmap[i] |= POLLIN;
			};
		};
		
		if (writefds != NULL)
		{
			if (FD_ISSET(i, writefds))
			{
				bitmap[i] |= POLLOUT;
			};
		};
		
		// we can ignore 'exceptfds' because errors are reported anyway
	};
	
	if (readfds != NULL) FD_ZERO(readfds);
	if (writefds != NULL) FD_ZERO(writefds);
	if (exceptfds != NULL) FD_ZERO(exceptfds);
	
	int status = _glidix_fpoll(bitmap, bitmap, flags, nanotimeout);
	if (status == -1)
	{
		return -1;
	};
	
	int bitsSet = 0;
	for (i=0; i<nfds; i++)
	{
		if (readfds != NULL)
		{
			if (bitmap[i] & POLLIN)
			{
				FD_SET(i, readfds);
				bitsSet++;
			};
		};
		
		if (writefds != NULL)
		{
			if (bitmap[i] & POLLOUT)
			{
				FD_SET(i, writefds);
				bitsSet++;
			};
		};
		
		if (exceptfds != NULL)
		{
			if (bitmap[i] & (POLLERR | POLLHUP | POLLNVAL))
			{
				FD_SET(i, exceptfds);
				bitsSet++;
			};
		};
	};
	
	return bitsSet;
};
