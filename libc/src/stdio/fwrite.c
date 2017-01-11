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

#include <stdio.h>
#include <string.h>

size_t fwrite(const void *buf, size_t size, size_t count, FILE *fp)
{
	if ((fp->_flags & __FILE_WRITE) == 0)
	{
		fp->_flags |= __FILE_FERROR;
		return 0;
	};

	const void *buf_org = buf;

	size_t ret = size * count;
	size_t rsz = size * count;
	while (fp->_bufsiz < rsz)
	{
		if (fp->_flush == NULL)
		{
			// truncate
			rsz = fp->_bufsiz;
		}
		else
		{
			if (fp->_bufsiz != 0) memcpy(fp->_wrbuf, buf, fp->_bufsiz);
			fp->_wrbuf += fp->_bufsiz;
			buf = (void*) ((char*) buf + fp->_bufsiz);
			rsz -= fp->_bufsiz;
			fp->_bufsiz = 0;
			fflush(fp);
		};
	};

	if (rsz != 0) memcpy(fp->_wrbuf, buf, rsz);
	fp->_wrbuf += rsz;
	fp->_bufsiz -= rsz;
	const char *scan = (const char *) buf_org;

	while (rsz--)
	{
		if (*scan == fp->_trigger)
		{
			if (fp->_flush != NULL) fflush(fp);
		};

		scan++;
	};

	//BREAKPOINT();
	if (fp->_flush != NULL) fflush(fp);		// TODO: shit, we need some way to call fflush() at exit instead.
	return ret;
};
