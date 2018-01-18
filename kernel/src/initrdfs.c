/*
	Glidix kernel

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

#include <glidix/initrdfs.h>
#include <glidix/string.h>
#include <glidix/memory.h>
#include <glidix/console.h>
#include <glidix/semaphore.h>
#include <glidix/sched.h>
#include <glidix/ftree.h>

typedef struct
{
	char			filename[100];
	char			mode[8];
	char			uid[8];
	char			gid[8];
	char			size[12];
	char			mtime[12];
	char			checksum[8];
	char			type;
	char			link_name[100];
	char			ustar[5];
	char			ustart_garb[3];
	char			owner_uname[32];
	char			owner_gname[32];
	char			dev_major[8];
	char			dev_minor[8];
	char			prefix[155];
	char			pad[12];
} PACKED TarHeader;

TarHeader *masterHeader;
SECTION(".initrd") uint8_t initrdImage[8*1024*1024];

#if 0
static uint64_t parseOct(const char *data)
{
	uint64_t out = 0;
	while (*data != 0)
	{
		out = out * 8 + ((*data++)-'0');
	};
	return out;
};
#endif

void initInitrdfs(KernelBootInfo *info)
{
	masterHeader = (TarHeader*) initrdImage;

	//TarHeader *header = masterHeader;
	panic("this is TODO");
};
