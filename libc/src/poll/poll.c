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
#include <fcntl.h>
#include <poll.h>
#include <unistd.h>

int poll(struct pollfd *fds, nfds_t nfds, int timeout)
{
	int flags;
	uint64_t nanotimeout;
	
	if (timeout < 0)
	{
		flags = 0;
		nanotimeout = 0;
	}
	else if (timeout == 0)
	{
		flags = O_NONBLOCK;
		nanotimeout = 0;
	}
	else
	{
		flags = 0;
		nanotimeout = (uint64_t)timeout * 1000000UL;		// 10^6 nanoseconds in a millisecond
	};
	
	int maxFiles = sysconf(_SC_OPEN_MAX);
	uint8_t bitmap[maxFiles];
	memset(bitmap, 0, maxFiles);
	
	unsigned int i;
	int numErrors = 0;
	for (i=0; i<nfds; i++)
	{
		if ((fds[i].fd < 0) || (fds[i].fd >= maxFiles))
		{
			fds[i].revents = POLLNVAL;
			numErrors++;
		}
		else
		{
			bitmap[fds[i].fd] |= (uint8_t) fds[i].events;
		};
	};
	
	if (numErrors != 0)
	{
		return numErrors;
	};
	
	int result = _glidix_fpoll(bitmap, bitmap, flags, nanotimeout);
	if (result == -1)
	{
		return -1;
	};
	
	int out = 0;
	for (i=0; i<nfds; i++)
	{
		fds[i].revents = (short int) bitmap[fds[i].fd];
		if (bitmap[fds[i].fd] != 0) out++;
	};
	
	return out;
};
