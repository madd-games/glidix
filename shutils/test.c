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

#include <sys/stat.h>
#include <stdlib.h>
#include <stdio.h>
#include <errno.h>
#include <unistd.h>

int fileTypeTest(const char *path, int type)
{
	struct stat st;
	if (stat(path, &st) != 0)
	{
		return 1;
	};
	
	if ((st.st_mode & S_IFMT) == type)
	{
		return 0;
	};
	
	return 1;
};

int testExpr(int argc, char *argv[])
{
	if (argc == 0)
	{
		return 1;
	};
	
	// TODO: if -a or -o are found (and not paranthesized), split into 2 expressions at that point
	// and evaluate separately
	
	if (strcmp(argv[0], "!") == 0)
	{
		int status = testExpr(argc-1, &argv[1]);
		if (status == -1) return -1;
		return !status;
	}
	else if (strcmp(argv[0], "(") == 0)
	{
		if (strcmp(argv[argc-1], ")") != 0)
		{
			return -1;
		};
		
		return testExpr(argc-2, &argv[1]);
	}
	else if (argc == 1)
	{
		// returns 1 (false) if the length is 0
		return strlen(argv[0]) == 0;
	}
	else if (argc == 2)
	{
		if (strcmp(argv[0], "-n") == 0)
		{
			return strlen(argv[1]) == 0;
		}
		else if (strcmp(argv[0], "-z") == 0)
		{
			return strlen(argv[1]) != 0;
		}
		else if (strcmp(argv[0], "-b") == 0)
		{
			return fileTypeTest(argv[1], S_IFBLK);
		}
		else if (strcmp(argv[0], "-c") == 0)
		{
			return fileTypeTest(argv[1], S_IFCHR);
		}
		else if (strcmp(argv[0], "-d") == 0)
		{
			return fileTypeTest(argv[1], S_IFDIR);
		}
		else if (strcmp(argv[0], "-e") == 0)
		{
			return !!access(argv[1], F_OK);
		}
		else if (strcmp(argv[0], "-f") == 0)
		{
			return fileTypeTest(argv[1], S_IFREG);
		}
		else if (strcmp(argv[0], "-g") == 0)
		{
			struct stat st;
			if (stat(argv[1], &st) != 0)
			{
				return 1;
			};
			
			return !(st.st_mode & 02000);
		}
		else if (strcmp(argv[0], "-G") == 0)
		{
			struct stat st;
			if (stat(argv[1], &st) != 0)
			{
				return 1;
			};
			
			return st.st_gid != getegid();
		}
		else if (strcmp(argv[0], "-h") == 0 || strcmp(argv[0], "-L") == 0)
		{
			struct stat st;
			if (lstat(argv[1], &st) != 0)
			{
				return 1;
			};
	
			return !S_ISLNK(st.st_mode);
		}
		else if (strcmp(argv[0], "-k") == 0)
		{
			struct stat st;
			if (stat(argv[1], &st) != 0)
			{
				return 1;
			};
			
			return !(st.st_mode & 01000);
		}
		else if (strcmp(argv[0], "-O") == 0)
		{
			struct stat st;
			if (stat(argv[1], &st) != 0)
			{
				return 1;
			};
			
			return st.st_uid != geteuid();
		}
		else if (strcmp(argv[0], "-p") == 0)
		{
			return fileTypeTest(argv[1], S_IFIFO);
		}
		else if (strcmp(argv[0], "-r") == 0)
		{
			return !!access(argv[1], R_OK);
		}
		else if (strcmp(argv[0], "-s") == 0)
		{
			struct stat st;
			if (stat(argv[1], &st) != 0)
			{
				return 1;
			};
			
			return st.st_size == 0;
		}
		else if (strcmp(argv[0], "-S") == 0)
		{
			return fileTypeTest(argv[1], S_IFSOCK);
		}
		else if (strcmp(argv[0], "-t") == 0)
		{
			int fd;
			if (sscanf(argv[1], "%d", &fd) != 1)
			{
				return 1;
			};
			
			return !isatty(fd);
		}
		else if (strcmp(argv[0], "-u") == 0)
		{
			struct stat st;
			if (stat(argv[1], &st) != 0)
			{
				return 1;
			};
			
			return !(st.st_mode & 04000);
		}
		else if (strcmp(argv[0], "-w") == 0)
		{
			return !!access(argv[1], W_OK);
		}
		else if (strcmp(argv[0], "-x") == 0)
		{
			return !!access(argv[1], X_OK);
		}
		else
		{
			return -1;
		};
	}
	else if (argc >= 3)
	{
		if (argc == 3)
		{
			if (strcmp(argv[1], "=") == 0)
			{
				return !!strcmp(argv[0], argv[2]);
			}
			else if (strcmp(argv[1], "!=") == 0)
			{
				return !strcmp(argv[0], argv[2]);
			}
			else if (strcmp(argv[1], "-ef") == 0)
			{
				struct stat infoA, infoB;
				if (stat(argv[0], &infoA) != 0) return 1;
				if (stat(argv[2], &infoB) != 0) return 1;
				return (infoA.st_dev != infoB.st_dev) || (infoA.st_ino != infoB.st_ino);
			}
			else if (strcmp(argv[1], "-nt") == 0)
			{
				struct stat infoA, infoB;
				if (stat(argv[0], &infoA) != 0) return 1;
				if (stat(argv[2], &infoB) != 0) return 1;
				return infoA.st_mtime < infoB.st_mtime;
			}
			else if (strcmp(argv[1], "-ot") == 0)
			{
				struct stat infoA, infoB;
				if (stat(argv[0], &infoA) != 0) return 1;
				if (stat(argv[2], &infoB) != 0) return 1;
				return infoA.st_mtime > infoB.st_mtime;
			};
		};
		
		int argA, argB;
		char *op;
		
		int i = 0;
		if (strcmp(argv[i], "-l") == 0)
		{
			argA = strlen(argv[i+1]);
			i += 2;
		}
		else
		{
			sscanf(argv[i], "%d", &argA);
			i++;
		};
		
		op = argv[i++];
		
		if (i == argc) return -1;
		
		if (strcmp(argv[i], "-l") == 0)
		{
			i++;
			if (i == argc) return -1;
			
			argB = strlen(argv[i++]);
		}
		else
		{
			sscanf(argv[i++], "%d", &argB);
		};
		
		if (argc != i)
		{
			return -1;
		};
		
		if (strcmp(op, "-eq") == 0)
		{
			return argA != argB;
		}
		else if (strcmp(op, "-ge") == 0)
		{
			return argA < argB;
		}
		else if (strcmp(op, "-gt") == 0)
		{
			return argA <= argB;
		}
		else if (strcmp(op, "-le") == 0)
		{
			return argA > argB;
		}
		else if (strcmp(op, "-lt") == 0)
		{
			return argA >= argB;
		}
		else if (strcmp(op, "-ne") == 0)
		{
			return argA == argB;
		}
		else
		{
			return -1;
		};
	}
	else
	{
		return -1;
	};
};

int main(int argc, char *argv[])
{
	if (strcmp(argv[0], "[") == 0)
	{
		if (strcmp(argv[argc-1], "]") != 0)
		{
			fprintf(stderr, "%s: syntax error\n", argv[0]);
			return 1;
		};
		
		argc--;
	};
	
	int status = testExpr(argc-1, &argv[1]);
	
	if (status == -1)
	{
		fprintf(stderr, "%s: syntax error\n", argv[0]);
		return 1;
	};
	
	return status;
};
