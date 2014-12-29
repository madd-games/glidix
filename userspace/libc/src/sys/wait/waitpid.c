/*
	Glidix Runtime

	Copyright (c) 2014, Madd Games.
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

#include <sys/wait.h>
#include <sys/glidix.h>
#include <errno.h>

pid_t waitpid(pid_t pid, int *stat_loc, int flags)
{
	/**
	 * _glidix_pollpid() is a funny one. When called without the WNOHANG flag, it is equivalent
	 * to calling it with WNOHANG and then calling pause(), except it is safe and doesn't deadlock
	 * us. So if WNOHANG is not passed, we call _glidix_pollpid() as normal, then we are essentially
	 * waiting for a signal; that signal could be SIGCHLD, so we call _glidix_pollpid() again, and if
	 * it fails the second time, that means we were interrupted (EINTR).
	 */
	if (flags & WNOHANG)
	{
		return _glidix_pollpid(pid, stat_loc, flags);
	}
	else
	{
		pid_t ret = _glidix_pollpid(pid, stat_loc, flags);
		if (ret == -1)
		{
			ret = _glidix_pollpid(pid, stat_loc, flags | WNOHANG);
			if (ret == -1)
			{
				_glidix_seterrno(EINTR);
				return -1;
			};
		};

		return ret;
	};
};
