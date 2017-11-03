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

#include <string.h>
#include <errno.h>

static char _strerror_buf[256];
static const char *_strerror_msg[84] = {
	"No error",
	"Argument list too long.",
	"Access denied.",
	"Address in use.",
	"Address not available.",
	"Address family not supported.",
	"Resource unavailable, try again.",
	"Connection already in progress.",
	"Bad file descriptor.",
	"Bad message.",
	"Device or resource busy.",
	"Operation canceled.",
	"No child processes.",
	"Connection aborted.",
	"Connection refused.",
	"Connection reset.",
	"Resource deadlock would occur.",
	"Destination address required.",
	"Mathematics argument out of domain of function.",
	"Reserved.",
	"File exists.",
	"Bad address.",
	"File too large.",
	"Host is unreachable.",
	"Identifier removed.",
	"Illegal byte sequence.",
	"Operation in progress.",
	"Interrupted function.",
	"Invalid argument.",
	"I/O error.",
	"Socket is connected.",
	"Is a directory.",
	"Too many levels of symbolic links.",
	"Too many open files.",
	"Too many links.",
	"Message too large.",
	"Reserved.",
	"Filename too long.",
	"Network is down.",
	"Connection aborted by network.",
	"Network unreachable.",
	"Too many files open in system.",
	"No buffer space available.",
	"No message is available on the STREAM head read queue.",
	"Reserved.",
	"No such device.",
	"No such file or directory.",
	"Executable file format error.",
	"No locks available.",
	"Reserved.",
	"Not enough space.",
	"No message of the desired type.",
	"Protocol not available.",
	"No space left on device.",
	"No STREAM resources.",
	"Reserved",
	"Not a STREAM.",
	"Reserved",
	"Function not supported.",
	"The socket is not connected.",
	"Not a directory.",
	"Directory not empty.",
	"Not a socket.",
	"Not supported.",
	"Inappropriate I/O control operation.",
	"No such device or address.",
	"Operation not supported on socket.",
	"Value too large to be stored in data type.",
	"Operation not permitted.",
	"Broken pipe.",
	"Protocol error.",
	"Protocol not supported.",
	"Protocol wrong type for socket.",
	"Result too large.",
	"Read-only file system.",
	"Invalid seek.",
	"No such process.",
	"Reserved.",
	"Timer expired.",
	"Reserved",
	"Connection timed out.",
	"Text file busy.",
	"Operation would block (may be the same value as [EAGAIN]).",
	"Cross-device link."
};

char *strerror(int errnum)
{
	strerror_r(errnum, _strerror_buf, 256);
	return _strerror_buf;
};

int strerror_r(int errnum, char *buf, size_t buflen)
{
	const char *msg;
	if ((errnum > EXDEV) || (errnum < 1))
	{
		msg = "Unknown error";
	}
	else
	{
		msg = _strerror_msg[errnum];
	};

	size_t len = strlen(msg)+1;
	if (len > buflen) len = buflen;
	memcpy(buf, msg, len);

	return 0;
};
