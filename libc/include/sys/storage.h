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

#ifndef _SYS_STORAGE_H
#define _SYS_STORAGE_H

#include <sys/glidix.h>
#include <sys/ioctl.h>

#define	IOCTL_SDI_IDENTITY			_GLIDIX_IOCTL_ARG(SDIdentity, _GLIDIX_IOCTL_INT_SDI, 0)

#define	SD_READONLY				(1 << 0)	/* device is read-only */
#define	SD_HANGUP				(1 << 1)	/* device hanged up */
#define	SD_EJECTABLE				(1 << 2)	/* device is ejectable (memory can be replaced, such as CD-ROM) */
#define	SD_REMOVEABLE				(1 << 3)	/* the whole drive can be removed at runtime */

typedef union
{
	struct
	{
		int				flags;
		int				partIndex;
		size_t				offset;
		size_t				size;
		char				name[128];
	};
	
	/* force the size to 256 bytes */
	char _size[256];
} SDIdentity;

#endif
