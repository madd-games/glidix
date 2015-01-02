/*
	Glidix Runtime

	Copyright (c) 2014-2015, Madd Games.
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

#define	IOCTL_NOARG(intf, cmd)				((intf << 16) | cmd)
#define	IOCTL_ARG(type, intf, cmd)			((sizeof(type) << 32) | IOCTL_NOARG(intf, cmd))
#define	IOCTL_PCI_DEVINFO				IOCTL_ARG(PCIDevinfoRequest, 1, 0x01)

typedef union
{
	struct
	{
		uint16_t				vendor;
		uint16_t				device;
		uint16_t				command;
		uint16_t				status;
		uint8_t					rev;
		uint8_t					progif;
		uint8_t					subclass;
		uint8_t					classcode;
		uint8_t					cacheLineSize;
		uint8_t					latencyTimer;
		uint8_t					headerType;
		uint8_t					bist;
		uint32_t				bar[6];
		uint32_t				cardbusCIS;
		uint16_t				subsysVendor;
		uint16_t				subsysID;
		uint32_t				expromBase;
		uint8_t					cap;
		uint8_t					resv[7];
		uint8_t					intline;
		uint8_t					intpin;
		uint8_t					mingrant;
		uint8_t					maxlat;
	} __attribute__ ((packed)) std;
	// TODO
} PCIDeviceConfig;

typedef struct
{
	uint8_t bus;
	uint8_t slot;
	uint8_t func;
	PCIDeviceConfig config;
} __attribute__ ((packed)) PCIDevinfoRequest;

void getline(char *buf)
{
	char c = 0;
	while (1)
	{
		fread(&c, 1, 1, stdin);
		if (c == '\n')
		{
			*buf = 0;
			break;
		};
		*buf++ = c;
	};
};

int _toint(const char *str)
{
	int out = 0;
	while (*str != 0)
	{
		out = out * 10 + ((*str++)-'0');
	};
	return out;
};

void cmd_cat(int argc, char **argv)
{
	if (argc < 2)
	{
		fprintf(stderr, "cat <filenames...>\n");
		fprintf(stderr, "  Display the contents of files.\n");
	}
	else
	{
		int i;
		for (i=1; i<argc; i++)
		{
			FILE *fp = fopen(argv[i], "r");
			if (fp == NULL)
			{
				fprintf(stderr, "cat: could not open %s\n", argv[i]);
				break;
			};
			//asm volatile ("xchg %bx, %bx");
			while (1)
			{
				int c = fgetc(fp);
				if (c == EOF) break;
				fputc(c, stdout);
				fflush(stdout);
			};
			fclose(fp);
		};
	};
};

void cmd_write(int argc, char **argv)
{
	if (argc != 3)
	{
		fprintf(stderr, "write <filename> <data>\n");
		fprintf(stderr, "  Writes some data to a file\n");
	}
	else
	{
		int fd = open(argv[1], O_CREAT | O_WRONLY, 0644);
		if (fd == -1)
		{
			fprintf(stderr, "write: could not open %s for write\n", argv[1]);
			return;
		};

		ssize_t sz = write(fd, argv[2], strlen(argv[2]));
		close(fd);

		if (sz == -1)
		{
			fprintf(stderr, "write: the write failed\n");
		}
		else
		{
			printf("successfully wrote %d bytes to %s\n", sz, argv[1]);
		};
	};
};

void cmd_run(int argc, char **argv)
{
	if (argc != 2)
	{
		fprintf(stderr, "run <executable>\n");
		fprintf(stderr, "  Runs an executable file.\n");
	}
	else
	{
		char pars[4] = {0, 0, 0, 0};
		pid_t pid = fork();
		if (pid == 0)
		{
			// i can into child
			if (_glidix_exec(argv[1], pars, 4) != 0)
			{
				fprintf(stderr, "cannot execute %s\n", argv[1]);
				exit(1);
			};
		}
		else
		{
			// i can into parent
			int status;
			waitpid(pid, &status, 0);
			printf("child (%d) terminated with status %d\n", pid, status);
		};
	};
};

void cmd_insmod(int argc, char **argv)
{
	if (argc < 3)
	{
		fprintf(stderr, "insmod [-v] <mod_name> <mod_file> <opts>\n");
		fprintf(stderr, "  Link a module into the kernel (-v makes it verbose)\n");
	}
	else
	{
		int flags = 0;
		const char *modname = NULL;
		const char *path = NULL;
		const char *opt = NULL;

		int i;
		for (i=1; i<argc; i++)
		{
			if (argv[i][0] == '-')
			{
				char *arg = argv[i];
				int j;
				for (j=1; j<strlen(arg); j++)
				{
					if (arg[j] == 'v')
					{
						flags |= _GLIDIX_INSMOD_VERBOSE;
					};
				};
			}
			else
			{
				if (modname == NULL) modname = argv[i];
				else if (path == NULL) path = argv[i];
				else if (opt == NULL) opt = argv[i];
			};
		};

		if (modname == NULL)
		{
			fprintf(stderr, "insmod: no module name specified\n");
			return;
		};

		if (path == NULL)
		{
			fprintf(stderr, "insmod: no module path specified\n");
			return;
		};

		if (_glidix_insmod(modname, path, opt, flags) != 0)
		{
			fprintf(stderr, "insmod: module loading failed\n");
			return;
		};
	};
};

void cmd_pcistat(int argc, char **argv)
{
	if (argc != 3)
	{
		fprintf(stderr, "pcistat <bus> <slot>\n");
		fprintf(stderr, "  Display information about a PCI device.\n");
	}
	else
	{
		PCIDevinfoRequest rq;
		rq.bus = _toint(argv[1]);
		rq.slot = _toint(argv[2]);
		rq.func = 0;

		int fd = open("/dev/pcictl", O_RDONLY);
		if (fd < 0)
		{
			fprintf(stderr, "pcistat: failed to open /dev/pcictl\n");
			return;
		};

		if (ioctl(fd, IOCTL_PCI_DEVINFO, &rq) != 0)
		{
			close(fd);
			fprintf(stderr, "pcistat: failed to ioctl\n");
			return;
		};

		close(fd);

		printf("Vendor:   0x%x\n", rq.config.std.vendor);
		printf("Device:   0x%x\n", rq.config.std.device);
		printf("Class:    0x%x\n", rq.config.std.classcode);
		printf("Subclass: 0x%x\n", rq.config.std.subclass);
	};
};

void lspci_print_dev(PCIDevinfoRequest rq)
{
	printf("%d\t\t%d\t\t\t%x\t\t%x\t\t%x\t\t%x\n",
		rq.bus, rq.slot, rq.config.std.vendor, rq.config.std.device, rq.config.std.classcode, rq.config.std.subclass
	);
};

void cmd_lspci()
{
	PCIDevinfoRequest rq;
	rq.func = 0;

	printf("Bus\t\tSlot\t\tVendor\t\tDevice\t\tClass\tSubclass\n");

	int fd = open("/dev/pcictl", O_RDONLY);
	if (fd == -1)
	{
		fprintf(stderr, "lspci: failed to open /dev/pcictl\n");
		return;
	};

	int bus, slot;
	for (bus=0; bus<256; bus++)
	{
		for (slot=0; slot<32; slot++)
		{
			rq.bus = (uint8_t) bus;
			rq.slot = (uint8_t) slot;
			rq.func = 0;
			ioctl(fd, IOCTL_PCI_DEVINFO, &rq);
			if (rq.config.std.vendor != 0xFFFF)
			{
				lspci_print_dev(rq);
				if (rq.config.std.headerType & 0x80)
				{
					for (rq.func=1; rq.func<8; rq.func++)
					{
						ioctl(fd, IOCTL_PCI_DEVINFO, &rq);
						if (rq.config.std.vendor != 0xFFFF)
						{
							lspci_print_dev(rq);
						};
					};
				};
			};
		};
	};
};

void cmd_stat(int argc, char **argv)
{
	if (argc != 2)
	{
		fprintf(stderr, "stat <filename>\n");
		fprintf(stderr, "  Display information about a file.\n");
	}
	else
	{
		struct stat st;
		if (stat(argv[1], &st) != 0)
		{
			fprintf(stderr, "stat: stat failed on %s\n", argv[1]);
			return;
		};

		printf("filename: %s\n", argv[1]);
		printf("owner: %d\tgroup:%d\n", st.st_uid, st.st_gid);
		printf("mode: %o\tsize: %d bytes\n", st.st_mode, st.st_size);
		const char *type = "???";
		if (S_ISBLK(st.st_mode))
		{
			type = "block device";
		}
		else if (S_ISCHR(st.st_mode))
		{
			type = "character device";
		}
		else if (S_ISDIR(st.st_mode))
		{
			type = "directory";
		}
		else if (S_ISFIFO(st.st_mode))
		{
			type = "FIFO";
		}
		else if (S_ISREG(st.st_mode))
		{
			type = "regular file";
		}
		else if (S_ISLNK(st.st_mode))
		{
			type = "symbolic link";
		};
		printf("type: %s\n", type);
	};
};

void cmd_ls(int argc, char **argv)
{
	if (argc != 2)
	{
		fprintf(stderr, "ls <directory>\n");
		fprintf(stderr, "  Display the contents of a directory\n");
	}
	else
	{
		DIR *dirp = opendir(argv[1]);
		if (dirp == NULL)
		{
			fprintf(stderr, "ls: could not open directory %s\n", argv[1]);
			return;
		};

		struct dirent *ent;
		while ((ent = readdir(dirp)) != NULL)
		{
			printf("%s\n", ent->d_name);
		};

		closedir(dirp);
	};
};

void printbyte(uint8_t byte)
{
	uint8_t high = (byte >> 4) & 0xF;
	uint8_t low =  byte & 0xF;

	const char *hexd = "0123456789ABCDEF";
	printf("%c%c ", hexd[high], hexd[low]);
};

void cmd_hexdump(int argc, char **argv)
{
	if (argc != 3)
	{
		fprintf(stderr, "hexdump <filename> <offset>\n");
		fprintf(stderr, "  Displays the hexdump 256 bytes of a file at offset.\n");
	}
	else
	{
		int index = _toint(argv[2]);
		int fd = open(argv[1], O_RDONLY);
		if (fd < 0)
		{
			fprintf(stderr, "hexdump: could not open %s\n", argv[1]);
			return;
		};

		lseek(fd, index, SEEK_SET);
		uint8_t buf[256];
		ssize_t sz = read(fd, buf, 256);
		close(fd);

		if (sz == -1)
		{
			fprintf(stderr, "hexdump: read() failed for some reason\n");
			return;
		};

		int x, y;
		for (y=0; y<16; y++)
		{
			for (x=0; x<16; x++)
			{
				printbyte(buf[y * 16 + x]);
			};

			printf("  ");

#if 0
			for (x=0; x<16; x++)
			{
				char c = (char) buf[y * 16 + x];
				printf("%c", c);
			};
#endif

			printf("\n");
		};

		printf("Total size actually read: %d\n", sz);
	};
};

void cmd_mount(int argc, char **argv)
{
	if (argc != 4)
	{
		fprintf(stderr, "mount <filesystem> <image> <mountpoint>\n");
		fprintf(stderr, "  Mount a filesystem stored on 'image' (a storage device or a virtual drive image) at the specified mountpoint.\n");
		fprintf(stderr, "  The mountpoint must end in '/'\n");
	}
	else
	{
		if (_glidix_mount(argv[1], argv[2], argv[3], 0) != 0)
		{
			fprintf(stderr, "mount: error\n");
		};
	};
};

void runCommand(char *buf)
{
	int argc = 1;
	char *scan = buf;
	while (*scan != 0)
	{
		if (*scan == ' ') argc++;
		scan++;
	};

	char **argv = (char**) malloc(sizeof(char*)*argc);
	argv[0] = buf;
	scan = buf;
	int i = 1;
	while (*scan != 0)
	{
		if (*scan == ' ')
		{
			argv[i++] = (scan+1);
			*scan = 0;
		};
		scan++;
	};

	if (strcmp(argv[0], "cat") == 0)
	{
		cmd_cat(argc, argv);
	}
	else if (strcmp(argv[0], "write") == 0)
	{
		cmd_write(argc, argv);
	}
	else if (strcmp(argv[0], "pid") == 0)
	{
		printf("%d\n", getpid());
	}
	else if (strcmp(argv[0], "exit") == 0)
	{
		if (getpid() != 2) exit(0);
	}
	else if (strcmp(argv[0], "run") == 0)
	{
		cmd_run(argc, argv);
	}
	else if (strcmp(argv[0], "insmod") == 0)
	{
		cmd_insmod(argc, argv);
	}
	else if (strcmp(argv[0], "pcistat") == 0)
	{
		cmd_pcistat(argc, argv);
	}
	else if (strcmp(argv[0], "lspci") == 0)
	{
		cmd_lspci();
	}
	else if (strcmp(argv[0], "stat") == 0)
	{
		cmd_stat(argc, argv);
	}
	else if (strcmp(argv[0], "ls") == 0)
	{
		cmd_ls(argc, argv);
	}
	else if (strcmp(argv[0], "hexdump") == 0)
	{
		cmd_hexdump(argc, argv);
	}
	else if (strcmp(argv[0], "diag") == 0)
	{
		_glidix_diag();
	}
	else if (strcmp(argv[0], "mount") == 0)
	{
		cmd_mount(argc, argv);
	}
	else
	{
		fprintf(stderr, "unknown command: %s\n", argv[0]);
	};
};

void shell()
{
	while (1)
	{
		printf("> ");
		fflush(stdout);
		char buf[256];
		getline(buf);
		if (buf[0] == 0) continue;
		runCommand(buf);
	};
};

void loadmods()
{
	DIR *dirp = opendir("/initrd/initmod");
	struct dirent *ent;

	char filename[128];
	char modname[128];
	while ((ent = readdir(dirp)) != NULL)
	{
		strcpy(filename, "/initrd/initmod/");
		strcat(filename, ent->d_name);

		strcpy(modname, "mod_");
		strcat(modname, ent->d_name);
		modname[strlen(modname)-4] = 0;

		printf("init: insmod %s (from %s)\n", modname, filename);
		_glidix_insmod(modname, filename, NULL, 0);
	};

	closedir(dirp);
};

int main(int argc, char *argv[])
{
	if (getpid() == 1)
	{
		//_glidix_insmod("mod_ps2kbd", "/initrd/ps2kbd.gkm", NULL, 0);
		loadmods();
		if (fork() == 0)
		{
			shell();
		};
		while (1);
	}
	else
	{
		shell();
	};
	return 0;
};
