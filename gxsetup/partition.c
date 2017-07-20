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

#include <sys/glidix.h>
#include <sys/stat.h>
#include <sys/wait.h>
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <string.h>
#include <dirent.h>
#include <fcntl.h>
#include <errno.h>

#include "render.h"
#include "msgbox.h"
#include "progress.h"

#define	NUM_SETUP_OPS			5

enum
{
	PART_TYPE_UNALLOCATED,
	PART_TYPE_UNUSED,
	PART_TYPE_GXFS,
	
	PART_TYPE_COUNT,
};

typedef struct Partition_
{
	int type;			/* PART_TYPE_* */
	char mntpoint[256];		/* or empty string if mount not wanted */
	size_t size;			/* in MB */
	int format;			/* 0 = don't format, 1 = format (GXFS only) */
	struct Partition_ *prev;
	struct Partition_ *next;
} Partition;

typedef struct Drive_
{
	char name[128];			/* sdX */
	int dirty;			/* whether or not to write the partition table */
	Partition* parts;		/* head of partition list or NULL if no table present */
	int fd;				/* disk file handle */
	struct Drive_ *next;
} Drive;

typedef struct
{
	uint8_t flags;
	uint8_t startHead;
	uint16_t startCylSector;
	uint8_t systemID;
	uint8_t endHead;
	uint16_t endCylSector;
	uint32_t startLBA;
	uint32_t partSize;
} __attribute__ ((packed)) MBRPart;

typedef struct
{
	char bootstrap[446];
	MBRPart parts[4];
	uint16_t sig;
} __attribute__ ((packed)) MBR;

typedef struct
{
	char mntpoint[256];
	char type[256];
	char device[256];
} FSTabEntry;

Drive *firstDrive = NULL;
static int startX, startY;

static FSTabEntry *fstab = NULL;
static size_t fsnum = 0;

static const char *startupScript = "\
# /etc/init/startup.sh\n\
# The startup script, responsible for starting up the whole system.\n\
# See startup(3) for more info.\n\
echo \"Mounting all filesystems...\"\n\
mount -a\n\
echo \"Loading configuration...\"\n\
. /etc/init/startup.conf\n\
echo \"Starting level 1 services...\"\n\
service state 1\n\
echo \"Loading kernel modules...\"\n\
ldmods\n\
echo \"Starting level 2 services...\"\n\
service state 2\n\
echo \"Starting login manager...\"\n\
exec $logmgr\n\
";

static const char *loginScript = "\
# /etc/init/login.sh\n\
# The login script, invoked when a user logs in.\n\
# See login(3) for more info.\n\
$SHELL\n\
echo \"logout\"\n\
";

static const char *startupConf = "\
# /etc/init/startup.conf\n\
# Startup configuration file. See startup(3) for more info.\n\
logmgr=\"%s\"\n\
";

Partition *splitArea(Partition *area, size_t offMB, size_t sizeMB)
{
	if (area->type != PART_TYPE_UNALLOCATED)
	{
		// refuse to split it; it's an overlap
		return area;
	};
	
	Partition *part = (Partition*) malloc(sizeof(Partition));
	part->type = PART_TYPE_UNUSED;
	part->mntpoint[0] = 0;
	part->size = sizeMB;
	part->format = 0;
	part->prev = NULL;
	part->next = NULL;
	
	Partition *head;
	if (offMB == 0)
	{
		head = part;
		part->prev = area->prev;
	}
	else
	{
		head = (Partition*) malloc(sizeof(Partition));
		head->type = PART_TYPE_UNALLOCATED;
		head->mntpoint[0] = 0;
		head->size = offMB;
		head->format = 0;
		head->prev = area->prev;
		head->next = part;
		part->prev = head;
	};
	
	if ((offMB + sizeMB) == area->size)
	{
		// we extend to the end of the area
		part->next = area->next;
	}
	else
	{
		// unallocated space after us
		part->next = (Partition*) malloc(sizeof(Partition));
		part->next->type = PART_TYPE_UNALLOCATED;
		part->next->mntpoint[0] = 0;
		part->next->size = area->size - sizeMB - offMB;
		part->next->format = 0;
		part->next->prev = part;
		part->next->next = area->next;
	};
	
	// get rid of the original area
	free(area);
	
	// thanks
	return head;
};

