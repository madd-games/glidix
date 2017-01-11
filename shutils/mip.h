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

#ifndef MIP_H
#define MIP_H

/**
 * Structures of the Madd Installable Package (MIP) file format.
 */

#include <stdlib.h>
#include <stdint.h>

#define	MIP_MODE_TYPEMASK	0xF000

#define	MIP_MODE_REGULAR	0x0000
#define	MIP_MODE_SYMLINK	0x7000
#define	MIP_MODE_DIRECTORY	0x8000

typedef struct
{
	uint8_t			magic[3];			/* MIP */
	uint8_t			version;			/* 1 or 2 */
	
	/* only if version >= 2 */
	char			setupFile[256];			/* file to execute after unpacking, or empty string */
} MIPHeader;

typedef struct
{
	char			filename[256];
	uint16_t		mode;				/* bit 0x8000 means directory */
	uint64_t		size;
} MIPFile;

#endif
