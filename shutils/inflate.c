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
	z_stream infstream;
	infstream.zalloc = NULL;
	infstream.zfree = NULL;
	infstream.opaque = NULL;
	infstream.avail_in = 0;
	infstream.avail_out = 0;
	
	if (inflateInit2(&infstream, MAX_WBITS | 32) != Z_OK)
	{
		fprintf(stderr, "%s: inflateInit2 failed\n", argv[0]);
		return 1;
	};
	
	while (1)
	{
		unsigned char buffer[BUFFER_SIZE];
		ssize_t sz = read(0, buffer, BUFFER_SIZE);
		if (sz == 0)
		{
			// flush the buffer
			infstream.avail_out = 0;
			while (infstream.avail_out == 0)
			{
				infstream.next_out = buffer;
				infstream.avail_out = BUFFER_SIZE;
				
				inflate(&infstream, Z_FINISH);
				write(1, buffer, BUFFER_SIZE - infstream.avail_out);
			};
			
			inflateEnd(&infstream);
			return 0;
		};
		
		if (sz == -1)
		{
			fprintf(stderr, "%s: read error: %s\n", argv[0], strerror(errno));
			return 1;
		};
		
		infstream.next_in = buffer;
		infstream.avail_in = sz;
		infstream.avail_out = 0;
		
		unsigned char outbuf[4096];
		while (infstream.avail_out == 0 && infstream.avail_in != 0)
		{
			infstream.next_out = outbuf;
			infstream.avail_out = BUFFER_SIZE;
			
			inflate(&infstream, 0);
			write(1, outbuf, BUFFER_SIZE - infstream.avail_out);
		};
	};
	
	return 0;
};
