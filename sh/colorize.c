/*
	Glidix Shell

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
#include <unistd.h>
#include <string.h>
#include <ctype.h>

#include "colorize.h"

/* list of shell keywords */
static const char *keywords[] = {
	// shutils
	"acl",
	"adduser",
	"cat",
	"chgrp",
	"chmod",
	"chown",
	"chxperm",
	"clear",
	"clock",
	"color",
	"cp",
	"crypt",
	"daemon",
	"date",
	"dbgexec",
	"dd",
	"dispconf",
	"echo",
	"eject",
	"env",
	"false",
	"fsinfo",
	"gxpad",
	"halt",
	"reboot",
	"poweroff",
	"insmod",
	"kill",
	"ldmods",
	"ln",
	"ls",
	"lspci",
	"mip-install",
	"mkdir",
	"mkgxfs",
	"mkinitrd",
	"mkmip",
	"mount",
	"netconf",
	"passwd",
	"ping",
	"ping6",
	"ps",
	"pwd",
	"realpath",
	"resolve",
	"rm",
	"rmmod",
	"rmuser",
	"route",
	"service",
	"sleep",
	"sniff",
	"stat",
	"sudo",
	"sync",
	"sysman",
	"systat",
	"test",
	"touch",
	"true",
	"umount",
	"userctl",
	"wget",
	"whoami",
	"whois",
	"xperm",
	
	// internal commands
	"exit",
	"export",
	"cd",
	"exec",
	
	// LIST TERMINATOR
	NULL
};

static int printColoredSingleString(const char *linbuff, int numChars)
{
	if (*linbuff != '\'') return 0;
	
	write(1, "\e\"\x02", 3);
	printf("'");
	linbuff++;
	numChars--;
	
	int result = 1;
	while (numChars)
	{
		char c = *linbuff++;
		printf("%c", c);
		result++;
		numChars--;
		
		if (c == '\'') break;
	};
	
	write(1,"\e\"\x07", 3);
	return result;
};

static int printColoredKeyword(const char *linbuff, int numChars)
{
	const char **scan;
	for (scan=keywords; *scan!=NULL; scan++)
	{
		const char *keyword = *scan;
		if (strlen(keyword) > numChars) continue;
		
		if (memcmp(linbuff, keyword, strlen(keyword)) == 0)
		{
			if ((int)strlen(keyword) != numChars)
			{
				if (isalnum(linbuff[strlen(keyword)]) || linbuff[strlen(keyword)] == '_' || linbuff[strlen(keyword)] == '-') continue;
			};
			
			write(1, "\e\"\x09", 3);
			printf("%s", keyword);
			write(1, "\e\"\x07", 3);
			return (int) strlen(keyword);
		};
	};
	
	return 0;
};

static int printColoredVar(const char *linbuff, int numChars)
{
	if (*linbuff != '$') return 0;
	
	linbuff++;
	write(1, "\e\"\x06", 3);
	printf("$");
	numChars--;
	
	int result = 1;
	while (numChars)
	{
		if (!isalnum(*linbuff) && *linbuff != '_')
		{
			break;
		};
		
		printf("%c", *linbuff++);
		numChars--;
		result++;
	};
	
	write(1, "\e\"\x07", 3);
	return result;
};

static int printColoredComment(const char *linbuff, int numChars)
{
	if (*linbuff == '#')
	{
		write(1, "\e\"\x08", 3);
		int count = numChars;
		while (count--) printf("%c", *linbuff++);
		write(1, "\e\"\x07", 3);
		return numChars;
	}
	else
	{
		return 0;
	};
};

static int printColoredSpecial(const char *linbuff, int numChars)
{
	const char *specials = "`&()[]{}*<>|!=.";
	if (strchr(specials, *linbuff) != NULL)
	{
		write(1, "\e\"\x04", 3);
		printf("%c", *linbuff);
		write(1, "\e\"\x07", 3);
		return 1;
	}
	else
	{
		return 0;
	};
};

int printColoredDoubleString(const char *linbuff, int numChars)
{
	if (*linbuff != '"') return 0;
	
	write(1, "\e\"\x02", 3);
	printf("\"");
	linbuff++;
	numChars--;
	
	int result = 1;
	while (numChars)
	{
		int count;
		if ((count = printColoredVar(linbuff, numChars)) != 0)
		{
			numChars -= count;
			linbuff += count;
			result += count;
		}
		else
		{
			char c = *linbuff;
			write(1, "\e\"\x02", 3);
			printf("%c", c);
			numChars--;
			result++;
			linbuff++;

			if (c == '"') break;
		};
	};
	
	write(1,"\e\"\x07", 3);
	return result;
};

void printColored(const char *linbuff, int numChars)
{
	while (numChars)
	{
		int count;
		if ((count = printColoredSingleString(linbuff, numChars)) != 0)
		{
			numChars -= count;
			linbuff += count;
		}
		else if ((count = printColoredKeyword(linbuff, numChars)) != 0)
		{
			numChars -= count;
			linbuff += count;
		}
		else if ((count = printColoredComment(linbuff, numChars)) != 0)
		{
			numChars -= count;
			linbuff += count;
		}
		else if ((count = printColoredVar(linbuff, numChars)) != 0)
		{
			numChars -= count;
			linbuff += count;
		}
		else if ((count = printColoredSpecial(linbuff, numChars)) != 0)
		{
			numChars -= count;
			linbuff += count;
		}
		else if ((count = printColoredDoubleString(linbuff, numChars)) != 0)
		{
			numChars -= count;
			linbuff += count;
		}
		else
		{
			printf("%c", *linbuff++);
			numChars--;
		};
	};
};
