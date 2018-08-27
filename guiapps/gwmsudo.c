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

#define	_GLIDIX_SOURCE
#include <sys/wait.h>
#include <sys/stat.h>
#include <stdio.h>
#include <libgwm.h>
#include <time.h>
#include <sys/time.h>
#include <stdlib.h>
#include <string.h>
#include <signal.h>
#include <unistd.h>
#include <dirent.h>
#include <pthread.h>
#include <poll.h>
#include <errno.h>
#include <fcntl.h>
#include <pwd.h>
#include <grp.h>

char passcrypt[128];
char shname[128];

GWMTextField *txtPassword;
int status = 1;

int canSudo()
{
	if (getuid() == 0) return 1;

	FILE *fp = fopen("/etc/sudoers", "r");
	if (fp == NULL)
	{
		fprintf(stderr, "CRITICAL: cannot open /etc/sudoers for reading: %s\n", strerror(errno));
		abort();
	};
	
	char linebuf[1024];
	int lineno = 0;
	while (fgets(linebuf, 1024, fp) != NULL)
	{
		// remove comments
		lineno++;
		char *comment = strchr(linebuf, '#');
		if (comment != NULL) *comment = 0;
		char *endline = strchr(linebuf, '\n');
		if (endline != NULL) *endline = 0;
		
		char *dir = strtok(linebuf, " \t");
		if (dir == NULL) continue;
		
		char *name = strtok(NULL, " \t");
		if (name == NULL)
		{
			fprintf(stderr, "CRITICAL: syntax error in /etc/sudoers on line %d\n", lineno);
			abort();
		};
		
		if (strcmp(dir, "user") == 0)
		{
			struct passwd *info = getpwnam(name);
			if (info == NULL) continue;
			if (info->pw_uid != getuid()) continue;
		}
		else if (strcmp(dir, "group") == 0)
		{
			struct group *info = getgrnam(name);
			if (info == NULL) continue;

			int numGroups = getgroups(0, NULL);
			gid_t *groups = (gid_t*) malloc(sizeof(gid_t)*numGroups);
			getgroups(numGroups, groups);
	
			int i;
			int found = 0;
			for (i=0; i<numGroups; i++)
			{
				if (groups[i] == info->gr_gid)
				{
					found = 1;
					break;
				};
			};
			
			free(groups);
			if (!found)
			{
				if (info->gr_gid != getgid())
				{
					continue;
				};
			};
		}
		else
		{
			fprintf(stderr, "CRITICAL: invalid directive in /etc/sudoers on line %d\n", lineno);
			abort();
		};
		
		int def;
		char *deftok = strtok(NULL, " \t");
		if (deftok == NULL)
		{
			fprintf(stderr, "CRITICAL: syntax error in /etc/sudoers on line %d\n", lineno);
			abort();
		};
		
		if (strcmp(deftok, "allow") == 0)
		{
			def = 1;
		}
		else if (strcmp(deftok, "deny") == 0)
		{
			def = 0;
		}
		else
		{
			fprintf(stderr, "CRITICAL: syntax error in /etc/sudoers on line %d\n", lineno);
			abort();
		};
		
		// TODO: exceptions
		fclose(fp);
		return def;
	};
	
	fclose(fp);
	return 0;	// do not allow by default
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

int hasAuthed()
{
	// check if the current user has recently (in the last 10 minutes) authenticated, and return
	// nonzero if so. this is to avoid constantly asking for the password
	
	// first make sure the auth directories exist
	mkdir("/run/sudo", 0755);
	mkdir("/run/sudo/auth", 0700);
	
	char path[256];
	sprintf(path, "/run/sudo/auth/%lu", getuid());
	
	struct stat st;
	if (stat(path, &st) != 0)
	{
		unlink(path);	// in case it exists but something's wrong with it
		return 0;
	};
	
	// check if it was created less than 10 minutes ago
	time_t earliestAllowed = time(NULL) - 10 * 60;
	if (st.st_btime < earliestAllowed)
	{
		unlink(path);	// too old
		return 0;
	};
	
	return 1;
};

void authOk()
{
	// report successful authentication, so that the user doesn't have to re-authenticate until
	// a timeout.
	
	// first make sure the auth directories exist
	mkdir("/run/sudo", 0755);
	mkdir("/run/sudo/auth", 0700);
	
	char path[256];
	sprintf(path, "/run/sudo/auth/%lu", getuid());
	close(open(path, O_CREAT | O_WRONLY, 0600));
};

int sudoCommand(GWMCommandEvent *ev)
{
	switch (ev->symbol)
	{
	case GWM_SYM_OK:
		{
			const char *password = gwmReadTextField(txtPassword);
			if (strcmp(crypt(password, passcrypt), passcrypt) != 0)
			{
				sleep(2);
				gwmMessageBox(NULL, "gwmsudo", "Sorry, please try again", GWM_MBICON_ERROR | GWM_MBUT_OK);
			}
			else
			{
				// correct password; set successful status and break
				status = 0;
				return GWM_EVSTATUS_BREAK;
			};
		};
		return GWM_EVSTATUS_CONT;
	case GWM_SYM_CANCEL:
		// break without setting the status to 0
		return GWM_EVSTATUS_BREAK;
	default:
		return GWM_EVSTATUS_CONT;
	};
};

int sudoHandler(GWMEvent *ev, GWMWindow *win, void *context)
{
	switch (ev->type)
	{
	case GWM_EVENT_COMMAND:
		return sudoCommand((GWMCommandEvent*) ev);
	default:
		return GWM_EVSTATUS_CONT;
	};
};

int main(int argc, char *argv[])
{
	if (gwmInit() != 0)
	{
		fprintf(stderr, "Failed to initialize GWM\n");
		return 1;
	};
	
	if (geteuid() != 0)
	{
		gwmMessageBox(NULL, argv[0], "Not running as effective root! probably the setuid flag is clear!",
				GWM_MBICON_ERROR | GWM_MBUT_OK);
		return 1;
	};

	if (!canSudo())
	{
		gwmMessageBox(NULL, argv[0], "You do not have administrator permissions", GWM_MBICON_ERROR | GWM_MBUT_OK);
		return 1;
	};

	// root running sudo should just be like running env.
	if (getuid() != 0 && !hasAuthed())
	{
		struct passwd *pwd = getpwuid(getuid());
		if (pwd == NULL)
		{
			char *errmsg;
			asprintf(&errmsg, "Cannot resolve current UID (%lu)", getuid());
			gwmMessageBox(NULL, argv[0], errmsg, GWM_MBICON_ERROR | GWM_MBUT_OK);
			return 1;
		};

		if (findPassword(pwd->pw_name) != 0)
		{
			char *errmsg;
			asprintf(&errmsg, "User %s is not in the shadow file!", pwd->pw_name);
			gwmMessageBox(NULL, argv[0], errmsg, GWM_MBICON_ERROR | GWM_MBUT_OK);
			return 1;
		};

		GWMWindow *topWindow = gwmCreateWindow(NULL, argv[0], GWM_POS_UNSPEC, GWM_POS_UNSPEC, 0, 0,
							GWM_WINDOW_HIDDEN | GWM_WINDOW_NOTASKBAR);
		GWMLayout *mainBox = gwmCreateBoxLayout(GWM_BOX_VERTICAL);
		gwmSetWindowLayout(topWindow, mainBox);
		
		GWMLabel *lblCaption = gwmNewLabel(topWindow);
		gwmBoxLayoutAddWindow(mainBox, lblCaption, 0, 5, GWM_BOX_ALL | GWM_BOX_FILL);
		gwmSetLabelFont(lblCaption, GWM_FONT_CAPTION);
		gwmSetLabelText(lblCaption, "Authentication required");
		
		GWMLabel *lblInfo = gwmNewLabel(topWindow);
		gwmBoxLayoutAddWindow(mainBox, lblInfo, 0, 5, GWM_BOX_ALL | GWM_BOX_FILL);
		gwmSetLabelText(lblInfo, "The operation you are trying to perform requires administrator privileges."
					" Please type your password below to grant this application administrator access."
					" If you do not trust this applicatin, please click 'Cancel'.");
		gwmSetLabelWidth(lblInfo, 500);
		
		txtPassword = gwmNewTextField(topWindow);
		gwmBoxLayoutAddWindow(mainBox, txtPassword, 0, 5, GWM_BOX_ALL | GWM_BOX_FILL);
		gwmSetTextFieldFlags(txtPassword, GWM_TXT_MASKED);
		
		GWMLayout *btnBox = gwmCreateBoxLayout(GWM_BOX_HORIZONTAL);
		gwmBoxLayoutAddLayout(mainBox, btnBox, 0, 5, GWM_BOX_ALL | GWM_BOX_FILL);
		
		gwmBoxLayoutAddSpacer(btnBox, 0, 0, 0);
		
		gwmBoxLayoutAddWindow(btnBox, gwmCreateStockButton(topWindow, GWM_SYM_OK), 0, 5, GWM_BOX_RIGHT);
		gwmBoxLayoutAddWindow(btnBox, gwmCreateStockButton(topWindow, GWM_SYM_CANCEL), 0, 0, 0);
		
		gwmFit(topWindow);
		gwmPushEventHandler(topWindow, sudoHandler, NULL);
		gwmSetWindowFlags(topWindow, GWM_WINDOW_MKFOCUSED | GWM_WINDOW_NOSYSMENU);
		gwmMainLoop();
		gwmQuit();
		
		if (status != 0) return 1;
		authOk();
	};

	// ok, now try switching to being root.
	if (setgid(0) != 0)
	{
		perror("setgid");
		return 1;
	};

	if (setuid(0) != 0)
	{
		perror("setuid");
		return 1;
	};

	// now run
	argv[0] = "env";
	execv("/usr/bin/env", argv);
	perror("execv");
	return 1;
};
