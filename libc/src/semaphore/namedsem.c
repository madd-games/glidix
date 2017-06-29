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


#include <sys/call.h>
#include <semaphore.h>
#include <errno.h>
#include <fcntl.h>
#include <unistd.h>
#include <sys/mman.h>

#define SEMDIR_PREFIX "/run/sem"

sem_t 	*sem_open(const char *name, int oflag, ... )
{
	mode_t mode;
	unsigned int value;
	
	
	if (name[0] != '/')
	{
		errno = EINVAL;
		return SEM_FAILED;
	};
		
	size_t len = strlen(name);
	if (len >= (PATH_MAX - strlen(SEMDIR_PREFIX)))
	{
		errno = ENAMETOOLONG;
		return SEM_FAILED;
	};
	
	char sempath[PATH_MAX];
	sprintf(sempath, SEMDIR_PREFIX "%s", name);
	
	if (oflag & O_CREAT)
	{
		va_list arguments;
		va_start ( arguments, oflag );
		mode = va_arg ( arguments, mode_t );
		value = va_arg ( arguments, unsigned int );
		va_end ( arguments );
	};
	
	int fd = open(sempath, oflag | O_RDWR, mode);
	if(fd == -1)
	{
		return SEM_FAILED;
	};
	
	void* map_ret = mmap(NULL, sizeof(sem_t), PROT_READ | PROT_WRITE, MAP_SHARED, fd, 0);
	
	if (map_ret == MAP_FAILED)
	{
		int errnum = errno;
		close(fd);
		errno = errnum;
		return SEM_FAILED;
	};
	
	sem_t *sem = (sem_t*) map_ret;
	
	lockf(fd, F_LOCK, 0);
	
	struct stat st;
	fstat(fd, &st);
	if(st.st_size != sizeof(sem_t))
	{
		if(!(oflag & O_CREAT))
		{
			close(fd);
			munmap( sem, sizeof(sem_t));
			errno = ENOENT;
			return SEM_FAILED;
		};
		ftruncate(fd, sizeof(sem_t));
		sem_init(sem, 1, value);
	};
	close(fd);
	return sem;
};

int sem_close(sem_t *sem)
{
	munmap(sem, sizeof(sem_t));
	return 0;
};

int sem_unlink(const char *name)
{
	if (name[0] != '/')
	{
		errno = ENOENT;
		return -1;
	};
		
	size_t len = strlen(name);
	if (len >= (PATH_MAX - strlen(SEMDIR_PREFIX)))
	{
		errno = ENAMETOOLONG;
		return -1;
	};
	
	char sempath[PATH_MAX];
	sprintf(sempath, SEMDIR_PREFIX "%s", name);
	
	return unlink(sempath);
};