void driveDetected(const char *name)
{
	char path[256];
	sprintf(path, "/dev/%s", name);
	
	int fd = open(path, O_RDWR);
	if (fd == -1)
	{
		return;
	};
	
	struct stat st;
	if (fstat(fd, &st) != 0)
	{
		close(fd);
		return;
	};
	
	Drive *drive = (Drive*) malloc(sizeof(Drive));
	if (drive == NULL)
	{
		return;
	};
	
	strcpy(drive->name, name);
	drive->dirty = 0;
	drive->parts = NULL;
	drive->fd = fd;
	drive->next = NULL;
	
	if (firstDrive == NULL)
	{
		firstDrive = drive;
	}
	else
	{
		Drive *last = firstDrive;
		while (last->next != NULL) last = last->next;
		last->next = drive;
	};
	
	// try to read the partition table
	MBR mbr;
	read(fd, &mbr, 512);
	
	if (mbr.sig != 0xAA55)
	{
		// no partition table here
		return;
	};
	
	// make sure all partitions are valid
	int i;
	for (i=0; i<4; i++)
	{
		if (mbr.parts[i].systemID != 0)
		{
			if ((mbr.parts[i].startLBA & 0x3F) != 0)
			{
				// start LBA not MB-aligned
				return;
			};
			
			if ((mbr.parts[i].partSize & 0x3F) != 0)
			{
				// size not multiple of MB
				return;
			};
		};
	};
	
	// parse the partitions, ignore overlappings
	drive->parts = (Partition*) malloc(sizeof(Partition));
	drive->parts->type = PART_TYPE_UNALLOCATED;
	drive->parts->mntpoint[0] = 0;
	drive->parts->size = (st.st_size - 512) / (1024*1024);
	drive->parts->format = 0;
	drive->parts->prev = NULL;
	drive->parts->next = NULL;
	
	for (i=0; i<4; i++)
	{
		if (mbr.parts[i].systemID != 0)
		{
			// partition!
			size_t offMB = (mbr.parts[i].startLBA >> 11) - 1;
			size_t sizeMB = mbr.parts[i].partSize >> 11;
			size_t endMB = offMB + sizeMB;
			
			// see if it fits within the first unallocated area
			if (endMB <= drive->parts->size)
			{
				drive->parts = splitArea(drive->parts, offMB, sizeMB);
			}
			else
			{
				Partition *scan = drive->parts;
				
				while (scan->next != NULL)
				{
					offMB -= scan->size;
					endMB = offMB + sizeMB;
					
					if (endMB <= scan->next->size)
					{
						scan->next = splitArea(scan->next, offMB, sizeMB);
					};
					
					scan = scan->next;
				};
			};
		};
	};
};

int partInit()
{
	DIR *dirp = opendir("/dev");
	if (dirp == NULL)
	{
		msgbox("ERROR", "Cannot open /dev!");
		return -1;
	};
	
	struct dirent *ent;
	while ((ent = readdir(dirp)) != NULL)
	{
		if (memcmp(ent->d_name, "sd", 2) == 0)
		{
			if (strlen(ent->d_name) == 3)
			{
				driveDetected(ent->d_name);
			};
		};
	};
	
	closedir(dirp);
	if (firstDrive == NULL)
	{
		msgbox("ERROR", "No drives found!");
		return -1;
	};
	
	return 0;
};

static int selected = 0;
static int tableSize;
static Drive *selectedDrive = NULL;
static Drive *parentDrive;
static Partition *selectedPart = NULL;

