/*
	Glidix kernel
	
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

#ifndef __glidix_ipreasm_h
#define __glidix_ipreasm_h

#include <glidix/common.h>

typedef struct IPFragment_
{
	int				lastFragment;		// 1 if last fragment
	uint32_t			fragOffset;
	uint32_t			fragSize;
	struct IPFragment_*		prev;
	struct IPFragment_*		next;
	char				data[];
} IPFragment;

typedef struct IPFragmentList_
{
	int				family;
	int				deadlineTicks;
	char				srcaddr[16];
	char				dstaddr[16];
	uint32_t			id;
	int				proto;
	IPFragment*			fragList;
	struct IPFragmentList_*		prev;
	struct IPFragmentList_*		next;
} IPFragmentList;

void ipreasmInit();
void ipreasmPass(int family, char *srcaddr, char *dstaddr, int proto, uint32_t id, uint32_t fragOff, char *data, size_t fragSize, int moreFrags);

#endif
