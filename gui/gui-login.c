/*
	Glidix GUI

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
#include <sys/glidix.h>
#include <stdio.h>
#include <pwd.h>
#include <termios.h>
#include <stdlib.h>
#include <unistd.h>
#include <grp.h>
#include <libgwm.h>
#include <string.h>

#define	WIN_WIDTH	232
#define	WIN_HEIGHT	78

GWMWindow *txtUsername;
GWMWindow *txtPassword;

char username[128];
char password[128];
char passcrypt[128];
char shname[128];

void getline(char *buffer)
{
	ssize_t count = read(0, buffer, 128);
	if (count == -1)
	{
		perror("read");
		exit(1);
	};

	if (count > 127)
	{
		fprintf(stderr, "input too long\n");
		exit(1);
	};

	buffer[count-1] = 0;
};

int nextShadowEntry(FILE *fp)
{
	char *put = shname;
	while (1)
	{
		int c = fgetc(fp);
		if (c == EOF)
		{
			return -1;
		};

		if (c == ':')
		{
			*put = 0;
			break;
		};

		*put++ = (char) c;
	};
	*put = 0;

	put = passcrypt;
	while (1)
	{
		int c = fgetc(fp);
		if (c == EOF)
		{
			return -1;
		};

		if (c == ':')
		{
			*put = 0;
			break;
		};

		*put++ = (char) c;
	};
	*put = 0;

	while (1)
	{
		int c = fgetc(fp);
		if (c == '\n')
		{
			break;
		};

		if (c == EOF)
		{
			return -1;
		};
	};

	return 0;
};

int findPassword(const char *username)
{
	FILE *fp = fopen("/etc/shadow", "r");
	if (fp == NULL)
	{
		perror("open /etc/shadow");
		exit(1);
	};

	while (nextShadowEntry(fp) != -1)
	{
		if (strcmp(shname, username) == 0)
		{
			fclose(fp);
			return 0;
		};
	};

	fclose(fp);
	return -1;
};

int logInCallback(void *ignore)
{
	gwmReadTextField(txtUsername, username, 0, 127);
	gwmReadTextField(txtPassword, password, 0, 127);
	
	struct passwd *pwd = getpwnam(username);
	if (pwd == NULL)
	{
		//sleep(5);
		gwmMessageBox(NULL, "Error", "Invalid username or password.", GWM_MBICON_ERROR | GWM_MBUT_OK);
		return 0;
	};

	if (findPassword(username) != 0)
	{
		gwmMessageBox(NULL, "Error", "This user is not in /etc/shadow!", GWM_MBICON_WARN | GWM_MBUT_OK);
		return 0;
	};

	if (strcmp(crypt(password, passcrypt), passcrypt) == 0)
	{
		// search all groups to find which ones we are in
		gid_t *groups = NULL;
		size_t numGroups = 0;
		
		struct group *grp;
		while ((grp = getgrent()) != NULL)
		{
			int i;
			for (i=0; grp->gr_mem[i]!=NULL; i++)
			{
				if (strcmp(grp->gr_mem[i], username) == 0)
				{
					groups = realloc(groups, sizeof(gid_t)*(numGroups+1));
					groups[numGroups++] = grp->gr_gid;
					break;
				};
			};
		};

		if (fork() == 0)
		{
			// set up the environment
			setenv("HOME", pwd->pw_dir, 1);
			setenv("SHELL", pwd->pw_shell, 1);
			setenv("USERNAME", pwd->pw_name, 1);
			chdir(pwd->pw_dir);
			
			if (_glidix_setgroups(numGroups, groups) != 0)
			{
				perror("_glidix_setgroups");
				exit(1);
			};
		
			if (setgid(pwd->pw_gid) != 0)
			{
				perror("setgid");
				exit(1);
			};

			if (setuid(pwd->pw_uid) != 0)
			{
				perror("setuid");
				exit(1);
			};

			execl("/usr/bin/sysbar", "sysbar", NULL);
			exit(1);
		};
		
		return -1;
	}
	else
	{
		//sleep(5);
		gwmMessageBox(NULL, "Error", "Invalid username or password.", GWM_MBICON_ERROR | GWM_MBUT_OK);
	};
	
	return 0;
};

int main(int argc, char *argv[])
{
	if (gwmInit() != 0)
	{
		fprintf(stderr, "%s: could not initialize GWM!\n", argv[0]);
		return 1;
	};
	
	if ((geteuid() != 0) || (getuid() != 0))
	{
		gwmMessageBox(NULL, "Error", "Login program requires root privileges.", GWM_MBICON_ERROR | GWM_MBUT_OK);
		gwmQuit();
		return 1;
	};
	
	int width, height;
	gwmScreenSize(&width, &height);
	
	GWMWindow *win = gwmCreateWindow(NULL, "Login", width/2-WIN_WIDTH/2, height/2-WIN_HEIGHT/2,
						WIN_WIDTH, WIN_HEIGHT, GWM_WINDOW_MKFOCUSED | GWM_WINDOW_NODECORATE);
	if (win == NULL)
	{
		fprintf(stderr, "Failed to create window!\n");
		gwmQuit();
		return 1;
	};
	
	DDISurface *canvas = gwmGetWindowCanvas(win);
	DDIColor background = {0x00, 0x00, 0xAA, 0x77};
	ddiFillRect(canvas, 0, 0, WIN_WIDTH, WIN_HEIGHT, &background);
	
	DDIColor white = {0xFF, 0xFF, 0xFF, 0xFF};
	
	DDIPen *pen = ddiCreatePen(&canvas->format, gwmGetDefaultFont(), 2, 4, 100, 20, 0, 0, NULL);
	ddiSetPenColor(pen, &white);
	ddiWritePen(pen, "Username:");
	ddiExecutePen(pen, canvas);
	ddiDeletePen(pen);

	pen = ddiCreatePen(&canvas->format, gwmGetDefaultFont(), 2, 26, 100, 20, 0, 0, NULL);
	ddiSetPenColor(pen, &white);
	ddiWritePen(pen, "Password:");
	ddiExecutePen(pen, canvas);
	ddiDeletePen(pen);
	
	txtUsername = gwmCreateTextField(win, "", 80, 2, 150, 0);
	txtPassword = gwmCreateTextField(win, "", 80, 24, 150, GWM_TXT_MASKED);
	
	GWMWindow *btnLogIn = gwmCreateButton(win, "Log in", 2, 46, 50, 0);
	gwmSetButtonCallback(btnLogIn, logInCallback, NULL);
	
	gwmPostDirty(win);
	gwmMainLoop();
	gwmQuit();
	return 0;
};