static void drawTable()
{
	selectedDrive = NULL;
	selectedPart = NULL;
	
	setCursor((uint8_t)startX, (uint8_t)startY);
	setColor(COLOR_WINDOW);
	printf("%-12s|%-19s|%-6s|%-20s|%-9s", "NAME", "SIZE (MB)", "USE AS", "MOUNT POINT", "FORMAT?");
	
	int i = 0;
	Drive *drive;
	
	for (drive=firstDrive; drive!=NULL; drive=drive->next)
	{
		setCursor((uint8_t)startX, (uint8_t)(startY+i+1));
		
		if (i == selected)
		{
			setColor(COLOR_SELECTION);
			selectedDrive = drive;
		}
		else
		{
			setColor(COLOR_WINDOW);
		};
		
		char label[70];
		if (drive->dirty)
		{
			sprintf(label, "%s*", drive->name);
		}
		else
		{
			strcpy(label, drive->name);
		};
		
		printf("%-70s", label);
		i++;
		
		Partition *part;
		int partno = 0;
		for (part=drive->parts; part!=NULL; part=part->next)
		{
			char prefix = 195;
			if (part->next == NULL) prefix = 192;
			
			setCursor((uint8_t)startX, (uint8_t)(startY+i+1));
			
			char name[12];
			if (part->type == PART_TYPE_UNALLOCATED)
			{
				strcpy(name, "UNALLOCATED");
			}
			else
			{
				sprintf(name, "%s%d", drive->name, partno++);
			};
			
			const char *useLabel = "";
			if (part->type == PART_TYPE_UNUSED)
			{
				useLabel = "-";
			}
			else if (part->type == PART_TYPE_GXFS)
			{
				useLabel = "GXFS";
			};
			
			char format = ' ';
			if (part->format) format = '*';

			if (i == selected)
			{
				setColor(COLOR_SELECTION);
				selectedPart = part;
				parentDrive = drive;
			}
			else
			{
				setColor(COLOR_WINDOW);
			};;
			
			printf("%c%-11s %-19lu %-6s %-20s [%c]      ",
				prefix, name, part->size, useLabel, part->mntpoint, format);
			i++;
		};
	};
	
	tableSize = i;
	setCursor(79, 24);
};

static int getDriveAction()
{
	static const char *labels[2] = {"Go back", "Create MBR partition table"};
	
	int option = 0;
	
	int startX, startY;
	renderWindow("\30" "\31" "Select action\t<ENTER> Make selection",
			"NEW PARTITION TABLE",
			50, 4,
			&startX, &startY);
	
	setCursor((uint8_t)startX, (uint8_t)startY);
	setColor(COLOR_WINDOW);
	printf("Drive '%s' has no partition table.", selectedDrive->name);
	
	while (1)
	{
		int i;
		for (i=0; i<2; i++)
		{
			setCursor((uint8_t)startX, (uint8_t)(startY+2+i));
			
			char prefix;
			if (i == option)
			{
				setColor(COLOR_SELECTION);
				prefix = 16;
			}
			else
			{
				setColor(COLOR_WINDOW);
				prefix = ' ';
			};
			
			printf("%c%-49s", prefix, labels[i]);
		};

		setCursor(79, 24);

		while (1)
		{
			uint8_t c;
			if (read(0, &c, 1) != 1) continue;
			
			if (c == 0x8B)
			{
				// up
				if (option != 0) option--;
				break;
			}
			else if (c == 0x8C)
			{
				// down
				if (option != 1) option++;
				break;
			}
			else if (c == '\n')
			{
				return option;
			};
		};
	};
};

static void newPartTable()
{
	struct stat st;
	if (fstat(selectedDrive->fd, &st) != 0)
	{
		msgbox("ERROR", "Failed to stat the drive!");
		return;
	};
	
	selectedDrive->parts = (Partition*) malloc(sizeof(Partition));
	selectedDrive->parts->type = PART_TYPE_UNALLOCATED;
	selectedDrive->parts->mntpoint[0] = 0;
	selectedDrive->parts->size = (st.st_size - 512) / (1024*1024);
	selectedDrive->parts->format = 0;
	selectedDrive->parts->prev = NULL;
	selectedDrive->parts->next = NULL;
	
	selectedDrive->dirty = 1;
};

