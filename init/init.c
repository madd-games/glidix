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

#define _GLIDIX_SOURCE
#include <stdio.h>
#include <signal.h>
#include <stdlib.h>
#include <unistd.h>
#include <sys/glidix.h>
#include <sys/fsinfo.h>
#include <sys/mman.h>
#include <sys/call.h>
#include <sys/systat.h>
#include <string.h>
#include <fcntl.h>
#include <sys/wait.h>
#include <sys/ioctl.h>
#include <sys/stat.h>
#include <dirent.h>
#include <time.h>
#include <errno.h>

int shouldHalt = 0;
int shouldRunPoweroff = 0;
int ranPoweroff = 0;

//char* confRootType = NULL;
//char* confRootDevice = NULL;
//char** confExec = NULL;

struct fsinfo startupFSList[256];
size_t startupFSCount;

int isStartupFS(struct fsinfo *info)
{
	size_t i;
	for (i=0; i<256; i++)
	{
		if (info->fs_dev == startupFSList[i].fs_dev)
		{
			return 1;
		};
	};
	
	return 0;
};

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
};

#if 0
int interpret_config_option(char *line)
{
	char *saveptr;
	const char *cmd = strtok_r(line, " ", &saveptr);
	
	if (strcmp(cmd, "root") == 0)
	{
#if 0
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
#endif
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
	
	return 0;
};
#endif

#if 0
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
	
	if (fork() == 0)
	{
		execv(confExec[0], confExec);
		perror("execv");
		exit(1);
	};
	
	return 0;
};
#endif

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
	DIR *dirp = opendir("/dev");
	if (dirp == NULL)
	{
		return;
	};
	
	struct dirent *ent;
	while ((ent = readdir(dirp)) != NULL)
	{
		if (strlen(ent->d_name) == 3)
		{
			if (memcmp(ent->d_name, "sd", 2) == 0)
			{
				char fullname[256];
				sprintf(fullname, "/dev/%s", ent->d_name);
				close(open(fullname, O_RDONLY));
			};
		};
	};
	
	closedir(dirp);
};

/**
 * Find a process to kill (during shutdown), kill it and return 0. If no more processes need to be killed,
 * returns -1.
 */
int killNextProcess()
{
	DIR *dirp = opendir("/proc");
	if (dirp == NULL)
	{
		printf("init: critical error: failed to open /proc: %s\n", strerror(errno));
		printf("WARNING: waiting 5 seconds and skipping process termination!\n");
		sleep(5);
		return -1;
	};
	
	struct stat st;
	struct dirent *ent;
	
	while ((ent = readdir(dirp)) != NULL)
	{
		if (ent->d_name[0] == '.')
		{
			continue;
		}
		else if (strcmp(ent->d_name, "self") == 0)
		{
			continue;
		}
		else if (strcmp(ent->d_name, "1") == 0)
		{
			continue;
		}
		else
		{
			// kill it only if it's our child right now
			char parentPath[256];
			char exePath[256];
			char procName[256];
			sprintf(parentPath, "/proc/%s/parent", ent->d_name);
			sprintf(exePath, "/proc/%s/exe", ent->d_name);
			procName[readlink(exePath, procName, 256)] = 0;
			
			if (stat(parentPath, &st) != 0)
			{
				printf("init: failed to stat %s: %s\n", parentPath, strerror(errno));
				printf("init: assuming process terminated\n");
				continue;
			};
			
			if (st.st_rdev == 1)
			{
				int pid;
				sscanf(ent->d_name, "%d", &pid);
				
				kill(pid, SIGTERM);
				
				alarm(10);
				int status = waitpid(pid, NULL, 0);
				int errnum = errno;
				alarm(0);
				
				if (status == -1)
				{
					if (errnum == EINTR)
					{
						printf("init: process %d ('%s') failed to terminate in 10 seconds; killing\n",
							pid, procName);

						kill(pid, SIGKILL);
						if (waitpid(pid, NULL, 0) == -1)
						{
							printf("init: waitpid unexpectedly failed on %d: %s\n",
								pid, strerror(errno));
							printf("init: assuming process terminated\n");
							continue;
						};
					}
					else
					{
						printf("init: waitpid unexpectedly failed on %d: %s\n", pid, strerror(errnum));
						printf("init: assuming process terminated\n");
						continue;
					};
				};
				
				// we terminated a process successfully
				closedir(dirp);
				return 0;
			};
		};
	};
	
	closedir(dirp);
	return -1;
};

