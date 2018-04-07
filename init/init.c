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
#include <sys/mount.h>
#include <sys/statvfs.h>
#include <sys/module.h>

int shouldHalt = 0;
int shouldRunPoweroff = 0;
int ranPoweroff = 0;

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

void on_signal(int sig, siginfo_t *si, void *ignore)
{
	if (sig == SIGCHLD)
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
			char parentLink[256];
			sprintf(parentPath, "/proc/%s/parent", ent->d_name);
			sprintf(exePath, "/proc/%s/exe", ent->d_name);
			procName[readlink(exePath, procName, 256)] = 0;
			
			int iAmParent = 0;
			if (stat(parentPath, &st) != 0)
			{
				// if the 'parent' link is broken, it means 'init' is indeed the parent
				iAmParent = 1;
			}
			else
			{
				parentLink[readlink(parentPath, parentLink, 256)] = 0;
				if (strcmp(parentLink, "../1") == 0)
				{
					iAmParent = 1;
				};
			};
			
			if (iAmParent)
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
	
	printf("init: unmounting filesystems...\n");
	struct fsinfo currentFSList[256];
	chdir("/");
	chroot("rootfs");
	size_t count = _glidix_fsinfo(currentFSList, 256);
	chroot(".");
	
	int i;
	for (i=count-1; i>=0; i--)
	{
		char actual_mntpoint[512];
		sprintf(actual_mntpoint, "/rootfs/%s", currentFSList[i].fs_mntpoint);
		
		printf("init: unmount %s\n", actual_mntpoint);
		if (unmount(actual_mntpoint, 0) != 0)
		{
			printf("init: failed to unmount %s: %s\n", currentFSList[i].fs_mntpoint, strerror(errno));
			printf("init: waiting 5 seconds and skipping this filesystem\n");
			sleep(5);
		};
	};
	
	printf("init: removing all kernel modules...\n");
	struct modstat ms;
	for (i=0; i<512; i++)
	{
		if (modstat(i, &ms) == 0)
		{
			if (rmmod(ms.mod_name, 0) != 0)
			{
				printf("init: failed to rmmod %s: %s\n", ms.mod_name, strerror(errno));
				printf("Report this problem to the module developer.\n");
				printf("I will now hang, you may turn off power manually.\n");
				printf("If the problem persists, remove the module for your safety.\n");
				while (1) pause();
			};
		};
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
char *rootImage = NULL;

int try_mount_root_candidate(const char *fstype, const char *image, uint8_t *bootid)
{
	// try mounting it first
	if (mount(fstype, image, "/rootfs", 0, NULL, 0) != 0)
	{
		return -1;
	};
	
	// if it mounted correctly, verify that the boot ID is as expected
	struct statvfs st;
	if (statvfs("/rootfs", &st) == 0)
	{
		if (memcmp(bootid, st.f_bootid, 16) == 0)
		{
			rootImage = strdup(image);
			return 0;
		};
	};
	
	// not the right boot ID, unmount
	unmount("/rootfs", 0);
	return -1;
};

int try_mount_root_with_type(const char *fstype, uint8_t *bootid)
{
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
	// create the /rootfs directory
	if (mkdir("/rootfs", 0755) != 0)
	{
		fprintf(stderr, "init: failed to create /rootfs: %s\n", strerror(errno));
		return -1;
	};
	
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
		if (open("/dev/tty0", O_RDWR) != 0) return 1;
		if (dup(0) != 1) return 1;
		if (dup(1) != 2) return 1;
		
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
		
		if (mkdir("/sem", 01777) != 0)
		{
			perror("mkdir /run/sem");
			return 1;
		};
		
		loadmods();
		
		printf("init: initializing partitions...\n");
		init_parts();
		
		printf("init: looking for root filesystem...\n");
		if (try_mount_root() != 0)
		{
			printf("init: failed to find the root filesystem!\n");
			return 1;
		};
		
		printf("init: setting up second-level filesystem...\n");
		if (mount("bind", "/dev", "/rootfs/dev", 0, NULL, 0) != 0)
		{
			perror("init: bind /dev");
			return 1;
		};

		if (mount("bind", "/proc", "/rootfs/proc", 0, NULL, 0) != 0)
		{
			perror("init: bind /proc");
			return 1;
		};

		if (mount("bind", "/initrd", "/rootfs/initrd", 0, NULL, 0) != 0)
		{
			perror("init: bind /initrd");
			return 1;
		};

		if (mount("bind", "/run", "/rootfs/run", 0, NULL, 0) != 0)
		{
			perror("init: bind /run");
			return 1;
		};

		if (mount("bind", "/run", "/rootfs/var/run", 0, NULL, 0) != 0)
		{
			perror("init: bind /var/run");
			return 1;
		};

		printf("init: setting up fsinfo...\n");
		int fd = open("/run/fsinfo", O_WRONLY | O_CREAT | O_EXCL, 0644);
		if (fd == -1)
		{
			perror("init: open /run/fsinfo");
			return 1;
		};
		
		struct __fsinfo_record record;
		memset(&record, 0, sizeof(struct __fsinfo_record));
		strcpy(record.__image, rootImage);
		strcpy(record.__mntpoint, "/");
		write(fd, &record, sizeof(struct __fsinfo_record));
		strcpy(record.__image, "none");
		strcpy(record.__mntpoint, "/dev");
		write(fd, &record, sizeof(struct __fsinfo_record));
		strcpy(record.__mntpoint, "/proc");
		write(fd, &record, sizeof(struct __fsinfo_record));
		strcpy(record.__mntpoint, "/initrd");
		write(fd, &record, sizeof(struct __fsinfo_record));
		strcpy(record.__mntpoint, "/run");
		write(fd, &record, sizeof(struct __fsinfo_record));
		strcpy(record.__mntpoint, "/var/run");
		write(fd, &record, sizeof(struct __fsinfo_record));
		close(fd);
		
		printf("init: executing startup script...\n");
		if (fork() == 0)
		{
			if (chdir("/rootfs") != 0)
			{
				fprintf(stderr, "init: cannot switch to /rootfs: %s\n", strerror(errno));
				_exit(1);
			};
			
			if (chroot("/rootfs") != 0)
			{
				fprintf(stderr, "init: failed to set root directory to /rootfs: %s\n", strerror(errno));
				_exit(1);
			};
			
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