static int newPartDialog(size_t *sizeptr)
{
	int numParts = 0;
	Partition *part;
	for (part=parentDrive->parts; part!=NULL; part=part->next)
	{
		if (part->type != PART_TYPE_UNALLOCATED)
		{
			numParts++;
		};
	};
	
	if (numParts == 4)
	{
		msgbox("ERROR", "Only 4 partitions are possible!");
		return 2;		// cancel
	};
	
	size_t size = selectedPart->size;
	static const char *optionLabels[3] = {"Create at start of space", "Create at end of space", "Cancel"};
	int option = 0;
	
	int startX, startY;
	renderWindow("\30" "\31" " Select option\t<ENTER> Accept\t<0-9> Enter size",
			"NEW PARTITION",
			50, 5,
			&startX, &startY);
	
	setCursor((uint8_t)startX, (uint8_t)startY);
	setColor(COLOR_WINDOW);
	printf("Size in MB:");
	
	while (1)
	{
		setCursor((uint8_t)(startX+20), (uint8_t)startY);
		setColor(COLOR_TEXTFIELD);
		printf("%-30lu", size);
		
		int i;
		for (i=0; i<3; i++)
		{
			setCursor((uint8_t)startX, (uint8_t)(startY+2+i));
			
			char prefix;
			if (i == option)
			{
				setColor(COLOR_SELECTION);
				prefix = 16;
			}
			else
			{
				setColor(COLOR_WINDOW);
				prefix = ' ';
			};
			
			printf("%c%-49s", prefix, optionLabels[i]);
		};
		
		setCursor(79, 24);
		
		while (1)
		{
			uint8_t c;
			if (read(0, &c, 1) != 1) continue;
			
			if (c == 0x8B)
			{
				// up
				if (option != 0) option--;
				break;
			}
			else if (c == 0x8C)
			{
				// down
				if (option != 2) option++;
				break;
			}
			else if (c == '\n')
			{
				if ((size <= selectedPart->size) && (size > 0))
				{
					*sizeptr = size;
					return option;
				};
			}
			else if ((c >= '0') && (c <= '9'))
			{
				size = size * 10 + c - '0';
				break;
			}
			else if (c == '\b')
			{
				size /= 10;
				break;
			};
		};
	};
};

void partEdit()
{
	int startX, startY;
	renderWindow("\30" "\31" " Select option\t<ENTER> Alter option",
			"EDIT PARTITION",
			50, 5,
			&startX, &startY);
	
	int option = 0;
	
	while (1)
	{
		int i;
		for (i=0; i<5; i++)
		{
			setCursor((uint8_t)startX, (uint8_t)(startY+i));
			
			char prefix;
			if (i == option)
			{
				setColor(COLOR_SELECTION);
				prefix = 16;
			}
			else
			{
				setColor(COLOR_WINDOW);
				prefix = ' ';
			};
			
			if (i == 0)
			{
				const char *useAs = "-";
				if (selectedPart->type == PART_TYPE_GXFS)
				{
					useAs = "GXFS";
				};
				
				printf("%cUse as:%42s", prefix, useAs);
			}
			else if (i == 1)
			{
				printf("%cMount point:%37s", prefix, selectedPart->mntpoint);
			}
			else if (i == 2)
			{
				const char *fmt = "NO";
				if (selectedPart->format) fmt = "YES";
				
				printf("%cFormat?:%41s", prefix, fmt);
			}
			else if (i == 3)
			{
				printf("%c%-49s", prefix, "Save changes");
			}
			else
			{
				printf("%c%-49s", prefix, "Delete partition");
			};
		};
		
		setCursor(79, 24);
		
		while (1)
		{
			uint8_t c;
			if (read(0, &c, 1) != 1) continue;
			
			if (c == 0x8B)
			{
				// up
				if (option != 0) option--;
				break;
			}
			else if (c == 0x8C)
			{
				// down
				if (option != 4) option++;
				break;
			}
			else if (c == '\n')
			{
				if (option == 0)
				{
					selectedPart->type = (selectedPart->type % (PART_TYPE_COUNT-1)) + 1;
				}
				else if (option == 2)
				{
					selectedPart->format = !selectedPart->format;
				}
				else if (option == 3)
				{
					parentDrive->dirty = 1;
					return;
				}
				else if (option == 4)
				{
					selectedPart->type = PART_TYPE_UNALLOCATED;
					selectedPart->mntpoint[0] = 0;
					selectedPart->format = 0;
					
					// see if we can "engulf" the area on the right
					if (selectedPart->next != NULL)
					{
						if (selectedPart->next->type == PART_TYPE_UNALLOCATED)
						{
							Partition *nextPart = selectedPart->next;
							selectedPart->next = nextPart->next;
							
							if (nextPart->next != NULL)
							{
								nextPart->next->prev = selectedPart;
							};
							
							selectedPart->size += nextPart->size;
							free(nextPart);
						};
					};
					
					// see if we can "engulf" the area on the left
					if (selectedPart->prev != NULL)
					{
						if (selectedPart->prev->type == PART_TYPE_UNALLOCATED)
						{
							Partition *prevPart = selectedPart->prev;
							selectedPart->prev = prevPart->prev;
							
							if (prevPart->prev != NULL)
							{
								prevPart->prev->next = selectedPart;
							}
							else
							{
								// we've just become the first partition
								parentDrive->parts = selectedPart;
							};
							
							selectedPart->size += prevPart->size;
							free(prevPart);
						};
					};
					
					selected = 0;
					parentDrive->dirty = 1;
					return;
				};
				
				break;
			}
			else if (c == '\b')
			{
				if (option == 1)
				{
					if (strlen(selectedPart->mntpoint) != 0)
					{
						selectedPart->mntpoint[strlen(selectedPart->mntpoint)-1] = 0;
					};
					
					break;
				};
			}
			else if ((c >= 32) && (c <= 127))
			{
				if (option == 1)
				{
					if (strlen(selectedPart->mntpoint) < 30)
					{
						char put[2];
						put[0] = (char) c;
						put[1] = 0;
						
						strcat(selectedPart->mntpoint, put);
					};
					
					break;
				};
			};
		};
	};
};