void onShutdownAlarm(int sig, siginfo_t *si, void *ignore)
{
	// NOP; just to interrupt the waitpid().
};

void shutdownSystem(int action)
{
	struct sigaction sa;
	memset(&sa, 0, sizeof(struct sigaction));
	sa.sa_handler = SIG_IGN;
	
	sigaction(SIGCHLD, &sa, NULL);
	sigaction(SIGINT, &sa, NULL);
	sigaction(SIGTERM, &sa, NULL);
	sigaction(SIGHUP, &sa, NULL);
	
	sa.sa_flags = SA_SIGINFO;
	sa.sa_sigaction = onShutdownAlarm;
	sigaction(SIGALRM, &sa, NULL);
	
	printf("init: asking remaining processes to terminate...\n");
	while (killNextProcess() == 0);
	
	printf("init: closing my remaining files...\n");
	int fd;
	for (fd=3; fd<sysconf(_SC_OPEN_MAX); fd++)
	{
		close(fd);
	};
	
	printf("init: unmounting non-startup filesystems...\n");
	struct fsinfo currentFSList[256];
	size_t count = _glidix_fsinfo(currentFSList, 256);
	
	size_t i;
	for (i=0; i<count; i++)
	{
		if (!isStartupFS(&currentFSList[i]))
		{
			if (_glidix_unmount(currentFSList[i].fs_mntpoint) != 0)
			{
				printf("init: failed to unmount %s: %s\n", currentFSList[i].fs_mntpoint, strerror(errno));
				printf("init: waiting 5 seconds and skipping this filesystem\n");
				sleep(5);
			};
		};
	};
	
	printf("init: removing all kernel modules...\n");
	DIR *dirp = opendir("/sys/mod");
	if (dirp == NULL)
	{
		printf("init: failed to open /sys/mod: %s\n", strerror(errno));
		printf("init: waiting 5 seconds and assuming modules don't need unloading\n");
		sleep(5);
	}
	else
	{
		struct dirent *ent;
		while ((ent = readdir(dirp)) != NULL)
		{
			if (ent->d_name[0] != '.')
			{
				if (_glidix_rmmod(ent->d_name, 0) != 0)
				{
					printf("init: failed to rmmod %s: %s\n", ent->d_name, strerror(errno));
					printf("Report this problem to the module developer.\n");
					printf("I will now hang, you may turn off power manually.\n");
					printf("If the problem persists, remove the module for your safety.\n");
					while (1) pause();
				};
			};
		};
		
		closedir(dirp);
	};
	
	printf("init: bringing the system down...\n");
	_glidix_down(action);
};

void id_to_string(char *buffer, uint8_t *bootid)
{
	size_t i;
	for (i=0; i<16; i++)
	{
		sprintf(&buffer[2*i], "%02hhX", bootid[i]);
	};
};

char **devList = NULL;

int try_mount_root_candidate(const char *fstype, const char *image, uint8_t *bootid)
{
	// try mounting it first
	if (_glidix_mount(fstype, image, "/", 0) != 0)
	{
		return -1;
	};
	
	// if it mounted correctly, verify that the boot ID is as expected
	struct fsinfo list[256];
	size_t count = _glidix_fsinfo(list, 256);

	size_t i;
	for (i=0; i<count; i++)
	{
		if (strcmp(list[i].fs_mntpoint, "/") == 0)
		{
			char idbuf[33];
			id_to_string(idbuf, list[i].fs_bootid);

			printf("init: boot ID of %s on %s is %s\n", fstype, image, idbuf);
			if (memcmp(list[i].fs_bootid, bootid, 16) == 0)
			{
				printf("init: root filesystem detected as %s on %s\n", fstype, image);
				return 0;
			}
			else
			{
				break;
			};
		};
	};
	
	// not the correct ID, unmount
	_glidix_unmount("/");
	return -1;
};

int try_mount_root_with_type(const char *fstype, uint8_t *bootid)
{
	//DIR *dirp = opendir("/dev");
	//if (dirp == NULL)
	//{
	//	fprintf(stderr, "init: cannot scan /dev: %s\n", strerror(errno));
	//	return -1;
	//};
	
	//struct dirent *ent;
	//while ((ent = readdir(dirp)) != NULL)
	//{
	//	if (memcmp(ent->d_name, "sd", 2) == 0)
	//	{
	//		char fullpath[256];
	//		sprintf(fullpath, "/dev/%s", ent->d_name);
	//		if (try_mount_root_candidate(fstype, fullpath, bootid) == 0) return 0;
	//	};
	//};
	
	//closedir(dirp);
	//return -1;

	char **scan;
	for (scan=devList; *scan!=NULL; scan++)
	{
		char *dev = *scan;
		if (try_mount_root_candidate(fstype, dev, bootid) == 0) return 0;
	};

	return -1;
};

