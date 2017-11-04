/*
	Glidix Shell Utilities

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

#include <sys/types.h>
#include <stdio.h>
#include <stdlib.h>
#include <fcntl.h>
#include <string.h>
#include <errno.h>
#include <unistd.h>

#define	FMT_PASSWD		"ssiisss"
#define	FMT_GROUP		"ssis"
#define	FMT_SHADOW		"ssssssss"

typedef struct PasswdEntry_
{
	struct PasswdEntry_*	prev;
	struct PasswdEntry_*	next;
	char*			username;
	char*			ex;		/* the string "x" (where old unices stored password hashes) */
	uint16_t		uid;
	uint16_t		gid;		/* primary group */
	char*			info;		/* typically "full name" under glidix */
	char*			home;
	char*			shell;
} PasswdEntry;

typedef struct GroupEntry_
{
	struct GroupEntry_*	prev;
	struct GroupEntry_*	next;
	char*			groupname;
	char*			ex;		/* the string "x" for some reason */
	uint16_t		gid;
	char*			members;	/* comma-separated member list */
} GroupEntry;

typedef struct ShadowEntry_
{
	struct ShadowEntry_*	prev;
	struct ShadowEntry_*	next;
	char*			username;
	char*			hash;
	char*			resv[6];	/* six reserved fields */
} ShadowEntry;

typedef struct
{
	void*			prev;
	void*			next;
	char			data[];
} Tuple;

void printShadow(FILE *fp, ShadowEntry *table)
{
	ShadowEntry *ent;
	for (ent=table; ent!=NULL; ent=ent->next)
	{
		fprintf(fp, "%s:%s:%s:%s:%s:%s:%s:%s\n", ent->username, ent->hash, ent->resv[0], ent->resv[1], ent->resv[2],
								ent->resv[3], ent->resv[4], ent->resv[5]);
	};
};

void printGroup(FILE *fp, GroupEntry *table)
{
	GroupEntry *ent;
	for (ent=table; ent!=NULL; ent=ent->next)
	{
		fprintf(fp, "%s:x:%hu:%s\n", ent->groupname, ent->gid, ent->members);
	};
};

void printPasswd(FILE *fp, PasswdEntry *table)
{
	PasswdEntry *ent;
	for (ent=table; ent!=NULL; ent=ent->next)
	{
		fprintf(fp, "%s:x:%hu:%hu:%s:%s:%s\n", ent->username, ent->uid, ent->gid, ent->info, ent->home, ent->shell);
	};
};

/**
 * Parse a line, divided by ':' operators, into fields, according to the given format. Returns 0
 * on success, or -1 if there is an error.
 *
 * The contents of 'line' are destroyed.
 */
int parseLine(char *line, const char *format, void *structPtr)
{
	char *put = (char*) structPtr;
	char *saveptr;
	char *tok;
	
	for (tok=strtok_r(line, ":", &saveptr); tok!=NULL; tok=strtok_r(NULL, ":", &saveptr))
	{
		char type = *format++;
		if (type == 0)
		{
			break;
		}
		else if (type == 's')
		{
			put = (char*) (((uint64_t)put + 7) & ~7);
			*((char**)put) = strdup(tok);
			put += 8;
		}
		else if (type == 'i')
		{
			put = (char*) (((uint64_t)put + 1) & ~1);
			sscanf(tok, "%hu", (uint16_t*) put);
			put += 2;
		}
		else
		{
			return -1;
		};
	};
	
	return 0;
};

/**
 * Read a whole table from a file. The first 2 fields in the structure must be "prev" and "next",
 * followed by the required table fields.
 */
void* loadTable(int fd, const char *format)
{
	// figure out the size of a tuple
	size_t tupleSize = 16;	/* prev and next */
	const char *scan;
	for (scan=format; *scan!=0; scan++)
	{
		if (*scan == 's')
		{
			tupleSize = (tupleSize + 7) & ~7;
			tupleSize += 8;
		}
		else if (*scan == 'i')
		{
			tupleSize = (tupleSize + 1) & ~1;
			tupleSize += 2;
		}
		else
		{
			return NULL;
		};
	};
	
	tupleSize = (tupleSize + 15) & ~15;
	
	// read the whole file contents
	char *data = NULL;
	size_t size = 0;
	char buffer[1024];
	
	while (1)
	{
		ssize_t sz = read(fd, buffer, 1024);
		if (sz == 0) break;
		
		data = realloc(data, size + sz);
		memcpy(&data[size], buffer, sz);
		size += sz;
	};
	
	data = realloc(data, size+1);
	data[size] = 0;
	
	char *saveptr;
	Tuple *last = NULL;
	Tuple *first = NULL;
	
	char *line;
	for (line=strtok_r(data, "\n", &saveptr); line!=NULL; line=strtok_r(NULL, "\n", &saveptr))
	{
		if (data[0] == 0) continue;
		
		Tuple *tuple = (Tuple*) malloc(tupleSize);
		tuple->prev = last;
		tuple->next = NULL;
		if (last == NULL)
		{
			first = last = tuple;
		}
		else
		{
			last->next = tuple;
			last = tuple;
		};
		
		if (parseLine(line, format, tuple->data) != 0) return NULL;
	};
	
	return first;
};

