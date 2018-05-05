/*
	Glidix Filesystem Tools

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

#ifndef FSTOOLS_H_
#define FSTOOLS_H_

#include <stdint.h>
#include <stdlib.h>

/**
 * Describes a magic value that may be searched for in files.
 */
typedef struct
{
	/**
	 * Offset within file to start the search in.
	 */
	off_t					offMin;
	
	/**
	 * Maximum offset, i.e. the first offset at which NOT to look for.
	 * -1 means unlimited.
	 */
	off_t					offMax;
	
	/**
	 * Number of bytes to skip between each scan; normally it is 1. Set it to the
	 * required alignment of the magic value if necessary.
	 */
	off_t					offStep;
	
	/**
	 * Size of the magic value.
	 */
	size_t					size;
	
	/**
	 * The magic value.
	 */
	uint8_t					magic[];
} FSMagic;

/**
 * Describes a MIME type.
 */
typedef struct FSMimeType_
{
	/**
	 * Next in the list.
	 */
	struct FSMimeType_*			next;
	
	/**
	 * Name of the type (e.g. "text/x-csrc").
	 */
	char					mimename[256];
	
	/**
	 * Array of filename patterns, and their amount. The patterns are on the heap
	 * (loaded with strdup() ).
	 */
	char**					filenames;
	size_t					numFilenames;
	
	/**
	 * Magic value (at start of file) and the size of it.
	 */
	size_t					magicSize;
	uint8_t*				magic;
	
	/**
	 * Label attached to the file type (a human-readable string). By default, it's just
	 * the mimetype itself.
	 */
	char*					label;
	
	/**
	 * Name of the icon attached to the file. The icon is a 64x64 image, which is sometimes
	 * scaled to 16x16 by GWM.
	 */
	char*					iconName;
} FSMimeType;

/**
 * Initialize the library by loading the MIME database.
 */
void fsInit();

/**
 * Quit the library.
 */
void fsQuit();

/**
 * Determine the type of a file, and return the description (FSMimeType). Always succeeds, unless
 * the library is not initialized.
 */
FSMimeType* fsGetType(const char *filename);

#endif
