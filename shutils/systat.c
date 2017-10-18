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

#include <sys/call.h>
#include <sys/systat.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <errno.h>

char *idToString(uint8_t *id)
{
	char *result = (char*) malloc(33);
	size_t i;
	
	for (i=0; i<16; i++)
	{
		sprintf(&result[2*i], "%02hhX", id[i]);
	};
	
	return result;
};

int main(int argc, char *argv[])
{
	struct system_state sst;
	memset(&sst, 0, sizeof(struct system_state));
	
	if (__syscall(__SYS_systat, &sst, sizeof(struct system_state)) != 0)
	{
		fprintf(stderr, "%s: %s\n", argv[0], strerror(errno));
		return 1;
	};
	
	printf("Boot ID:       %s\n", idToString(sst.sst_bootid));
	return 0;
};
