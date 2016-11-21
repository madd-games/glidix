/*
	Glidix Shell Utilities

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

#ifndef __glidix__
#error This program can only run on Glidix!
#endif

#include <sys/stat.h>
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <signal.h>
#include <string.h>
#include <time.h>
#include <fcntl.h>
#include <dirent.h>
#include <errno.h>

char *progName;

typedef struct
{
	int					level;
	int					stopsig;
	int					stoptimeout;
	char					execpath[256];
	char					param[256];
	int					log;
} Service;

void usage()
{
	fprintf(stderr, "USAGE:\t%s start <service-name>\n", progName);
	fprintf(stderr, "\tStarts the specified service.\n\n");
	
	fprintf(stderr, "\t%s stop <service-name>\n", progName);
	fprintf(stderr, "\tStops the specified service.\n\n");
	
	fprintf(stderr, "\t%s restart <service-name>\n", progName);
	fprintf(stderr, "\tRestarts the specified service.\n\n");
	
	fprintf(stderr, "\t%s state <number>\n", progName);
	fprintf(stderr, "\tChanges the system state number, starting or stopping\n");
	fprintf(stderr, "\tthe relevant services.\n");
};

int getsigbyname(const char *signame)
{
	if (strcmp(signame, "SIGHUP") == 0) return SIGHUP;
	if (strcmp(signame, "SIGINT") == 0) return SIGINT;
	if (strcmp(signame, "SIGKILL") == 0) return SIGKILL;
	return SIGTERM;
};

void interpret_config_option(Service *info, char *line)
{
	char *saveptr;
	const char *cmd = strtok_r(line, " ", &saveptr);
	
	if (strcmp(cmd, "level") == 0)
	{
		char *lvlstr = strtok_r(NULL, " ", &saveptr);
		if (lvlstr != NULL)
		{
			sscanf(lvlstr, "%d", &info->level);
		};
	}
	else if (strcmp(cmd, "stopsig") == 0)
	{
		char *signame = strtok_r(NULL, " ", &saveptr);
		if (signame != NULL)
		{
			info->stopsig = getsigbyname(signame);
		};
	}
	else if (strcmp(cmd, "stoptimeout") == 0)
	{
		char *tostr = strtok_r(NULL, " ", &saveptr);
		if (tostr != NULL)
		{
			sscanf(tostr, "%d", &info->stoptimeout);
		};
	}
	else if (strcmp(cmd, "exec") == 0)
	{
		char *execpath = strtok_r(NULL, " ", &saveptr);
		if (execpath != NULL)
		{
			strcpy(info->execpath, execpath);
			char *param = strtok_r(NULL, " ", &saveptr);
			if (param != NULL)
			{
				strcpy(info->param, param);
			}
			else
			{
				info->param[0] = 0;
			};
		};
	}
	else if (strcmp(cmd, "log") == 0)
	{
		char *word = strtok_r(NULL, " ", &saveptr);
		if (word != NULL)
		{
			if (strcmp(word, "on") == 0)
			{
				info->log = 1;
			}
			else if (strcmp(word, "off") == 0)
			{
				info->log = 0;
			};
		};
	};
};

void read_service_conf(Service *info, const char *srvname)
{
	info->level = 2;
	info->stopsig = SIGTERM;
	info->stoptimeout = 10;
	info->execpath[0] = 0;
	info->param[0] = 0;
	info->log = 1;

	char filename[256];
	sprintf(filename, "/etc/services/%s.conf", srvname);
	
	struct stat st;
	if (stat(filename, &st) == -1)
	{
		return;
	};
	
	char *config_raw = (char*) malloc(st.st_size);
	int fd = open(filename, O_RDONLY);
	if (fd == -1)
	{
		return;
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
			interpret_config_option(info, token);
		};
	};
};


int is_process_running(int pid)
{
	char procdir[256];
	sprintf(procdir, "/proc/%d", pid);
	struct stat st;
	return stat(procdir, &st) == 0;
};

int stop_service(const char *name)
{
	Service info;
	read_service_conf(&info, name);
	
	char pidfile[256];
	sprintf(pidfile, "/etc/services/%s.pid", name);
	
	FILE *fp = fopen(pidfile, "rb");
	if (fp == NULL)
	{
		fprintf(stderr, "%s: service '%s' is not running\n", progName, name);
		return 1;
	};
	
	int pid;
	fscanf(fp, "%d", &pid);
	fclose(fp);
	
	if (pid < 2)
	{
		fprintf(stderr, "%s: service '%s' is reporting invalid pid %d\n", progName, name, pid);
		fprintf(stderr, "the service database is severely damaged, you should reboot the system\n");
		return 1;
	};
	
	if (kill(pid, info.stopsig) != 0)
	{
		fprintf(stderr, "%s: could not kill service '%s' (pid %d)\n", progName, name, pid);
		return 1;
	};
	
	time_t beginWait = time(NULL);
	while (is_process_running(pid))
	{
		if ((time(NULL)-beginWait) > info.stoptimeout)
		{
			// we timed out; force the process to die
			kill(pid, SIGKILL);
		};
	};
	
	unlink(pidfile);
	printf("Service '%s' stopped\n", name);
	return 0;
};

int start_service(const char *name)
{
	Service info;
	read_service_conf(&info, name);
	
	if (info.execpath[0] == 0)
	{
		fprintf(stderr, "%s: configuration file /etc/services/%s.conf is lacking\n", progName, name);
		fprintf(stderr, "an 'exec' directive, or does not exist.\n");
		return 1;
	};
	
	char pidfile[256];
	sprintf(pidfile, "/etc/services/%s.pid", name);
	struct stat st;
	
	if (stat(pidfile, &st) == 0)
	{
		fprintf(stderr, "%s: service '%s' is already running\n", progName, name);
		return 1;
	};
	
	pid_t pid = fork();
	if (pid == 0)
	{
		setsid();
		close(0);
		close(1);
		close(2);
		
		int nullfd = open("/dev/null", O_RDWR);
		dup2(0, nullfd);
		
		if (info.log)
		{
			// create the directories if necessary
			mkdir("/var", 0755);
			mkdir("/var/log", 0755);
			mkdir("/var/log/services", 0755);
			
			char logfile[256];
			sprintf(logfile, "/var/log/services/%s.log", name);
			
			int logfd = open(logfile, O_RDWR | O_CREAT, 0600);
			dup2(1, logfd);
			dup2(2, logfd);
			close(logfd);
		}
		else
		{
			dup2(1, nullfd);
			dup2(2, nullfd);
		};
		
		// close the main null descriptor
		close(nullfd);
		
		// run the srv-wrapper to manage the service
		const char *par = info.param;
		if (par[0] == 0) par = NULL;
		execl("/usr/bin/srv-wrapper", "srv-wrapper", info.execpath, par, NULL);
		while (1) pause();
	}
	else
	{
		FILE *fp = fopen(pidfile, "wb");
		fprintf(fp, "%d", (int) pid);
		fclose(fp);
	};
	
	printf("Service '%s' started (pid %d)\n", name, (int) pid);
	return 0;
};

int set_system_state(int state)
{
	printf("Switching to System State %d\n", state);
	
	DIR *dirp = opendir("/etc/services");
	struct dirent *ent;
	Service srv;
	
	if (dirp == NULL)
	{
		fprintf(stderr, "Cannot open /etc/services: %s\n", strerror(errno));
		return 1;
	};
	
	while ((ent = readdir(dirp)) != NULL)
	{
		char name[256];
		strcpy(name, ent->d_name);
		
		if (strcmp(&name[strlen(name)-5], ".conf") == 0)
		{
			name[strlen(name)-5] = 0;
			
			read_service_conf(&srv, name);
			if (srv.level <= state)
			{
				start_service(name);
			}
			else
			{
				stop_service(name);
			};
		};
	};
	
	closedir(dirp);
	return 0;
};

int main(int argc, char *argv[])
{
	progName = argv[0];
	
	// all commands are formed with 3 arguments
	if (argc != 3)
	{
		usage();
		return 1;
	};
	
	if (getuid() != 0)
	{
		fprintf(stderr, "%s: only root can do this\n", argv[0]);
		return 1;
	};
	
	if (getgid() != 0)
	{
		fprintf(stderr, "%s: my group ID is not root!\n", argv[0]);
		return 1;
	};
	
	if (strcmp(argv[1], "start") == 0)
	{
		return start_service(argv[2]);
	}
	else if (strcmp(argv[1], "stop") == 0)
	{
		return stop_service(argv[2]);
	}
	else if (strcmp(argv[1], "restart") == 0)
	{
		if (stop_service(argv[2]) != 0)
		{
			return 1;
		};
		
		return start_service(argv[2]);
	}
	else if (strcmp(argv[1], "state") == 0)
	{
		int state;
		if (sscanf(argv[2], "%d", &state) != 1)
		{
			fprintf(stderr, "%s: failed to parse argument: should be a number\n", argv[0]);
			return 1;
		};
		
		return set_system_state(state);
	}
	else
	{
		usage();
		return 1;
	};
};
