/*
	Glidix Runtime

	Copyright (c) 2014-2016, Madd Games.
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
#include <signal.h>
#include <stdlib.h>
#include <unistd.h>
#include <sys/glidix.h>
#include <string.h>
#include <fcntl.h>
#include <sys/wait.h>
#include <sys/ioctl.h>
#include <sys/stat.h>
#include <dirent.h>
#include <time.h>

int shouldHalt = 0;
int shouldRunPoweroff = 0;
int ranPoweroff = 0;

char* confRootType = NULL;
char* confRootDevice = NULL;
char** confExec = NULL;

void loadmods()
{
	DIR *dirp = opendir("/initrd/initmod");
	if (dirp == NULL)
	{
		perror("opendir /initrd/initmod");
		exit(1);
	};

	struct dirent *ent;

	char filename[256];
	char modname[256];
	while ((ent = readdir(dirp)) != NULL)
	{
		if (ent->d_name[0] != '.')
		{
			strcpy(filename, "/initrd/initmod/");
			strcat(filename, ent->d_name);

			strcpy(modname, "mod_");
			strcat(modname, ent->d_name);
			modname[strlen(modname)-4] = 0;

			printf("init: insmod %s (from %s)\n", modname, filename);
			_glidix_insmod(modname, filename, NULL, 0);
		};
	};

	closedir(dirp);
	
	_glidix_insmod("mod_sdide", "/initrd/sdide.gkm", NULL, 0);
};

int interpret_config_option(char *line)
{
	char *saveptr;
	const char *cmd = strtok_r(line, " ", &saveptr);
	
	if (strcmp(cmd, "root") == 0)
	{
		confRootType = strtok_r(NULL, " ", &saveptr);
		if (confRootType == NULL)
		{
			fprintf(stderr, "init: syntax error in 'root' command\n");
			return -1;
		};
		
		confRootDevice = strtok_r(NULL, " ", &saveptr);
		if (confRootDevice == NULL)
		{
			fprintf(stderr, "init: syntax error in 'root' command\n");
			return -1;
		};
	}
	else if (strcmp(cmd, "exec") == 0)
	{
		char **args = NULL;
		int numArgs=0;
		const char *par;
		for (par=strtok_r(NULL, " ", &saveptr); par!=NULL; par=strtok_r(NULL, " ", &saveptr))
		{
			args = realloc(args, sizeof(char*)*(numArgs+1));
			args[numArgs++] = strdup(par);
		};
		
		args = realloc(args, sizeof(char*)*(numArgs+1));
		args[numArgs] = NULL;
		
		confExec = args;
	}
	else
	{
		fprintf(stderr, "init: bad command: %s\n", cmd);
		return -1;
	};
};

int load_config(const char *filename)
{
	struct stat st;
	if (stat(filename, &st) == -1)
	{
		perror(filename);
		return -1;
	};
	
	char *config_raw = (char*) malloc(st.st_size);
	int fd = open(filename, O_RDONLY);
	if (fd == -1)
	{
		perror(filename);
		return -1;
	};
	
	read(fd, config_raw, st.st_size);
	close(fd);
	
	config_raw[st.st_size] = 0;
	
	char *saveptr;
	char *token;
	
	for (token=strtok_r(config_raw, "\n", &saveptr); token!=NULL; token=strtok_r(NULL, "\n", &saveptr))
	{
		if (token[0] == 0)
		{
			continue;
		}
		else if (token[0] == '#')
		{
			continue;
		}
		else
		{
			if (interpret_config_option(token) == -1) return -1;
		};
	};
	
	if (confExec == NULL)
	{
		fprintf(stderr, "init: no exec command specified\n");
		return -1;
	};
	
	if (confRootType != NULL)
	{
		if (_glidix_mount(confRootType, confRootDevice, "/", 0) != 0)
		{
			perror("init: mount root");
			printf("init: failed to mount root device %s (%s)\n", confRootDevice, confRootType);
			return -1;
		};
	};
	
	if (stat(confExec[0], &st) != 0)
	{
		_glidix_unmount("/");
		printf("init: cannot stat executable %s\n", confExec[0]);
		return -1;
	};

	printf("init: cleaning up service pid files\n");
	DIR *dp = opendir("/etc/services");
	if (dp != NULL)
	{
		struct dirent *ent;
		while ((ent = readdir(dp)) != NULL)
		{
			const char *name = ent->d_name;
			if (strlen(name) > 4)
			{
				if (memcmp(&name[strlen(name)-4], ".pid", 4) == 0)
				{
					printf("init: removing pid file %s\n", name);
					
					char pidpath[256];
					sprintf(pidpath, "/etc/services/%s", name);
					unlink(pidpath);
				};
			};
		};
		
		closedir(dp);
	};
	
	if (fork() == 0)
	{
		execv(confExec[0], confExec);
		exit(1);
	};
	
	return 0;
};

void on_signal(int sig, siginfo_t *si, void *ignore)
{
	if (sig == SIGINT)
	{
		//kill(2, SIGINT);
	}
	else if (sig == SIGCHLD)
	{
		waitpid(si->si_pid, NULL, 0);
	}
	else if (sig == SIGTERM)
	{
		shouldHalt = 1;
	}
	else if (sig == SIGHUP)
	{
		// the power button was pressed.
		shouldRunPoweroff = 1;
	};
};

void init_parts()
{
	close(open("/dev/sda", O_RDONLY));
	close(open("/dev/sdc", O_RDONLY));
};

int main(int argc, char *argv[])
{
	if (getpid() == 1)
	{
		setenv("PATH", "/bin:/usr/local/bin:/usr/bin:/mnt/bin", 1);
		setenv("HOME", "/root", 1);
		setenv("LD_LIBRARY_PATH", "/usr/lib:/lib", 1);

		struct sigaction sa;
		sa.sa_sigaction = on_signal;
		sa.sa_flags = SA_SIGINFO;
		if (sigaction(SIGINT, &sa, NULL) != 0)
		{
			perror("sigaction SIGINT");
			return 1;
		};
		if (sigaction(SIGCHLD, &sa, NULL) != 0)
		{
			perror("sigaction SIGCHLD");
			return 1;
		};
		if (sigaction(SIGTERM, &sa, NULL) != 0)
		{
			perror("sigaction SIGTERM");
			return 1;
		};
		if (sigaction(SIGHUP, &sa, NULL) != 0)			// SIGHUP is sent when the power button is pressed
		{
			perror("sigaction SIGHUP");
			return 1;
		};

		loadmods();
		
		printf("init: initializing partitions...\n");
		init_parts();

		printf("init: loading configuration file /initrd/startup.conf...\n");
		if (load_config("/initrd/startup.conf") == -1)
		{
			printf("init: attempting startup with fallback configuration file /initrd/startup-fallback.conf...\n");
			if (load_config("/initrd/startup-fallback.conf") == -1)
			{
				printf("init: failed to start up\n");
			};
		};

		while (1)
		{
			pause();
			if (shouldHalt)
			{
				sa.sa_handler = SIG_DFL;
				if (sigaction(SIGCHLD, &sa, NULL) != 0)
				{
					perror("sigaction SIGCHLD");
					return 1;
				};

				int fd = open("/etc/down-action", O_RDONLY);
				char downAction[256];
				memset(downAction, 0, 256);
				downAction[read(fd, downAction, 16)] = 0;
				close(fd);

				int action = _GLIDIX_DOWN_HALT;
				if (strcmp(downAction, "poweroff") == 0)
				{
					action = _GLIDIX_DOWN_POWEROFF;
				}
				else if (strcmp(downAction, "reboot") == 0)
				{
					action = _GLIDIX_DOWN_REBOOT;
				};

				_glidix_down(action);
			}
			else if ((shouldRunPoweroff) && (!ranPoweroff))
			{
				ranPoweroff = 1;
				if (fork() == 0)
				{
					if (execl("/usr/bin/halt", "poweroff", NULL) == -1)
					{
						perror("exec poweroff");
						fprintf(stderr, "forcing shutdown\n");
						kill(1, SIGTERM);
						exit(1);
					};
				};
			};
		};
	}
	else
	{
		fprintf(stderr, "%s: not allowed to execute with pid other than 1!\n", argv[0]);
		return 1;
	};
	return 0;
};
