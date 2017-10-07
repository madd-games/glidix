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

#include <sys/debug.h>
#include <sys/wait.h>
#include <unistd.h>
#include <string.h>
#include <fcntl.h>
#include <stdlib.h>
#include <signal.h>

char* __dbg_addr2line(const char *path, uint64_t offset)
{
	int pipefd[2];
	if (pipe2(pipefd, O_CLOEXEC) != 0)
	{
		return strdup("(pipe failed)");
	};
	
	pid_t pid = fork();
	if (pid == -1)
	{
		return strdup("??");
	}
	else if (pid == 0)
	{
		close(pipefd[0]);
		close(1);
		dup(pipefd[1]);
		close(2);
		open("/dev/null", O_RDWR);
		
		execl("/usr/bin/objdump", "objdump", "--dwarf=decodedline", path, NULL);
		_exit(1);
	}
	else
	{
		close(pipefd[1]);
		char *result = NULL;
		
		uint64_t prevAddr = 0;
		unsigned long prevLineno = 0;
		char *prevFile = NULL;
		
		FILE *fp = fdopen(pipefd[0], "r");
		char linebuf[1024];
		while (fgets(linebuf, 1024, fp) != NULL)
		{
			char *endline = strchr(linebuf, '\n');
			if (endline != NULL) *endline = 0;
			
			char *saveptr;
			char *filename = strtok_r(linebuf, " \t", &saveptr);
			if (filename == NULL) continue;
			
			char *linespec = strtok_r(NULL, " \t", &saveptr);
			if (linespec == NULL) continue;
			
			char *addrspec = strtok_r(NULL, " \t", &saveptr);
			if (addrspec == NULL) continue;
			
			char *end = strtok_r(NULL, " \t", &saveptr);
			if (end != NULL) continue;
			
			if (memcmp(addrspec, "0x", 2) == 0) addrspec += 2;
			
			unsigned long lineno = strtoul(linespec, &end, 10);
			if (*end != 0) continue;
			
			unsigned long address = strtoul(addrspec, &end, 16);
			if (*end != 0) continue;

			if (prevAddr < offset && address > offset && prevFile != NULL)
			{
				address = offset;
				lineno = prevLineno;
				filename = prevFile;
			};
			
			if (address == offset)
			{
				char buffer[2096];
				sprintf(buffer, "%s:%lu", filename, lineno);
				free(prevFile);
				
				kill(pid, SIGKILL);
				result = strdup(buffer);
				break;
			};
			
			prevLineno = lineno;
			prevAddr = address;
			free(prevFile);
			prevFile = strdup(filename);
		};
		fclose(fp);
		
		int wstatus;
		while (waitpid(pid, &wstatus, 0) != pid);	// skip all EINTR
		
		if (result == NULL) return strdup("??");
		return result;
	};
};
