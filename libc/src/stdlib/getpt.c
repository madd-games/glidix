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

#include <stdlib.h>
#include <fcntl.h>
#include <errno.h>
#include <termios.h>

int posix_openpt(int flags)
{
	return open("/dev/ptmx", flags);
};

int getpt()
{
	return open("/dev/ptmx", O_RDWR | O_NOCTTY);
};

int grantpt(int fd)
{
	int status = ioctl(fd, __IOCTL_TTY_GRANTPT);
	if (status == -1)
	{
		if (errno == EINVAL)
		{
			errno = ENOTTY;
		};
	};

	return status;
};

int unlockpt(int fd)
{
	int status = ioctl(fd, __IOCTL_TTY_UNLOCKPT);
	if (status == -1)
	{
		if (errno == EINVAL)
		{
			errno = ENOTTY;
		};
	};

	return status;
};

int ptsname_r(int fd, char *buffer, size_t buflen)
{
	if (buflen < 256)
	{
		errno = ERANGE;
		return -1;
	};
	
	int status = ioctl(fd, __IOCTL_TTY_PTSNAME, buffer);
	if (status == -1)
	{
		if (errno == EINVAL)
		{
			errno = ENOTTY;
		};
	};

	return status;
};

char *ptsname(int fd)
{
	static char buffer[256];
	if (ptsname_r(fd, buffer, 256) == 0)
	{
		return buffer;
	};
	
	return NULL;
};