int haveMountpointExcept(const char *point, Partition *except)
{
	Drive *drive;
	for (drive=firstDrive; drive!=NULL; drive=drive->next)
	{
		Partition *part;
		for (part=drive->parts; part!=NULL; part=part->next)
		{
			if (part != except)
			{
				if (strcmp(part->mntpoint, point) == 0)
				{
					return 1;
				};
			};
		};
	};
	
	return 0;
};

int isTableOK()
{
	if (!haveMountpointExcept("/", NULL))
	{
		msgbox("ERROR", "You must select a root partition!");
		return 0;
	};
	
	Drive *drive;
	for (drive=firstDrive; drive!=NULL; drive=drive->next)
	{
		Partition *part;
		for (part=drive->parts; part!=NULL; part=part->next)
		{
			if (part->mntpoint[0] != 0)
			{
				if (haveMountpointExcept(part->mntpoint, part))
				{
					char msg[70];
					sprintf(msg, "Duplicate mountpoint: %s", part->mntpoint);
					msgbox("ERROR", msg);
					return 0;
				};
			};
			
			if ((part->type == PART_TYPE_UNUSED) && (part->mntpoint[0] != 0))
			{
				msgbox("ERROR", "Unused partitions cannot be mounted!");
				return 0;
			};
			
			if ((part->format) && (part->type != PART_TYPE_GXFS))
			{
				msgbox("ERROR", "Only GXFS partitions may be formatted!");
				return 0;
			};
			
			if ((strcmp(part->mntpoint, "/") == 0) && (part->type != PART_TYPE_GXFS))
			{
				msgbox("ERROR", "The root partition must be GXFS!");
				return 0;
			};
			
			if ((part->format) && (part->size < 32))
			{
				msgbox("ERROR", "A partition must be at least 32MB to format");
				return 0;
			};
			
			if ((strcmp(part->mntpoint, "/") == 0) && (part->size < 1024))
			{
				msgbox("ERROR", "The root partition must be at least 1GB!");
				return 0;
			};
		};
	};
	
	return 1;
};

void partEditor()
{
	while (1)
	{
		renderWindow("\30" "\31" " Select partition\t<ENTER> Edit partition\t" "\x1A" " Save and continue",
				"PARTITIONING",
				70, 20,
				&startX, &startY);
		drawTable();
		
		while (1)
		{
			uint8_t c;
			if (read(0, &c, 1) != 1) continue;
			
			if (c == 0x8B)
			{
				// up arrow
				if (selected != 0) selected--;
				break;
			}
			else if (c == 0x8C)
			{
				// down arrow
				if (selected != (tableSize-1)) selected++;
				break;
			}
			else if (c == 0x8E)
			{
				// right arrow
				if (isTableOK()) return;
				break;
			}
			else if (c == '\n')
			{
				if (selectedDrive != NULL)
				{
					if (selectedDrive->parts == NULL)
					{
						if (getDriveAction() == 1)
						{
							newPartTable();
						};
						
						break;
					};
				}
				else if (selectedPart != NULL)
				{
					if (selectedPart->type == PART_TYPE_UNALLOCATED)
					{
						size_t newSize;
						int option = newPartDialog(&newSize);
						
						if (option == 0)
						{
							// new partition at start of area
							if (selectedPart->prev != NULL)
							{
								selectedPart->prev->next = splitArea(selectedPart, 0, newSize);
							}
							else
							{
								parentDrive->parts = splitArea(selectedPart, 0, newSize);
							};
							
							parentDrive->dirty = 1;
						}
						else if (option == 1)
						{
							// new partition at end of area
							size_t off = selectedPart->size - newSize;
							
							if (selectedPart->prev != NULL)
							{
								selectedPart->prev->next = splitArea(selectedPart, off, newSize);
							}
							else
							{
								parentDrive->parts = splitArea(selectedPart, off, newSize);
							};
							
							parentDrive->dirty = 1;
						}
						else
						{
							// NOP
						};
					}
					else
					{
						partEdit();
					};
					
					break;
				};
			};
		};
	};
};

