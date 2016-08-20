/*
	Glidix Network Manager

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

#include <sys/stat.h>
#include <stdio.h>
#include <stdlib.h>
#include <fcntl.h>
#include <unistd.h>
#include <string.h>
#include <errno.h>
#include <dlfcn.h>
#include <gxnetman.h>

#define	SPACES				" \t"

static const char *progName;
static NetmanIfConfig *ifList = NULL;
static size_t numIfs = 0;

static void addInterface(NetmanIfConfig *config)
{
	ifList = realloc(ifList, sizeof(NetmanIfConfig)*(numIfs+1));
	memcpy(&ifList[numIfs], config, sizeof(NetmanIfConfig));
	numIfs++;
};

volatile sig_atomic_t running = 1;

int netman_running()
{
	return running;
};

static void netmanSignal(int signal, siginfo_t *si, void *context)
{
	if (signal == SIGINT)
	{
		running = 0;
	};
};

static void gotoDaemon(int family, const char *lockfile, const char *logfile, int pipefd, const char *ifname)
{
	int lockfd = open(lockfile, O_RDWR | O_CREAT, 0644);
	if (lockfd == -1)
	{
		char c = NETMAN_DAEMON_NOLOCK;
		write(pipefd, &c, 1);
		close(pipefd);
		exit(1);
	};
	
	if (lockf(lockfd, F_TLOCK, 0) != 0)
	{
		char c = NETMAN_DAEMON_NOLOCK;
		write(pipefd, &c, 1);
		close(pipefd);
		exit(1);
	};
	
	close(0);
	close(1);
	close(2);
	
	if (open("/dev/null", O_RDONLY) != 0)
	{
		char c = NETMAN_DAEMON_NONULL;
		write(pipefd, &c, 1);
		close(pipefd);
		exit(1);
	};
	
	if (open(logfile, O_WRONLY | O_CREAT | O_TRUNC, 0644) != 1)
	{
		char c = NETMAN_DAEMON_NOLOG;
		write(pipefd, &c, 1);
		close(pipefd);
		exit(1);
	};
	
	if (dup(1) != 2)
	{
		char c = NETMAN_DAEMON_NOLOG;
		write(pipefd, &c, 1);
		close(pipefd);
		exit(1);
	};
	
	struct sigaction sa;
	memset(&sa, 0, sizeof(struct sigaction));
	sa.sa_sigaction = netmanSignal;
	sa.sa_flags = SA_SIGINFO;
	if (sigaction(SIGINT, &sa, NULL) != 0)
	{
		char c = NETMAN_DAEMON_NOSIG;
		write(pipefd, &c, 1);
		close(pipefd);
		exit(1);
	};
	
	char buffer[256];
	sprintf(buffer, "%d\n", getpid());
	write(lockfd, buffer, strlen(buffer));
	
	// now close all files except the lock file and standard streams
	// this includes the pipe
	int fd;
	for (fd=3; fd<sysconf(_SC_OPEN_MAX); fd++)
	{
		if (fd != lockfd)
		{
			close(fd);
		};
	};
	
	size_t i;
	for (i=0; i<numIfs; i++)
	{
		if (strcmp(ifList[i].ifname, ifname) == 0)
		{
			if (ifList[i].family == family)
			{
				printf("[netman] attempting to configure using '%s'\n", ifList[i].method);
				ifList[i].ifupfunc(&ifList[i]);
				if (!running) break;
			};
		};
	};
	
	printf("[netman] ending configuration attempts\n");
	exit(1);
};

int main(int argc, char *argv[])
{
	progName = argv[0];
	
	if (argc != 2)
	{
		fprintf(stderr, "USAGE:\t%s <interface-name>\n", argv[0]);
		fprintf(stderr, "\tBrings up the specified network interface.\n\n");
		return 1;
	};
	
	if (geteuid() != 0)
	{
		fprintf(stderr, "%s: you must be root", argv[0]);
		return 1;
	};
	
	if (strlen(argv[1]) > 15)
	{
		fprintf(stderr, "%s: invalid interface name: %s\n", argv[0], argv[1]);
		return 1;
	};
	
	// ensure that any necessary directories exist (ignore errors if they already exist)
	mkdir("/etc", 0755);
	mkdir("/etc/dns", 0755);
	mkdir("/etc/dns/ipv6", 0755);
	mkdir("/etc/dns/ipv4", 0755);
	mkdir("/var", 0755);
	mkdir("/var/log", 0755);
	mkdir("/var/log/netman", 0755);
	mkdir("/run", 0755);
	mkdir("/run/netman", 0755);
	
	FILE *fp = fopen("/etc/if.conf", "r+");
	if (fp == NULL)
	{
		fprintf(stderr, "%s: failed to open /etc/if.conf: %s\n", argv[0], strerror(errno));
		return 1;
	};
	
	if (lockf(fileno(fp), F_TLOCK, 0) != 0)
	{
		fprintf(stderr, "%s: failed to acquire if.conf lock: %s\n", argv[0], strerror(errno));
		fclose(fp);
		return 1;
	};
	
	NetmanIfConfig config;
	config.family = AF_UNSPEC;
	
	char linebuf[4096];
	int lineno = 0;
	while (fgets(linebuf, 4093, fp) != NULL)
	{
		char *line = linebuf;
		
		lineno++;
		if (line[strlen(line)-1] != '\n')
		{
			fprintf(stderr, "%s: if.conf line %d error: line too long (longer than 4093 characters)\n",
				argv[0], lineno);
			fclose(fp);
			return 1;
		};
		
		while ((line[0] == ' ') || (line[0] == '\t')) line++;
		
		if (line[0] == '\n')
		{
			continue;
		};
		
		if (line[0] == '#')
		{
			continue;
		};
		
		line[strlen(line)-1] = 0;
		
		if (line[0] == 0)
		{
			continue;
		};
		
		if (line[0] == '}')
		{
			if (config.family == AF_UNSPEC)
			{
				// we're not within an interface definition block
				fprintf(stderr, "%s: if.conf line %d error: unexpected '}'", argv[0], lineno);
				fclose(fp);
				return 1;
			}
			else
			{
				addInterface(&config);
				config.family = AF_UNSPEC;
			};
		}
		else if (config.family != AF_UNSPEC)
		{
			config.linefunc(&config, lineno, line);
		}
		else
		{
			char *interfaceKeyword = strtok(line, SPACES);
			if (interfaceKeyword == NULL)
			{
				// empty line??????? idk
				continue;
			};
			
			if (strcmp(interfaceKeyword, "interface") != 0)
			{
				fprintf(stderr, "%s: if.conf line %d error: expected 'interface', not '%s'\n",
					argv[0], lineno, interfaceKeyword);
				fclose(fp);
				return 1;
			};
			
			char *ifname = strtok(NULL, SPACES);
			if (ifname == NULL)
			{
				fprintf(stderr, "%s: if.conf line %d error: expecting interface name, not EOL\n",
					argv[0], lineno);
				fclose(fp);
				return 1;
			};
			
			if (strlen(ifname) > 15)
			{
				fprintf(stderr, "%s: if.conf line %d error: interface name '%s' is too long (max 15 chars)\n",
					argv[0], lineno, ifname);
				fclose(fp);
				return 1;
			};
			
			strcpy(config.ifname, ifname);
			
			char *family = strtok(NULL, SPACES);
			if (family == NULL)
			{
				fprintf(stderr, "%s: if.conf line %d error: expected address family, not EOL\n",
					argv[0], lineno);
				fclose(fp);
				return 1;
			};
			
			if (strcmp(family, "ipv4") == 0)
			{
				config.family = AF_INET;
			}
			else if (strcmp(family, "ipv6") == 0)
			{
				config.family = AF_INET6;
			}
			else
			{
				fprintf(stderr, "%s: if.conf line %d error: invalid address family '%s'\n",
					argv[0], lineno, family);
				fclose(fp);
				return 1;
			};
			
			char *method = strtok(NULL, SPACES);
			if (method == NULL)
			{
				fprintf(stderr, "%s: if.conf line %d error: expected configuration method, not EOL\n",
					argv[0], lineno);
				fclose(fp);
				return 1;
			};
			
			if (strlen(method) > 31)
			{
				fprintf(stderr, "%s: if.conf line %d error: invalid configuration method '%s'\n",
					argv[0], lineno, method);
				fclose(fp);
				return 1;
			};
			
			strcpy(config.method, method);
			config.status = NETMAN_STATUS_OK;
			
			char *op = strtok(NULL, SPACES);
			
			char libname[256];
			char initname[256];
			char linename[256];
			char ifupname[256];
			sprintf(libname, "/usr/libexec/netman/%s/%s.so", family, method);
			sprintf(initname, "netman_%s_init", method);
			sprintf(linename, "netman_%s_line", method);
			sprintf(ifupname, "netman_%s_ifup", method);
			
			void *lib = dlopen(libname, RTLD_LAZY);
			if (lib == NULL)
			{
				fprintf(stderr, "%s: failed to load %s: %s\n", argv[0], libname, dlerror());
				fclose(fp);
				return 1;
			};
			
			netman_conf_init_t initfunc = (netman_conf_init_t) dlsym(lib, initname);
			if (initfunc == NULL)
			{
				fprintf(stderr, "%s: the library %s does not export the needed symbol '%s'\n",
					argv[0], libname, initname);
				dlclose(lib);
				fclose(fp);
				return 1;
			};
			
			config.linefunc = (netman_conf_line_t) dlsym(lib, linename);
			if (config.linefunc == NULL)
			{
				fprintf(stderr, "%s: the library %s does not export the needed symbol '%s'\n",
					argv[0], libname, linename);
				dlclose(lib);
				fclose(fp);
				return 1;
			};
			
			config.ifupfunc = (netman_conf_ifup_t) dlsym(lib, ifupname);
			if (config.ifupfunc == NULL)
			{
				fprintf(stderr, "%s: the library %s does not export the needed smbol '%s'\n",
					argv[0], libname, ifupname);
				dlclose(lib);
				fclose(fp);
				return 1;
			};
			
			if (initfunc(&config) != 0)
			{
				fprintf(stderr, "%s: the '%s' module failed to initialize\n", argv[0], method);
				dlclose(lib);
				fclose(fp);
				return 1;
			};
			
			if (op != NULL)
			{
				if (strcmp(op, "{") != 0)
				{
					fprintf(stderr, "%s: if.conf line %d error: expected '{' or EOL, not '%s'\n",
						argv[0], lineno, op);
					fclose(fp);
					return 1;
				};
			}
			else
			{
				addInterface(&config);
				config.family = AF_UNSPEC;
			};
		};
	};
	
	if (config.family != AF_UNSPEC)
	{
		fprintf(stderr, "%s: if.conf error: no terminating '}' for '%s'\n", argv[0], config.ifname);
		return 1;
	};
	
	// make sure the 'run' directory for this interface exists
	char ifrundir[256];
	char lockfile4[256];
	char lockfile6[256];
	sprintf(ifrundir, "/run/netman/%s", argv[1]);
	mkdir(ifrundir, 0755);
	sprintf(lockfile4, "/run/netman/%s/ipv4", argv[1]);
	sprintf(lockfile6, "/run/netman/%s/ipv6", argv[1]);
	
	int found = 0;
	size_t i;
	for (i=0; i<numIfs; i++)
	{
		if (strcmp(ifList[i].ifname, argv[1]) == 0)
		{
			found = 1;
			if (ifList[i].status != NETMAN_STATUS_OK)
			{
				fprintf(stderr, "%s: interface configuration includes errors\n", argv[0]);
				fclose(fp);
				return 1;
			};
		};
	};
	
	if (!found)
	{
		fprintf(stderr, "%s: interface configuration not found: %s\n", argv[0], argv[1]);
		fclose(fp);
		return 2;	/* we return 2 to indicate this specific error for "ifinit" */
	};
	
	// OK, start both daemons
	char logdir[256];
	char logfile4[256];
	char logfile6[256];
	sprintf(logdir, "/var/log/netman/%s", argv[1]);
	mkdir(logdir, 0755);
	
	sprintf(logfile4, "/var/log/netman/%s/ipv4.log", argv[1]);
	sprintf(logfile6, "/var/log/netman/%s/ipv6.log", argv[1]);
	
	// the IPv4 daemon
	int pipefd[2];
	if (pipe(pipefd) != 0)
	{
		fprintf(stderr, "%s: pipe creation failed for IPv4: %s\n", argv[0], strerror(errno));
		fclose(fp);
		return 1;
	};
	
	pid_t pid = fork();
	if (pid == -1)
	{
		fprintf(stderr, "%s: fork failed: %s\n", argv[0], strerror(errno));
		fclose(fp);
		return 1;
	}
	else if (pid == 0)
	{
		// close the read end of the pipe
		close(pipefd[0]);
		setsid();
		pid = fork();
		
		if (pid == -1)
		{
			char c = NETMAN_DAEMON_NOFORK;
			write(pipefd[1], &c, 1);
			close(pipefd[1]);
			exit(1);
		}
		else if (pid == 0)
		{
			gotoDaemon(AF_INET, lockfile4, logfile4, pipefd[1], argv[1]);
		}
		else
		{
			// i am the parent; exit.
			exit(1);
		};
	}
	else
	{
		// close the write end of the pipe
		close(pipefd[1]);
		
		// wait for the daemon to write the status to the pipe
		char c = NETMAN_DAEMON_OK;
		read(pipefd[0], &c, 1);
		close(pipefd[0]);
		
		if (c != NETMAN_DAEMON_OK)
		{
			if (c == NETMAN_DAEMON_NOLOCK)
			{
				fprintf(stderr, "%s: interface %s already running\n", argv[0], argv[1]);
				fclose(fp);
				return 1;
			}
			else if (c == NETMAN_DAEMON_NOFORK)
			{
				fprintf(stderr, "%s: cannot start IPv4 daemon: second fork failed\n", argv[0]);
				fclose(fp);
				return 1;
			}
			else if (c == NETMAN_DAEMON_NONULL)
			{
				fprintf(stderr, "%s: cannot start IPv4 daemon: failed to open /dev/null", argv[0]);
				fclose(fp);
				return 1;
			}
			else if (c == NETMAN_DAEMON_NOLOG)
			{
				fprintf(stderr, "%s: cannot start IPv4 daemon: failed to open log file %s\n", argv[0], logfile4);
				fclose(fp);
				return 1;
			}
			else if (c == NETMAN_DAEMON_NOSIG)
			{
				fprintf(stderr, "%s: cannot start IPv4 daemon: sigaction failed\n", argv[0]);
				fclose(fp);
				return 1;
			}
			else
			{
				fprintf(stderr, "%s: unknown error occured\n", argv[0]);
				fclose(fp);
				return 1;
			};
		};
	};

	if (pipe(pipefd) != 0)
	{
		fprintf(stderr, "%s: pipe: %s\n", argv[0], strerror(errno));
		fclose(fp);
		return 1;
	};
	
	// now the IPv6 daemon
	pid = fork();
	if (pid == -1)
	{
		fprintf(stderr, "%s: fork failed: %s\n", argv[0], strerror(errno));
		fclose(fp);
		return 1;
	}
	else if (pid == 0)
	{
		// close the read end of the pipe
		close(pipefd[0]);
		setsid();
		pid = fork();
		
		if (pid == -1)
		{
			char c = NETMAN_DAEMON_NOFORK;
			write(pipefd[1], &c, 1);
			close(pipefd[1]);
			exit(1);
		}
		else if (pid == 0)
		{
			gotoDaemon(AF_INET6, lockfile6, logfile6, pipefd[1], argv[1]);
		}
		else
		{
			// i am the parent; exit.
			exit(1);
		};
	}
	else
	{
		// close the write end of the pipe
		close(pipefd[1]);
		
		// wait for the daemon to write the status to the pipe
		char c = NETMAN_DAEMON_OK;
		read(pipefd[0], &c, 1);
		close(pipefd[0]);
		
		if (c != NETMAN_DAEMON_OK)
		{
			if (c == NETMAN_DAEMON_NOLOCK)
			{
				fprintf(stderr, "%s: interface %s already running\n", argv[0], argv[1]);
				fclose(fp);
				return 1;
			}
			else if (c == NETMAN_DAEMON_NOFORK)
			{
				fprintf(stderr, "%s: cannot start IPv6 daemon: second fork failed\n", argv[0]);
				fclose(fp);
				return 1;
			}
			else if (c == NETMAN_DAEMON_NONULL)
			{
				fprintf(stderr, "%s: cannot start IPv6 daemon: failed to open /dev/null", argv[0]);
				fclose(fp);
				return 1;
			}
			else if (c == NETMAN_DAEMON_NOLOG)
			{
				fprintf(stderr, "%s: cannot start IPv6 daemon: failed to open log file %s\n", argv[0], logfile4);
				fclose(fp);
				return 1;
			}
			else if (c == NETMAN_DAEMON_NOSIG)
			{
				fprintf(stderr, "%s: cannot start IPv6 daemon: sigaction failed\n", argv[0]);
				fclose(fp);
				return 1;
			}
			else
			{
				fprintf(stderr, "%s: unknown error occured\n", argv[0]);
				fclose(fp);
				return 1;
			};
		};
	};

	fclose(fp);
	
	return 0;
};
