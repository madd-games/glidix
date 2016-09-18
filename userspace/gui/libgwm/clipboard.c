/*
	Glidix GUI

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

#include <libgwm.h>
#include <unistd.h>
#include <fcntl.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <errno.h>

static int openClipboard()
{
	char cbPath[256];
	sprintf(cbPath, "/run/clipboard/%lu", geteuid());
	
	int fd = open(cbPath, O_RDWR | O_CREAT | O_CLOEXEC, 0600);
	if (fd == -1) return -1;
	
	int status;
	do
	{
		status = lockf(fd, F_LOCK, 0);
	} while ((status == -1) && (errno == EINTR));
	
	if (status == -1)
	{
		close(fd);
		return -1;
	};
	
	return fd;
};

void gwmClipboardPutText(const char *text, size_t textSize)
{
	int fd = openClipboard();
	if (fd == -1) return;
	
	if (ftruncate(fd, 0) != 0)
	{
		close(fd);
		return;
	};
	
	GWMClipboardHeader header;
	memset(&header, 0, 64);
	header.cbMagic = GWM_CB_MAGIC;
	header.cbType = GWM_CB_TEXT;
	
	pwrite(fd, &header, 64, 0);
	pwrite(fd, text, textSize, 64);
	close(fd);
};

char* gwmClipboardGetText(size_t *sizeOut)
{
	int fd = openClipboard();
	if (fd == -1) return NULL;
	
	GWMClipboardHeader header;
	if (pread(fd, &header, 64, 0) != 64)
	{
		close(fd);
		return NULL;
	};
	
	if (header.cbMagic != GWM_CB_MAGIC)
	{
		close(fd);
		return NULL;
	};
	
	if (header.cbType != GWM_CB_TEXT)
	{
		close(fd);
		return NULL;
	};
	
	struct stat st;
	if (fstat(fd, &st) != 0)
	{
		close(fd);
		return NULL;
	};
	
	*sizeOut = st.st_size - 64;
	char *buffer = (char*) malloc(st.st_size - 64 + 1);
	buffer[st.st_size - 64] = 0;
	pread(fd, buffer, st.st_size - 64, 64);
	close(fd);
	
	return buffer;
};