int try_mount_root()
{
	// get the list of devices
	size_t numDevs = 0;
	DIR *dirp = opendir("/dev");
	if (dirp == NULL)
	{
		fprintf(stderr, "init: failed to scan /dev: %s\n", strerror(errno));
		return -1;
	};

	struct dirent *ent;
	while ((ent = readdir(dirp)) != NULL)
	{
		if (memcmp(ent->d_name, "sd", 2) == 0)
		{
			char *devname = (char*) malloc(strlen(ent->d_name) + strlen("/dev/") + 1);
			sprintf(devname, "/dev/%s", ent->d_name);

			devList = (char**) realloc(devList, sizeof(char*) * (numDevs+1));
			devList[numDevs++] = devname;

			printf("init: detected candidate storage device: %s\n", devname);
		};
	};

	devList = (char**) realloc(devList, sizeof(char*) * (numDevs+1));
	devList[numDevs] = NULL;

	closedir(dirp);

	char names[256*16];
	int drvcount = (int) __syscall(__SYS_fsdrv, names, 256);
	if (drvcount == -1)
	{
		fprintf(stderr, "init: cannot get filesystem driver list: %s\n", strerror(errno));
		return 1;
	};

	struct system_state sst;
	if (__syscall(__SYS_systat, &sst, sizeof(struct system_state)) != 0)
	{
		fprintf(stderr, "init: failed to get system state: %s\n", strerror(errno));
	};
	
	char idbuf[33];
	id_to_string(idbuf, sst.sst_bootid);
	printf("init: kernel boot ID is %s\n", idbuf);

	const char *scan = names;
	while (drvcount--)
	{
		const char *fstype = scan;
		scan += 16;
		
		if (try_mount_root_with_type(fstype, sst.sst_bootid) == 0) return 0;
	};
	
	return -1;
};

int main(int argc, char *argv[])
{	
	if (getpid() == 1)
	{
		setenv("PATH", "/usr/local/bin:/usr/bin:/bin", 1);
		setenv("HOME", "/root", 1);
		setenv("LD_LIBRARY_PATH", "/usr/local/lib:/usr/lib:/lib", 1);

		struct sigaction sa;
		memset(&sa, 0, sizeof(struct sigaction));
		sa.sa_sigaction = on_signal;
		sigemptyset(&sa.sa_mask);
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

		// get the list of "startup filesystems", which shall not be unmounted when shutting down
		startupFSCount = _glidix_fsinfo(startupFSList, 256);
		
		_glidix_mount("ramfs", ".tmp", "/tmp/", 0);
		_glidix_mount("ramfs", ".run", "/run/", 0);
		_glidix_mount("ramfs", ".run", "/var/run/", 0);
		_glidix_mount("ramfs", ".run", "/", 0);
		
		if (mkdir("/sem", 01777) != 0)
		{
			perror("mkdir /run/sem");
			return 1;
		};
		
		_glidix_unmount("/");
		loadmods();
		
		printf("init: initializing partitions...\n");
		init_parts();
		
#if 0
		printf("init: loading configuration file /initrd/startup.conf...\n");
		if (load_config("/initrd/startup.conf") == -1)
		{
			printf("init: attempting startup with fallback configuration file /initrd/startup-fallback.conf...\n");
			if (load_config("/initrd/startup-fallback.conf") == -1)
			{
				printf("init: failed to start up\n");
			};
		};
#endif
		
		printf("init: looking for root filesystem...\n");
		if (try_mount_root() != 0)
		{
			printf("init: failed to find the root filesystem!\n");
			return 1;
		};
		
		printf("init: executing startup script...\n");
		if (fork() == 0)
		{
			execl("/bin/sh", "/bin/sh", "/etc/init/startup.sh", NULL);
			perror("init: exec");
			_exit(1);
		};
		
		while (1)
		{
			pause();
			if (shouldHalt)
			{
				tcsetpgrp(0, getpgrp());
				
				printf("init: received shutdown request\n");
				int fd = open("/run/down-action", O_RDONLY);
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

				shutdownSystem(action);
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
