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
#include <dirent.h>
#include <unistd.h>
#include <errno.h>
#include <string.h>

struct dirent *readdir(DIR *dirp)
{
	struct dirent *out;
	readdir_r(dirp, &dirp->_dirbuf, &out);
	return out;
};

int readdir_r(DIR *dirp, struct dirent *entry, struct dirent **result)
{
	if (dirp->_idx == 0)
	{
		struct stat st;
		st.st_ino = 2;
		stat(".", &st);
		strcpy(entry->d_name, ".");
		entry->d_ino = st.st_ino;
		dirp->_idx = 1;
		*result = entry;
		return 0;
	}
	else if (dirp->_idx == 1)
	{
		struct stat st;
		st.st_ino = 2;
		stat("..", &st);
		strcpy(entry->d_name, "..");
		entry->d_ino = st.st_ino;
		dirp->_idx = 2;
		*result = entry;
		return 0;
	};

	ssize_t ret = 0;
	if (dirp->_fd != -2) ret = read(dirp->_fd, entry, sizeof(struct dirent));
	if (ret == 0)
	{
		*result = NULL;
		_glidix_seterrno(ENOENT);
		return -1;
	};

	*result = entry;
	return 0;
};
