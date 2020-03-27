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
#include <sys/wait.h>
#include <sys/glidix.h>
#include <stdio.h>
#include <pwd.h>
#include <termios.h>
#include <stdlib.h>
#include <unistd.h>
#include <grp.h>
#include <libgwm.h>
#include <string.h>
#include <errno.h>
#include <fcntl.h>

GWMTextField* txtUsername;
GWMTextField* txtPassword;
GWMButton* btnLogin;

const char *username;
const char *password;
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

	struct flock lock;
	memset(&lock, 0, sizeof(struct flock));
	lock.l_type = F_RDLCK;
	lock.l_whence = SEEK_SET;
	lock.l_start = 0;
	lock.l_len = 0;
	
	int status;
	do
	{
		status = fcntl(fileno(fp), F_SETLKW, &lock);
	} while (status != 0 && errno == EINTR);

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

int doLogin()
{
	username = gwmReadTextField(txtUsername);
	password = gwmReadTextField(txtPassword);
	
	struct passwd *pwd = getpwnam(username);
	if (pwd == NULL)
	{
		sleep(3);
		gwmMessageBox(NULL, "Error", "Invalid username or password.", GWM_MBICON_ERROR | GWM_MBUT_OK);
		return GWM_EVSTATUS_CONT;
	};

	if (findPassword(username) != 0)
	{
		gwmMessageBox(NULL, "Error", "This user is not in /etc/shadow!", GWM_MBICON_WARN | GWM_MBUT_OK);
		return GWM_EVSTATUS_CONT;
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
			
			// make sure the basic directories exist
			mkdir("Desktop", 0755);
			mkdir("Documents", 0755);
			mkdir("Pictures", 0755);
			mkdir("Music", 0755);
			mkdir("Videos", 0755);
			mkdir("Downloads", 0755);
			
			if (fork() == 0)
			{
				// the "-filemgr" is there to let the file manager distinguish that it must
				// open a dekstop window by argv[0]
				execl("/usr/bin/filemgr", "-filemgr", NULL);
				perror("exec filemgr");
				_exit(1);
			};
			
			if (fork() == 0)
			{
				struct stat st;
				if (stat(".wallpaper", &st) != 0)
				{
					unlink(".wallpaper");	// in case it's a broken link
					symlink("/usr/share/images/wallpaper.png", ".wallpaper");
				};
				
				execl("/usr/bin/wallpaper", "wallpaper", "--scale", "--image=.wallpaper", NULL);
				_exit(1);
			};
			
			execl("/usr/bin/sysbar", "sysbar", NULL);
			perror("exec");
			_exit(1);
		};
		
		return GWM_EVSTATUS_BREAK;
	}
	else
	{
		sleep(3);
		gwmMessageBox(NULL, "Error", "Invalid username or password.", GWM_MBICON_ERROR | GWM_MBUT_OK);
	};
	
	return GWM_EVSTATUS_CONT;
};

