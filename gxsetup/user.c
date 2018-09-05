/*
	Glidix Installer

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
#include <stdio.h>
#include <termios.h>
#include <string.h>
#include <stdlib.h>
#include <signal.h>
#include <unistd.h>
#include <fcntl.h>

#include "render.h"
#include "user.h"
#include "msgbox.h"
#include "progress.h"

static char username[80];
static char fullname[80];
static char password[80];

static void displayPrompt(const char *prompt, char *buffer, char mask)
{
	*buffer = 0;
	
	int startX, startY;
	renderWindow("<ENTER> Accept entered text",
			"ACCOUNT SETUP",
			60, 5,
			&startX, &startY);
	
	setCursor((uint8_t)(startX), (uint8_t)(startY+1));
	setColor(COLOR_WINDOW);
	
	printf("%s", prompt);
	
	while (1)
	{
		setCursor((uint8_t)startX, (uint8_t)(startY+3));
		setColor(COLOR_TEXTFIELD);
		
		if (mask != 0)
		{
			char temp[80];
			memset(temp, 0, 80);
			memset(temp, ' ', 60);
			memset(temp, mask, strlen(buffer));
			printf("%-60s", temp);
		}
		else
		{
			printf("%-60s", buffer);
		};
		
		setCursor((uint8_t)(startX+strlen(buffer)), (uint8_t)(startY+3));
		
		uint8_t c;
		if (read(0, &c, 1) != 1) continue;
		
		if (c == '\b')
		{
			size_t sz = strlen(buffer);
			if (sz == 0) continue;
			buffer[sz-1] = 0;
		}
		else if ((c >= 32) && (c <= 127))
		{
			size_t sz = strlen(buffer);
			if (sz == 69) continue;
			buffer[sz] = c;
			buffer[sz+1] = 0;
		}
		else if (c == '\n')
		{
			if (buffer[0] != 0) break;
		};
	};
};

void promptUserInfo()
{
	while (1)
	{
		displayPrompt("Enter a username for the admin:", username, 0);
		displayPrompt("Enter full name:", fullname, 0);
		displayPrompt("Enter a password:", password, '*');

		char pass2[80];
		displayPrompt("Re-type password:", pass2, '*');
		
		if (strcmp(password, pass2) == 0)
		{
			break;
		}
		else
		{
			msgbox("ACCOUNT SETUP", "The passwords you entered do not match!");
		};
	};
};

void userSetup()
{
	drawProgress("SETUP", "Creating admin account...", 1, 1);
	
	// create the home directory and set the owner to be the new user
	char homedir[256];
	sprintf(homedir, "/mnt/home/%s", username);
	mkdir(homedir, 0700);
	chown(homedir, 1000, 1000);
	chmod(homedir, 0700);
	
	// create the /etc/passwd file
	FILE *fp = fopen("/mnt/etc/passwd", "w");
	if (fp == NULL)
	{
		msgbox("FATAL ERROR", "Failed to open /mnt/etc/passwd!");
		return;
	};
	
	fprintf(fp, "root:x:0:0:root:/root:/bin/sh\n");
	fprintf(fp, "%s:x:1000:1000:%s:/home/%s:/bin/sh\n", username, fullname, username);
	fclose(fp);
	
	// now the /etc/shadow file
	char salt[2];
	char *hexd = "0123456789abcdef";
	unsigned char c;
	int fd = open("/dev/random", O_RDONLY);
	if (fd == -1)
	{
		c = 0x5D;
	}
	else
	{
		read(fd, &c, 1);
		close(fd);
	};
	
	salt[0] = hexd[c >> 4];
	salt[1] = hexd[c & 0xF];
	
	fp = fopen("/mnt/etc/shadow", "w");
	fprintf(fp, "root:*:0:0:0:0:0:0\n");
	fprintf(fp, "%s:%s:0:0:0:0:0:0\n", username, crypt(password, salt));
	fclose(fp);
	
	chmod("/mnt/etc/shadow", 0600);
	
	// now /etc/group
	fp = fopen("/mnt/etc/group", "w");
	fprintf(fp, "root:x:0:root\n");
	fprintf(fp, "admin:x:1:root,%s\n", username);
	fprintf(fp, "%s:x:1000:%s\n", username, username);
	fclose(fp);
	
	// finally /etc/sudoers
	// give the 'admin' group sole permission to sudo for now
	fp = fopen("/mnt/etc/sudoers", "w");
	fprintf(fp, "# /etc/sudoers\n");
	fprintf(fp, "# This file controls who and how is allowed to use 'sudo'\n");
	fprintf(fp, "# See sudoers(3) for more information.\n");
	fprintf(fp, "group admin allow\n");
	fclose(fp);
	
	chmod("/mnt/etc/sudoers", 0600);
};
