/*
	Glidix Runtime

	Copyright (c) 2014-2015, Madd Games.
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

#include <sys/stat.h>
#include <stdio.h>
#include <unistd.h>
#include <errno.h>

int rename(const char *oldpath, const char *newpath)
{
	struct stat st_old, st_new;
	if (lstat(oldpath, &st_old) != 0)
	{
		return -1;
	};

	if (lstat(newpath, &st_new) == 0)
	{
		if ((st_new.st_dev == st_old.st_dev) && (st_new.st_ino == st_old.st_ino))
		{
			// same file, do nothing.
			return 0;
		};

		if (st_new.st_dev != st_old.st_dev)
		{
			// don't even try to do a cross-device rename
			errno = EXDEV;
			return -1;
		};

		if (unlink(newpath) != 0)
		{
			return -1;
		};
	};

	if (link(oldpath, newpath) != 0)
	{
		return -1;
	};

	if (unlink(oldpath) != 0)
	{
		return -1;
	};

	return 0;
};