int eventHandler(GWMEvent *ev, GWMWindow *win, void *context)
{
	GWMCommandEvent *cmd = (GWMCommandEvent*) ev;
	
	switch (ev->type)
	{
	case GWM_EVENT_COMMAND:
		if (cmd->symbol == GWM_SYM_OK)
		{
			return doLogin();
		};
		return GWM_EVSTATUS_CONT;
	case GWM_EVENT_CLOSE:
		// prevent closing by Alt-F4
		return GWM_EVSTATUS_OK;
	default:
		return GWM_EVSTATUS_CONT;
	};
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
	
	// set up default background
	pid_t pid = fork();
	if (pid == -1)
	{
		fprintf(stderr, "%s: fork failed: %s\n", argv[0], strerror(errno));
		return 1;
	}
	else if (pid == 0)
	{
		execl("/usr/bin/wallpaper", "wallpaper", "--image=/usr/share/images/splash.png", "--color=#000000", NULL);
		perror("exec wallpaper");
		_exit(1);
	}
	else
	{
		waitpid(pid, NULL, 0);
	};
	
	// now the default theme (GlidixGreen)
	pid = fork();
	if (pid == -1)
	{
		fprintf(stderr, "%s: fork failed: %s\n", argv[0], strerror(errno));
		return 1;
	}
	else if (pid == 0)
	{
		execl("/usr/bin/theme", "theme", "load", "/usr/share/themes/GlidixGreen.thm", NULL);
		perror("exec theme");
		_exit(1);
	}
	else
	{
		waitpid(pid, NULL, 0);
	};
	
	GWMWindow *top = gwmNewPlainWindow();
	gwmPushEventHandler(top, eventHandler, NULL);
	
	GWMLayout *box = gwmCreateBoxLayout(GWM_BOX_VERTICAL);
	gwmSetWindowLayout(top, box);
	
	GWMLayout *flex = gwmCreateFlexLayout(3);
	gwmBoxLayoutAddLayout(box, flex, 0, 2, GWM_BOX_ALL);
	
	GWMLabel *lblUsername = gwmNewLabel(top);
	gwmSetLabelText(lblUsername, "Username:");
	gwmFlexLayoutAddWindow(flex, lblUsername, 0, 0, GWM_FLEX_FILL, GWM_FLEX_CENTER);
	
	gwmFlexLayoutAddLayout(flex, gwmCreateStaticLayout(2, 2, 2, 2), 0, 0, GWM_FLEX_FILL, GWM_FLEX_FILL);
	
	txtUsername = gwmNewTextField(top);
	gwmFlexLayoutAddWindow(flex, txtUsername, 0, 0, GWM_FLEX_FILL, GWM_FLEX_CENTER);
	
	gwmFlexLayoutAddLayout(flex, gwmCreateStaticLayout(2, 2, 2, 2), 0, 0, GWM_FLEX_FILL, GWM_FLEX_FILL);
	gwmFlexLayoutAddLayout(flex, gwmCreateStaticLayout(2, 2, 2, 2), 0, 0, GWM_FLEX_FILL, GWM_FLEX_FILL);
	gwmFlexLayoutAddLayout(flex, gwmCreateStaticLayout(2, 2, 2, 2), 0, 0, GWM_FLEX_FILL, GWM_FLEX_FILL);
	
	GWMLabel *lblPassword = gwmNewLabel(top);
	gwmSetLabelText(lblPassword, "Password:");
	gwmFlexLayoutAddWindow(flex, lblPassword, 0, 0, GWM_FLEX_FILL, GWM_FLEX_CENTER);
	
	gwmFlexLayoutAddLayout(flex, gwmCreateStaticLayout(2, 2, 2, 2), 0, 0, GWM_FLEX_FILL, GWM_FLEX_FILL);
	
	txtPassword = gwmNewTextField(top);
	gwmSetTextFieldFlags(txtPassword, GWM_TXT_MASKED);
	gwmFlexLayoutAddWindow(flex, txtPassword, 0, 0, GWM_FLEX_FILL, GWM_FLEX_CENTER);

	GWMSeparator *sep = gwmNewSeparator(top);
	gwmBoxLayoutAddWindow(box, sep, 0, 2, GWM_BOX_LEFT | GWM_BOX_RIGHT | GWM_BOX_FILL);
	
	GWMLayout *btnbox = gwmCreateBoxLayout(GWM_BOX_HORIZONTAL);
	gwmBoxLayoutAddLayout(box, btnbox, 0, 2, GWM_BOX_FILL | GWM_BOX_ALL);
	
	gwmBoxLayoutAddSpacer(btnbox, 1, 0, 0);
	
	btnLogin = gwmCreateButtonWithLabel(top, GWM_SYM_OK, "Log in");
	gwmBoxLayoutAddWindow(btnbox, btnLogin, 0, 0, 0);
	
	gwmFit(top);
	gwmSetWindowFlags(top, GWM_WINDOW_MKFOCUSED);
	gwmMainLoop();
	gwmQuit();
	return 0;
};