void formatDrive(const char *path)
{
	pid_t pid = fork();
	if (pid == -1)
	{
		msgbox("ERROR", "fork() failed!");
		return;
	}
	else if (pid == 0)
	{
		close(0);
		close(1);
		close(2);
		open("/dev/null", O_RDWR);
		dup(0);
		dup(1);
		execl("/usr/bin/mkgxfs", "mkgxfs", path, NULL);
		exit(1);
	}
	else
	{
		int status;
		waitpid(pid, &status, 0);
		
		int ok = WIFEXITED(status) && (WEXITSTATUS(status) == 0);
		if (!ok)
		{
			msgbox("ERROR", "mkgxfs failed!");
			return;
		};
	};
};

static void makeMountPointDirs(const char *mntpoint)
{
	char dirname[256];
	strcpy(dirname, "/mnt/");
	char *put = &dirname[4];
	
	mntpoint = &mntpoint[4];
	
	while (*mntpoint != 0)
	{
		do
		{
			*put++ = *mntpoint++;
		} while ((*mntpoint != 0) && (*mntpoint != '/'));
		
		*put = 0;
		mkdir(dirname, 0755);
	};
};

void partFlush()
{
	int numDrivesToEdit = 0;
	Drive *drive;
	for (drive=firstDrive; drive!=NULL; drive=drive->next)
	{
		if (drive->dirty) numDrivesToEdit++;
	};
	
	if (numDrivesToEdit == 0)
	{
		return;
	};
	
	int drivesDone = 0;
	for (drive=firstDrive; drive!=NULL; drive=drive->next)
	{
		if (drive->dirty)
		{
			char msg[30];
			sprintf(msg, "Partitioning %s...", drive->name);
			drawProgress("WRITING PARTITIONS", msg, drivesDone, numDrivesToEdit);
			
			MBR mbr;
			memset(&mbr, 0, 512);
			
			/* "int 0x18" */
			mbr.bootstrap[0] = 0xCD;
			mbr.bootstrap[1] = 0x18;
			mbr.sig = 0xAA55;
			
			MBRPart *put = mbr.parts;
			
			Partition *part;
			size_t offset = 1;
			for (part=drive->parts; part!=NULL; part=part->next)
			{
				if (part->type != PART_TYPE_UNALLOCATED)
				{
					if (strcmp(part->mntpoint, "/") == 0)
					{
						put->flags = 0x80;
					}
					else
					{
						put->flags = 0;
					};
					
					put->systemID = 0x7F;
					put->startLBA = (offset << 11);
					put->partSize = (part->size << 11);
					put++;
				};
				
				offset += part->size;
			};

			lseek(drive->fd, 0, SEEK_SET);
			write(drive->fd, &mbr, 512);
			
			drivesDone++;
		};
		
		close(drive->fd);
	};
	
	// format the necessary partitions
	for (drive=firstDrive; drive!=NULL; drive=drive->next)
	{
		drawProgress("WRITING PARTITIONS", "Formatting...", 1, 1);
		
		int partIndex = 0;
		Partition *part;
		for (part=drive->parts; part!=NULL; part=part->next)
		{
			if (part->type != PART_TYPE_UNALLOCATED)
			{
				if (part->format)
				{
					char pathname[256];
					sprintf(pathname, "/dev/%s%d", drive->name, partIndex);
					
					formatDrive(pathname);
				};
				
				partIndex++;
			};
		};
	};
	
	// sort the partitions into a list of mountpoints (fstab) sorted by ascending mountpoint
	// path length; since this is the order we'll have to mount them in.
	while (1)
	{
		Partition *shortestPart = NULL;
		char shortestMountPoint[256];
		char deviceName[256];
		
		for (drive=firstDrive; drive!=NULL; drive=drive->next)
		{
			int partIndex = 0;
			Partition *part;
			
			for (part=drive->parts; part!=NULL; part=part->next)
			{
				if (part->type == PART_TYPE_GXFS)
				{
					if (part->mntpoint[0] != 0)
					{
						int thisOne = 0;
						
						if (shortestPart == NULL)
						{
							thisOne = 1;
						}
						else
						{
							if (strlen(part->mntpoint) < strlen(shortestMountPoint))
							{
								thisOne = 1;
							};
						};
						
						if (thisOne)
						{
							shortestPart = part;
							strcpy(shortestMountPoint, part->mntpoint);
							sprintf(deviceName, "/dev/%s%d", drive->name, partIndex);
						};
					};
				};
				
				partIndex++;
			};
		};
		
		if (shortestPart == NULL)
		{
			break;
		};
		
		shortestPart->mntpoint[0] = 0;
		
		size_t fstabIndex = fsnum++;
		fstab = (FSTabEntry*) realloc(fstab, sizeof(FSTabEntry)*fsnum);
		sprintf(fstab[fstabIndex].mntpoint, "/mnt%s", shortestMountPoint);
		strcpy(fstab[fstabIndex].type, "gxfs");
		strcpy(fstab[fstabIndex].device, deviceName);
		
		char *mntpoint = fstab[fstabIndex].mntpoint;
		if (mntpoint[strlen(mntpoint)-1] != '/')
		{
			strcat(mntpoint, "/");
		};
	};
	
	// install the bootloader
	drawProgress("SETUP", "Installing bootloader...", 0, NUM_SETUP_OPS);
	size_t idx;
	char bootdev[256];
	for (idx=0; idx<fsnum; idx++)
	{
		if (strcmp(fstab[idx].mntpoint, "/mnt/") == 0)
		{
			memcpy(bootdev, fstab[idx].device, 8);	/* /dev/sdX */
			bootdev[8] = 0;
		};
	};
	
	// we can simply assume the boot device exists; the partition editor forces the user
	// to specify one.
	pid_t pid = fork();
	if (pid == -1)
	{
		msgbox("ERROR", "fork() failed");
		return;
	};
	
	if (pid == 0)
	{
		close(0);
		close(1);
		close(2);
		open("/dev/null", O_RDWR);
		dup(0);
		dup(1);
		execl("/usr/bin/gxboot-i", "gxboot-install", bootdev, NULL);
		exit(1);
	};
	
	int status;
	waitpid(pid, &status, 0);
	
	if (!WIFEXITED(status))
	{
		msgbox("ERROR", "gxboot-install didn't terminate successfully!");
		return;
	};
	
	if (WEXITSTATUS(status) != 0)
	{
		msgbox("ERROR", "gxboot-install didn't terminate successfully!");
		return;
	};
	
	// mount all partitions
	drawProgress("SETUP", "Mounting filesystems...", 1, NUM_SETUP_OPS);
	for (idx=0; idx<fsnum; idx++)
	{
		makeMountPointDirs(fstab[idx].mntpoint);
		if (_glidix_mount(fstab[idx].type, fstab[idx].device, fstab[idx].mntpoint, 0) != 0)
		{
			char errmsg[256];
			sprintf(errmsg, "Cannot mount %s at %s: %s", fstab[idx].device, fstab[idx].mntpoint, strerror(errno));
			msgbox("ERROR", errmsg);
		};
	};
	
	// create all the directories that should exist on a glidix filesystem
	drawProgress("SETUP", "Creating directories...", 2, NUM_SETUP_OPS);
	mkdir("/mnt/usr", 0755);
	mkdir("/mnt/usr/bin", 0755);
	mkdir("/mnt/usr/lib", 0755);
	mkdir("/mnt/usr/libexec", 0755);
	mkdir("/mnt/usr/share", 0755);
	mkdir("/mnt/usr/local", 0755);
	mkdir("/mnt/usr/local/bin", 0755);
	mkdir("/mnt/usr/local/lib", 0755);
	mkdir("/mnt/usr/local/share", 0755);
	mkdir("/mnt/bin", 0755);
	mkdir("/mnt/lib", 0755);
	mkdir("/mnt/mnt", 0755);
	mkdir("/mnt/sys", 0755);
	mkdir("/mnt/sys/mod", 0755);
	mkdir("/mnt/proc", 0755);
	mkdir("/mnt/dev", 0755);
	mkdir("/mnt/run", 0755);
	mkdir("/mnt/var", 0755);
	mkdir("/mnt/var/run", 0755);
	mkdir("/mnt/var/log", 0755);
	mkdir("/mnt/etc", 0755);
	mkdir("/mnt/etc/init", 0755);
	mkdir("/mnt/etc/modules", 0755);
	mkdir("/mnt/boot", 0755);
	mkdir("/mnt/initrd", 0755);
	mkdir("/mnt/tmp", 0755);
	mkdir("/mnt/media", 0755);
	mkdir("/mnt/home", 0755);
	mkdir("/mnt/root", 0755);
	
	// configuration files
	drawProgress("SETUP", "Creating configuration files...", 3, NUM_SETUP_OPS);
	
	// set up /etc/fstab
	FILE *fp = fopen("/mnt/etc/fstab", "w");
	fprintf(fp, "# /etc/fstab\n");
	fprintf(fp, "# Lists filesystems to be automatically mounted on startup\n");
	fprintf(fp, "# See fstab(3) for more info.\n");
	
	for (idx=0; idx<fsnum; idx++)
	{
		if (strcmp(fstab[idx].mntpoint, "/mnt/") != 0)
		{
			char mntpoint[256];
			strcpy(mntpoint, &fstab[idx].mntpoint[4]);
			mntpoint[strlen(mntpoint)-1] = 0;		// remove final slash
			
			fprintf(fp, "%s\t%s\t%s\n", mntpoint, fstab[idx].type, fstab[idx].device);
		};
	};
	
	fclose(fp);
	
	// create the /etc/init/startup.sh script
	fp = fopen("/mnt/etc/init/startup.sh", "w");
	fprintf(fp, "%s", startupScript);
	fclose(fp);
	
	// create the /etc/init/login.sh script
	fp = fopen("/mnt/etc/init/login.sh", "w");
	fprintf(fp, "%s", loginScript);
	fclose(fp);
	
	// configure GWM
	//const char *logmgr = "gui";
	const char *logmgr = "logmgr";
	pid = fork();
	if (pid == -1)
	{
		msgbox("ERROR", "fork() failed for dispconf");
		return;
	};
	
	if (pid == 0)
	{
		close(0);
		close(1);
		close(2);
		open("/dev/null", O_RDWR);
		dup(0);
		dup(1);
		
		execl("/usr/bin/dispconf", "dispconf", "/mnt/etc/gwm.conf", NULL);
		exit(1);
	};
	
	waitpid(pid, &status, 0);
	if (!WIFEXITED(status))
	{
		logmgr = "logmgr";
	};
	
	if (WEXITSTATUS(status) != 0)
	{
		logmgr = "logmgr";
	};
	
	if (strcmp(logmgr, "logmgr") == 0)
	{
		msgbox("NOTICE", "Display configuration failed, disabling GUI.");
	};
	
	// create the /etc/init/startup.conf file
	fp = fopen("/mnt/etc/init/startup.conf", "w");
	fprintf(fp, startupConf, logmgr);
	fclose(fp);
	
	// now onto setting up the initrd
	drawProgress("SETUP", "Creating the initrd...", 4, NUM_SETUP_OPS);
	
	pid = fork();
	if (pid == -1)
	{
		msgbox("ERROR", "fork() failed for mkinitrd");
		return;
	};
	
	if (pid == 0)
	{
		close(0);
		close(1);
		close(2);
		open("/dev/null", O_RDWR);
		dup(0);
		dup(1);
		
		char optRootDev[512];
		for (idx=0; idx<fsnum; idx++)
		{
			if (strcmp(fstab[idx].mntpoint, "/mnt/") == 0)
			{
				sprintf(optRootDev, "--rootdev=%s", fstab[idx].device);
			};
		};
		execl("/usr/bin/mkinitrd", "mkinitrd", optRootDev, "--rootfs=gxfs", "--output=/mnt/boot/vmglidix.tar", NULL);
		exit(1);
	};
	
	waitpid(pid, &status, 0);
	
	if (!WIFEXITED(status))
	{
		msgbox("ERROR", "mkinitrd terminated with an error!");
		return;
	};
	
	if (WEXITSTATUS(status) != 0)
	{
		msgbox("ERROR", "mkinitrd terminated with an error!");
		return;
	};
};