/**
 * Remove a user from the given group. Returns 0 when the user has been removed, or was not
 * in the group at all; it returns -1 if the group is now empty and must be removed.
 */
int removeFromGroup(GroupEntry *ent, const char *username)
{
	char *newMembers = (char*) malloc(strlen(ent->members)+1);
	newMembers[0] = 0;
	
	char *saveptr;
	char *tok;
	for (tok=strtok_r(ent->members, ",", &saveptr); tok!=NULL; tok=strtok_r(NULL, ",", &saveptr))
	{
		if (strcmp(tok, username) != 0)
		{
			if (newMembers[0] == 0)
			{
				strcpy(newMembers, tok);
			}
			else
			{
				strcat(newMembers, ",");
				strcat(newMembers, tok);
			};
		};
	};
	
	if (newMembers[0] == 0)
	{
		free(newMembers);
		return -1;		// group now empty
	};
	
	free(ent->members);
	ent->members = newMembers;
	return 0;
};

int main(int argc, char *argv[])
{
	if (geteuid() != 0)
	{
		fprintf(stderr, "%s: you must run me as root\n", argv[0]);
		return 1;
	};
	
	int fdPasswd = open("/etc/passwd", O_RDWR);
	if (fdPasswd == -1)
	{
		fprintf(stderr, "%s: failed to open /etc/passwd: %s\n", argv[0], strerror(errno));
		return 1;
	};
	
	int fdGroup = open("/etc/group", O_RDWR);
	if (fdGroup == -1)
	{
		fprintf(stderr, "%s: failed to open /etc/group: %s\n", argv[0], strerror(errno));
		return 1;
	};
	
	int fdShadow = open("/etc/shadow", O_RDWR);
	if (fdShadow == -1)
	{
		fprintf(stderr, "%s: failed to open /etc/shadow: %s\n", argv[0], strerror(errno));
		return 1;
	};
	
	struct flock lock;
	memset(&lock, 0, sizeof(struct flock));
	lock.l_type = F_WRLCK;
	lock.l_whence = SEEK_SET;
	lock.l_start = 0;
	lock.l_len = 0;
	
	if (fcntl(fdPasswd, F_SETLKW, &lock) != 0)
	{
		fprintf(stderr, "%s: failed to lock /etc/passwd: %s\n", argv[0], strerror(errno));
		return 1;
	};
	
	if (fcntl(fdGroup, F_SETLKW, &lock) != 0)
	{
		fprintf(stderr, "%s: failed to lock /etc/group: %s\n", argv[0], strerror(errno));
		return 1;
	};
	
	if (fcntl(fdShadow, F_SETLKW, &lock) != 0)
	{
		fprintf(stderr, "%s: failed to lock /etc/shadow: %s\n", argv[0], strerror(errno));
		return 1;
	};
	
	// load all the database components
	PasswdEntry *tabPasswd = (PasswdEntry*) loadTable(fdPasswd, FMT_PASSWD);
	if (tabPasswd == NULL)
	{
		fprintf(stderr, "%s: failed to parse /etc/passwd!\n", argv[0]);
		return 1;
	};
	
	GroupEntry *tabGroup = (GroupEntry*) loadTable(fdGroup, FMT_GROUP);
	if (tabGroup == NULL)
	{
		fprintf(stderr, "%s: failed to parse /etc/group!\n", argv[0]);
		return 1;
	};
	
	ShadowEntry *tabShadow = (ShadowEntry*) loadTable(fdShadow, FMT_SHADOW);
	if (tabShadow == NULL)
	{
		fprintf(stderr, "%s: failed to parse /etc/shadow!\n", argv[0]);
		return 1;
	};
	
	// now we can parse the control script
	char line[1024];
	while (fgets(line, 1024, stdin) != NULL)
	{
		char *newline = strchr(line, '\n');
		if (newline != NULL) *newline = 0;
		
		char *commentPos = strchr(line, '#');
		if (commentPos != NULL) *commentPos = 0;
		
		char *saveptr;
		char *cmd = strtok_r(line, " \t", &saveptr);
		if (cmd == NULL) continue;
		
		if (strcmp(cmd, "print") == 0)
		{
			fprintf(stderr, "[passwd]\n");
			printPasswd(stderr, tabPasswd);
			fprintf(stderr, "[group]\n");
			printGroup(stderr, tabGroup);
			fprintf(stderr, "[shadow]\n");
			printShadow(stderr, tabShadow);
		}
		else if (strcmp(cmd, "save") == 0)
		{
			lseek(fdPasswd, 0, SEEK_SET);
			lseek(fdGroup, 0, SEEK_SET);
			lseek(fdShadow, 0, SEEK_SET);
			
			ftruncate(fdPasswd, 0);
			ftruncate(fdGroup, 0);
			ftruncate(fdShadow, 0);
			
			FILE *fpPasswd = fdopen(dup(fdPasswd), "w");
			printPasswd(fpPasswd, tabPasswd);
			fclose(fpPasswd);
			
			FILE *fpGroup = fdopen(dup(fdGroup), "w");
			printGroup(fpGroup, tabGroup);
			fclose(fpGroup);
			
			FILE *fpShadow = fdopen(dup(fdShadow), "w");
			printShadow(fpShadow, tabShadow);
			fclose(fpShadow);
		}
		else if (strcmp(cmd, "adduser") == 0)
		{
			char *username = strtok_r(NULL, " \t", &saveptr);
			char *hash = strtok_r(NULL, " \t", &saveptr);
			char *home = strtok_r(NULL, " \t", &saveptr);
			char *shell = strtok_r(NULL, " \t", &saveptr);
			char *info = strtok_r(NULL, "", &saveptr);
			
			if (username == NULL || hash == NULL || home == NULL || shell == NULL)
			{
				fprintf(stderr, "%s: `adduser' syntax error\n", argv[0]);
				return 1;
			};
			
			PasswdEntry *ent;
			for (ent=tabPasswd; ent!=NULL; ent=ent->next)
			{
				if (strcmp(ent->username, username) == 0)
				{
					fprintf(stderr, "%s: user `%s' already exits\n", argv[0], username);
					return 1;
				};
			};
			
			GroupEntry *gent;
			for (gent=tabGroup; gent!=NULL; gent=gent->next)
			{
				if (strcmp(gent->groupname, username) == 0)
				{
					fprintf(stderr, "%s: cannot create user `%s' because a group with this name exists\n",
						argv[0], username);
					return 1;
				};
			};
			
			// allocate user ID
			uint16_t uid;
			for (uid=1000; uid!=0; uid++)
			{
				int found = 0;
				for (ent=tabPasswd; ent!=NULL; ent=ent->next)
				{
					if (ent->uid == uid)
					{
						found = 1;
						break;
					};
				};
				
				if (!found) break;
			};
			
			if (uid == 0)
			{
				fprintf(stderr, "%s: ran out of user IDs!\n", argv[0]);
				return 1;
			};
			
			// allocate group ID
			uint16_t gid;
			for (gid=1000; gid!=0; gid++)
			{
				int found = 0;
				for (gent=tabGroup; gent!=NULL; gent=gent->next)
				{
					if (gent->gid == gid)
					{
						found = 1;
						break;
					};
				};
				
				if (!found) break;
			};
			
			if (gid == 0)
			{
				fprintf(stderr, "%s: ran out of group IDs!\n", argv[0]);
				return 1;
			};
			
			// add an entry to /etc/passwd
			if (info == NULL) info = username;
			PasswdEntry *pwnew = (PasswdEntry*) malloc(sizeof(PasswdEntry));
			pwnew->next = NULL;
			pwnew->username = strdup(username);
			pwnew->ex = strdup("x");
			pwnew->uid = uid;
			pwnew->gid = gid;
			pwnew->info = strdup(info);
			pwnew->home = strdup(home);
			pwnew->shell = strdup(shell);
			
			if (tabPasswd == NULL)
			{
				pwnew->prev = NULL;
				tabPasswd = pwnew;
			}
			else
			{
				ent = tabPasswd;
				while (ent->next != NULL) ent = ent->next;
				ent->next = pwnew;
				pwnew->prev = ent;
			};
			
			// add the group to /etc/group
			GroupEntry *grnew = (GroupEntry*) malloc(sizeof(GroupEntry));
			grnew->next = NULL;
			grnew->groupname = strdup(username);
			grnew->ex = strdup("x");
			grnew->gid = gid;
			grnew->members = strdup(username);
			
			if (tabGroup == NULL)
			{
				grnew->prev = NULL;
				tabGroup = grnew;
			}
			else
			{
				gent = tabGroup;
				while (gent->next != NULL) gent = gent->next;
				gent->next = grnew;
				grnew->prev = gent;
			};
			
			// and finally password hash in /etc/shadow
			ShadowEntry *shnew = (ShadowEntry*) malloc(sizeof(ShadowEntry));
			shnew->next = NULL;
			shnew->username = strdup(username);
			shnew->hash = strdup(hash);
			
			int i;
			for (i=0; i<6; i++)
			{
				shnew->resv[i] = strdup("0");
			};
			
			if (tabShadow == NULL)
			{
				shnew->prev = NULL;
				tabShadow = shnew;
			}
			else
			{
				ShadowEntry *shent = tabShadow;
				while (shent->next != NULL) shent = shent->next;
				shent->next = shnew;
				shnew->prev = shent;
			};
		}
		else if (strcmp(cmd, "rmuser") == 0)
		{
			char *username = strtok_r(NULL, " \t", &saveptr);
			if (username == NULL)
			{
				fprintf(stderr, "%s: `rmuser' syntax error\n", argv[0]);
				return 1;
			};
			
			// first remove the entry from /etc/passwd
			int found = 0;
			PasswdEntry *ent;
			for (ent=tabPasswd; ent!=NULL; ent=ent->next)
			{
				if (strcmp(ent->username, username) == 0)
				{
					found = 1;
					
					if (ent->prev != NULL) ent->prev->next = ent->next;
					if (ent->next != NULL) ent->next->prev = ent->prev;
					if (tabPasswd == ent) tabPasswd = ent->next;
					
					free(ent);
					break;
				};
			};
			
			if (!found)
			{
				fprintf(stderr, "%s: no user named `%s'\n", argv[0], username);
				return 1;
			};
			
			// since we were successful, remove this user from any groups that
			// they are a member of
			GroupEntry *grent = tabGroup;
			while (grent != NULL)
			{
				if (removeFromGroup(grent, username) == -1)
				{
					if (grent->prev != NULL) grent->prev->next = grent->next;
					if (grent->next != NULL) grent->next->prev = grent->prev;
					if (tabGroup == grent) tabGroup = grent->next;
					
					GroupEntry *next = grent->next;
					free(grent);
					grent = next;
				}
				else
				{
					grent = grent->next;
				};
			};
			
			// now remove from the shadow file
			ShadowEntry *shent;
			for (shent=tabShadow; shent!=NULL; shent=shent->next)
			{
				if (strcmp(shent->username, username) == 0)
				{
					if (shent->prev != NULL) shent->prev->next = shent->next;
					if (shent->next != NULL) shent->next->prev = shent->prev;
					if (tabShadow == shent) tabShadow = shent->next;
					
					free(shent);
					break;
				};
			};
		}
		else if (strcmp(cmd, "hash") == 0)
		{
			char *username = strtok_r(NULL, " \t", &saveptr);
			char *hash = strtok_r(NULL, " \t", &saveptr);
			
			if (username == NULL || hash == NULL)
			{
				fprintf(stderr, "%s: `hash' syntax error\n", argv[0]);
				return 1;
			};

			int found = 0;
			ShadowEntry *ent;
			for (ent=tabShadow; ent!=NULL; ent=ent->next)
			{
				if (strcmp(ent->username, username) == 0)
				{
					found = 1;
					free(ent->hash);
					ent->hash = strdup(hash);
					break;
				};
			};
			
			if (!found)
			{
				fprintf(stderr, "%s: no user named `%s'\n", argv[0], username);
				return 1;
			};
		}
		else if (strcmp(cmd, "gethash") == 0)
		{
			char *username = strtok_r(NULL, " \t", &saveptr);
			
			if (username == NULL)
			{
				fprintf(stderr, "%s: `gethash' syntax error\n", argv[0]);
				return 1;
			};
			
			int found = 0;
			ShadowEntry *ent;
			for (ent=tabShadow; ent!=NULL; ent=ent->next)
			{
				if (strcmp(ent->username, username) == 0)
				{
					printf("%s\n", ent->hash);
					found = 1;
					break;
				};
			};
			
			if (!found)
			{
				fprintf(stderr, "%s: no user named `%s'\n", argv[0], username);
				return 1;
			};
		}
		else
		{
			fprintf(stderr, "%s: unknown control command: %s\n", argv[0], cmd);
			return 1;
		};
	};
	
	close(fdPasswd);
	close(fdGroup);
	close(fdShadow);
	return 0;
};
