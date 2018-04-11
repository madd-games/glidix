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
#include <stdlib.h>
#include <zlib.h>
#include <string.h>
#include <errno.h>

#define	BUFFER_SIZE			4096

int main(int argc, char *argv[])
{
	z_stream defstream;
	defstream.zalloc = NULL;
	defstream.zfree = NULL;
	defstream.opaque = NULL;
	defstream.avail_in = 0;
	defstream.avail_out = 0;
	
	if (deflateInit2(&defstream, Z_BEST_COMPRESSION, Z_DEFLATED, MAX_WBITS + 16, 8, Z_DEFAULT_STRATEGY) != Z_OK)
	{
		fprintf(stderr, "%s: deflateInit2 failed\n", argv[0]);
		return 1;
	};
	
	while (1)
	{
		unsigned char buffer[BUFFER_SIZE];
		ssize_t sz = read(0, buffer, BUFFER_SIZE);
		if (sz == 0)
		{
			// flush the buffer
			defstream.avail_out = 0;
			while (defstream.avail_out == 0)
			{
				defstream.next_out = buffer;
				defstream.avail_out = BUFFER_SIZE;
				
				deflate(&defstream, Z_FINISH);
				write(1, buffer, BUFFER_SIZE - defstream.avail_out);
			};
			
			deflateEnd(&defstream);
			return 0;
		};
		
		if (sz == -1)
		{
			fprintf(stderr, "%s: read error: %s\n", argv[0], strerror(errno));
			return 1;
		};
		
		defstream.next_in = buffer;
		defstream.avail_in = sz;
		defstream.avail_out = 0;
		
		unsigned char outbuf[4096];
		while (defstream.avail_out == 0 && defstream.avail_in != 0)
		{
			defstream.next_out = outbuf;
			defstream.avail_out = BUFFER_SIZE;
			
			deflate(&defstream, 0);
			write(1, outbuf, BUFFER_SIZE - defstream.avail_out);
		};
	};
	
	return 0;
};
