/*
	Glidix Shell Utilities

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

#include <stdio.h>
#include <signal.h>
#include <stdlib.h>
#include <unistd.h>
#include <sys/glidix.h>
#include <sys/fsinfo.h>
#include <sys/mman.h>
#include <sys/call.h>
#include <sys/systat.h>
#include <string.h>
#include <fcntl.h>
#include <sys/wait.h>
#include <sys/ioctl.h>
#include <sys/stat.h>
#include <dirent.h>
#include <time.h>
#include <errno.h>
#include <sys/mount.h>
#include <sys/statvfs.h>
#include <sys/module.h>

void printFrames(const char *label, size_t frames)
{
	printf("%-40s %-20lu %-10lu MB\n", label, frames, frames/256);
};

int main(int argc, char *argv[])
{
	struct system_state sst;
	if (__syscall(__SYS_systat, &sst, sizeof(struct system_state)) != 0)
	{
		fprintf(stderr, "%s: failed to get system state: %s\n", argv[0], strerror(errno));
		return 1;
	};
	
	printf("%-40s %-20s %-10s\n", "TYPE", "FRAMES", "SIZE");
	printFrames("Total memory:", sst.sst_frames_total);
	printFrames("Memory in use:", sst.sst_frames_used - sst.sst_frames_cached);
	printFrames("Cache memory:", sst.sst_frames_cached);
	printFrames("Available memory:", sst.sst_frames_total - sst.sst_frames_used + sst.sst_frames_cached);
	printFrames("Unallocated memory:", sst.sst_frames_total - sst.sst_frames_used);
	
	return 0;
};
