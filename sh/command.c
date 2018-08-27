/*
	Glidix Shell

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

#include <sys/wait.h>
#include <unistd.h>
#include <stdlib.h>
#include <stdio.h>
#include <errno.h>
#include <string.h>
#include <signal.h>
#include <fcntl.h>
#include <pwd.h>
#include <grp.h>

#include "command.h"
#include "strops.h"
#include "dict.h"
#include "sh.h"

#define	WHITESPACES	" \t\n"

static int findExecPath(const char *cmd, char *path)
{
	if (strchr(cmd, '/') != NULL)
	{
		if (strlen(cmd) >= PATH_MAX) return -1;
		else
		{
			strcpy(path, cmd);
			return 0;
		};
	}
	else if (strcmp(cmd, "[") == 0)
	{
		strcpy(path, "/usr/bin/test");
		return 0;
	}
	else
	{
		const char *pathspec = dictGet(&dictEnviron, "PATH");
		if (pathspec == NULL)
		{
			return -1;
		};
		
		char *pathlist = strdup(pathspec);
		char *saveptr;
		char *token;
		
		for (token=strtok_r(pathlist, ":", &saveptr); token!=NULL; token=strtok_r(NULL, ":", &saveptr))
		{
			if ((strlen(token) + strlen(cmd) + 1) < PATH_MAX)
			{
				sprintf(path, "%s/%s", token, cmd);
				if (access(path, X_OK) == 0)
				{
					free(pathlist);
					return 0;
				};
			};
		};
		
		free(pathlist);
		return -1;
	};
};

static int executeGroup(CmdGroup *group)
{
	// because the members are sorted right-to-left, the group leader is spawned first
	// and so all children can see its PID, which is necessary to properly set the group.
	int childrenLeft = 0;
	
	int prevInput;
	CmdMember *member;
	for (member=group->firstMember; member!=NULL; member=member->next)
	{
		Dict localEnviron;
		dictInitFrom(&localEnviron, dictEnviron.list);
		
		member->pid = -1;
		
		char **ptr;
		for (ptr=member->tokens; *ptr!=NULL; ptr++)
		{
			if (str_find(*ptr, "=", "\"'") == NULL)
			{
				break;
			};
		
			dictPut(&localEnviron, *ptr);
		};

		if (*ptr == NULL)
		{
			member->status = 0;
			continue;
		}
		else if (strcmp(*ptr, "exit") == 0)
		{
			if (ptr[1] == NULL)
			{
				exit(0);
			}
			else
			{
				int exitStatus;
				sscanf(ptr[1], "%d", &exitStatus);
				exit(exitStatus);
			};
		}
		else if (strcmp(*ptr, "export") == 0)
		{
			for (ptr++; *ptr!=NULL; ptr++)
			{
				if (strchr(*ptr, '=') != NULL)
				{
					dictPut(&dictEnviron, *ptr);
				}
				else
				{
					const char *currentValue = dictGet(&dictShellVars, *ptr);
					if (currentValue == NULL)
					{
						currentValue = "";
					};
					
					char spec[strlen(currentValue) + strlen(*ptr) + 2];
					sprintf(spec, "%s=%s", *ptr, currentValue);
					
					dictPut(&dictEnviron, spec);
				};
			};
			
			member->status = 0;
		}
		else if (strcmp(*ptr, "cd") == 0)
		{
			const char *dir;
			if (ptr[1] == NULL)
			{
				dir = dictGet(&dictEnviron, "HOME");
				if (dir == NULL)
				{
					dir = "/";
				};
			}
			else
			{
				dir = ptr[1];
			};
			
			if (chdir(dir) != 0)
			{
				fprintf(stderr, "cd: %s: %s\n", dir, strerror(errno));
				member->status = 0x0100;
				continue;
			};
			
			member->status = 0;
		}
		else if (strcmp(*ptr, "setuid") == 0)
		{
			if (ptr[1] == NULL)
			{
				fprintf(stderr, "SYNTAX:\tsetuid <username>\n");
				member->status = 0x0100;
				continue;
			};
			
			struct passwd *pw = getpwnam(ptr[1]);
			if (pw == NULL)
			{
				perror("setuid: getpwnam");
				member->status = 0x0100;
				continue;
			};
			
			if (setuid(pw->pw_uid) != 0)
			{
				perror("setuid");
				member->status = 0x0100;
				continue;
			};
			
			member->status = 0;
		}
		else if (strcmp(*ptr, "setgid") == 0)
		{
			if (ptr[1] == NULL)
			{
				fprintf(stderr, "SYNTAX:\tsetgid <username>\n");
				member->status = 0x0100;
				continue;
			};
			
			struct group *grp = getgrnam(ptr[1]);
			if (grp == NULL)
			{
				perror("setgid: getgrnam");
				member->status = 0x0100;
				continue;
			};
			
			if (setgid(grp->gr_gid) != 0)
			{
				perror("setgid");
				member->status = 0x0100;
				continue;
			};
			
			member->status = 0;
		}
		else if (strcmp(*ptr, ".") == 0)
		{
			if (ptr[1] == NULL)
			{
				fprintf(stderr, "SYNTAX:\t. <script>\n");
				member->status = 0x0100;
				continue;
			};
			
			FILE *fp = fopen(ptr[1], "r");
			if (fp == NULL)
			{
				fprintf(stderr, "%s: %s\n", ptr[1], strerror(errno));
				member->status = 0x0100;
				continue;
			};
			
			shSource(fp);
			return 0;
		}
		else if (strcmp(*ptr, "exec") == 0)
		{
			ptr++;
			if (ptr == NULL)
			{
				fprintf(stderr, "USAGE:\texec <command> [args]...\n");
				member->status = 0x0100;
				continue;
			};
			
			char execPath[PATH_MAX];
			if (findExecPath(*ptr, execPath) != 0)
			{
				fprintf(stderr, "%s: command not found\n", *ptr);
				member->status = 0x0100;
				continue;
			};
			
			execve(execPath, ptr, localEnviron.list);
			fprintf(stderr, "exec %s: %s\n", *ptr, strerror(errno));
			member->status = 0x0100;
		}
		else if ((*ptr)[0] == '{')
		{
			if ((*ptr)[strlen(*ptr)-1] != '}')
			{
				fprintf(stderr, "%s: syntax error\n", *ptr);
				member->status = 1;
				continue;
			};
			
			char *cmd = *ptr;
			cmd++;
			cmd[strlen(cmd)-1] = 0;

			shInline(strdup(cmd));
			member->status = shRun();
			shPop();
		}
		else if ((*ptr)[0] == '(')
		{
			if ((*ptr)[strlen(*ptr)-1] != ')')
			{
				fprintf(stderr, "%s: syntax error\n", *ptr);
				member->status = 1;
				continue;
			};

			int pipefd[2];
			if (member->next != NULL)
			{
				pipe(pipefd);
			};
			
			childrenLeft = 1;
			member->pid = fork();
			if (member->pid == 0)
			{
				member->pid = getpid();
				setpgid(0, group->firstMember->pid);
				
				int i;
				for (i=0; i<3; i++)
				{
					if (isatty(i))
					{
						tcsetpgrp(i, group->firstMember->pid);
					};
				};
				
				if (member->prev != NULL)
				{
					close(1);
					dup2(prevInput, 1);
					close(prevInput);
				};
				
				if (member->next != NULL)
				{
					close(pipefd[1]);
					close(0);
					dup2(pipefd[0], 0);
					close(pipefd[0]);
				};
				
				CmdRedir *redir;
				for (redir=member->redir; redir!=NULL; redir=redir->next)
				{	
					if (redir->targetName[0] == '&')
					{
						int fd = 1;
						sscanf(redir->targetName, "&%d", &fd);
						if (fd != redir->fd)
						{
							close(redir->fd);
							dup2(fd, redir->fd);
							
							// we do NOT close the target descriptor in this case
						};
					}
					else
					{
						int fd = open(redir->targetName, redir->oflag, 0644);
						if (fd != redir->fd)
						{
							close(redir->fd);
							dup2(fd, redir->fd);
							close(fd);
						};
					};
				};
				
				char *cmd = *ptr;
				cmd++;
				cmd[strlen(cmd)-1] = 0;
				
				shInline(strdup(cmd));
				shClearStack();
				return shRun();
			}
			else
			{
				if (member->next != NULL)
				{
					// (only then did we create a pipe)
					close(pipefd[0]);
				};
				
				if (member->prev != NULL)
				{
					close(prevInput);
				};
				prevInput = pipefd[1];
			};
		}
		else
		{
			char execPath[PATH_MAX];
			if (findExecPath(*ptr, execPath) != 0)
			{
				fprintf(stderr, "%s: command not found\n", *ptr);
				member->status = 0x0100;
				continue;
			};

			int pipefd[2];
			if (member->next != NULL)
			{
				pipe(pipefd);
			};
			
			childrenLeft = 1;
			member->pid = fork();
			if (member->pid == 0)
			{
				member->pid = getpid();
				setpgid(0, group->firstMember->pid);
				
				int i;
				for (i=0; i<3; i++)
				{
					if (isatty(i))
					{
						tcsetpgrp(i, group->firstMember->pid);
					};
				};
				
				if (member->prev != NULL)
				{
					close(1);
					dup2(prevInput, 1);
					close(prevInput);
				};
				
				if (member->next != NULL)
				{
					close(pipefd[1]);
					close(0);
					dup2(pipefd[0], 0);
					close(pipefd[0]);
				};
				
				CmdRedir *redir;
				for (redir=member->redir; redir!=NULL; redir=redir->next)
				{	
					if (redir->targetName[0] == '&')
					{
						int fd = 1;
						sscanf(redir->targetName, "&%d", &fd);
						if (fd != redir->fd)
						{
							close(redir->fd);
							dup2(fd, redir->fd);
							
							// we do NOT close the target descriptor in this case
						};
					}
					else
					{
						int fd = open(redir->targetName, redir->oflag, 0644);
						if (fd != redir->fd)
						{
							close(redir->fd);
							dup2(fd, redir->fd);
							close(fd);
						};
					};
				};
				
				execve(execPath, ptr, localEnviron.list);
				fprintf(stderr, "%s: cannot exec %s: %s\n", *ptr, execPath, strerror(errno));
				_exit(1);
			}
			else
			{
				if (member->next != NULL)
				{
					// (only then did we create a pipe)
					close(pipefd[0]);
				};
				
				if (member->prev != NULL)
				{
					close(prevInput);
				};
				prevInput = pipefd[1];
			};
		};
	};
	
	while (childrenLeft)
	{
		int status;
		pid_t pid = wait(&status);
		
		childrenLeft = 0;
		for (member=group->firstMember; member!=NULL; member=member->next)
		{
			if (member->pid == pid)
			{
				member->pid = -1;
				member->status = status;
			};
			
			if (member->pid != -1)
			{
				childrenLeft = 1;
			};
		};
	};
	
	int i;
	for (i=0; i<3; i++)
	{
		if (isatty(i))
		{
			tcsetpgrp(i, getpgrp());
		};
	};
	
	return group->firstMember->status;
};

int cmdRun(char *cmd)
{	
	char *strp = cmd;
	char *token;
	
	CmdChain chain;
	CmdGroup *currentGroup = (CmdGroup*) malloc(sizeof(CmdGroup));
	chain.firstGroup = currentGroup;
	
	currentGroup->next = NULL;
	currentGroup->mustSucceed = 0;
	
	CmdMember *currentMember = (CmdMember*) malloc(sizeof(CmdMember));
	currentGroup->firstMember = currentMember;
	
	currentMember->tokens = NULL;
	currentMember->numTokens = 0;
	currentMember->prev = currentMember->next = NULL;
	currentMember->redir = NULL;
	/* status determined by wait() */
	
	CmdRedir *currentRedir;
	
	int isVarSetting = 1;
	int nextRedir = 0;
	int status;
	CmdGroup *group;
	for (token=str_token(&strp, WHITESPACES, "\"'"); token!=NULL; token=str_token(&strp, WHITESPACES, "\"'"))
	{
		if (nextRedir)
		{
			nextRedir = 0;
			currentRedir->targetName = token;
		}
		else if (token[strlen(token)-1] == '>')
		{
			CmdRedir *redir = (CmdRedir*) malloc(sizeof(CmdRedir));
			
			if (currentMember->redir == NULL)
			{
				currentMember->redir = redir;
			}
			else
			{
				CmdRedir *last = currentMember->redir;
				while (last->next != NULL) last = last->next;
				last->next = redir;
			};
			
			currentRedir = redir;
			redir->next = NULL;
			
			if (strlen(token) == 1)
			{
				redir->fd = 1;	/* stdout */
				redir->oflag = O_WRONLY | O_CREAT | O_TRUNC;
			}
			else if (strcmp(token, ">>") == 0)
			{
				redir->fd = 1;	/* stdout */
				redir->oflag = O_WRONLY | O_CREAT | O_APPEND;
			}
			else if (strcmp(&token[strlen(token)-2], ">>") == 0)
			{
				redir->fd = 1;
				sscanf(token, "%d>>", &redir->fd);
				redir->oflag = O_WRONLY | O_CREAT | O_APPEND;
			}
			else
			{
				redir->fd = 1;
				sscanf(token, "%d>", &redir->fd);
				redir->oflag = O_WRONLY | O_CREAT | O_TRUNC;
			};
			
			nextRedir = 1;
		}
		else if (strcmp(token, "|") == 0)
		{
			CmdMember *newMember = (CmdMember*) malloc(sizeof(CmdMember));
			currentMember->prev = newMember;
			newMember->next = currentMember;
			newMember->prev = NULL;

			currentMember->tokens = (char**) realloc(currentMember->tokens,
									sizeof(char*)*(currentMember->numTokens+1));
			currentMember->tokens[currentMember->numTokens] = NULL;
			
			currentMember = newMember;
			currentGroup->firstMember = newMember;
			
			currentMember->tokens = NULL;
			currentMember->numTokens = 0;
			currentMember->redir = NULL;
		}
		else if (strcmp(token, "||") == 0)
		{
			currentMember->tokens = (char**) realloc(currentMember->tokens,
									sizeof(char*)*(currentMember->numTokens+1));
			currentMember->tokens[currentMember->numTokens] = NULL;
			
			currentGroup->mustSucceed = 0;

			currentGroup->next = (CmdGroup*) malloc(sizeof(CmdGroup));
			currentGroup = currentGroup->next;
			currentGroup->next = NULL;

			currentMember = (CmdMember*) malloc(sizeof(CmdMember));
			currentGroup->firstMember = currentMember;
	
			currentMember->tokens = NULL;
			currentMember->numTokens = 0;
			currentMember->prev = currentMember->next = NULL;
			currentMember->redir = NULL;
		}
		else if (strcmp(token, "&&") == 0)
		{
			currentMember->tokens = (char**) realloc(currentMember->tokens,
									sizeof(char*)*(currentMember->numTokens+1));
			currentMember->tokens[currentMember->numTokens] = NULL;
			
			currentGroup->mustSucceed = 1;

			currentGroup->next = (CmdGroup*) malloc(sizeof(CmdGroup));
			currentGroup = currentGroup->next;
			currentGroup->next = NULL;

			currentMember = (CmdMember*) malloc(sizeof(CmdMember));
			currentGroup->firstMember = currentMember;
	
			currentMember->tokens = NULL;
			currentMember->numTokens = 0;
			currentMember->prev = currentMember->next = NULL;
			currentMember->redir = NULL;
		}
		else if (token[0] == '(' && currentMember->numTokens != 0)
		{
			fprintf(stderr, "sh: invalid subshell syntax\n");
			status = 1;
			goto end;
		}
		else if (token[0] == '{' && currentMember->numTokens != 0)
		{
			fprintf(stderr, "sh: invalid subshell syntax\n");
			status = 1;
			goto end;
		}
		else
		{
			int index = currentMember->numTokens++;
			currentMember->tokens = (char**) realloc(currentMember->tokens, sizeof(char*)*currentMember->numTokens);
			currentMember->tokens[index] = token;
			str_canon(token);
		
			if (str_find(token, "=", "\"'") == NULL)
			{
				isVarSetting = 0;
			};
		};
	};
	
	if (nextRedir)
	{
		currentRedir->targetName = "/dev/null";
	};
	
	currentMember->tokens = (char**) realloc(currentMember->tokens, sizeof(char*)*(currentMember->numTokens+1));
	currentMember->tokens[currentMember->numTokens] = NULL;

	if (isVarSetting)
	{
		// setting shell variables only
		char **ptr;
		for (ptr=currentMember->tokens; *ptr!=NULL; ptr++)
		{
			dictPut(&dictShellVars, *ptr);
		};
	}
	else
	{
		// execute each group in the chain
		CmdGroup *group;
		for (group=chain.firstGroup; group!=NULL; group=group->next)
		{
			status = executeGroup(group);
			
			int success = 0;
			if (WIFEXITED(status))
			{
				if (WEXITSTATUS(status) == 0)
				{
					success = 1;
				};
			};
			
			const char *suffix = "";
			if (WCOREDUMP(status)) suffix = " (core dumped)";
			
			if (WIFSIGNALED(status))
			{
				int sig = WTERMSIG(status);
				switch (sig)
				{
				case SIGABRT:
					fprintf(stderr, "Aborted%s\n", suffix);
					break;
				case SIGALRM:
					fprintf(stderr, "Alarm%s\n", suffix);
					break;
				case SIGBUS:
					fprintf(stderr, "Bus error%s\n", suffix);
					break;
				case SIGFPE:
					fprintf(stderr, "Arithmetic error%s\n", suffix);
					break;
				case SIGHUP:
					fprintf(stderr, "Hangup%s\n", suffix);
					break;
				case SIGILL:
					fprintf(stderr, "Illegal instruction%s\n", suffix);
					break;
				case SIGKILL:
					fprintf(stderr, "Killed%s\n", suffix);
					break;
				case SIGPIPE:
					fprintf(stderr, "Broken pipe%s\n", suffix);
					break;
				case SIGSEGV:
					fprintf(stderr, "Invalid memory access%s\n", suffix);
					break;
				case SIGSTOP:
					fprintf(stderr, "Stopped%s\n", suffix);
					break;
				case SIGTERM:
					fprintf(stderr, "Terminated%s\n", suffix);
					break;
				case SIGSYS:
					fprintf(stderr, "Bad system call%s\n", suffix);
					break;
				};
			};
			
			if (success != group->mustSucceed)
			{
				break;
			};
		};
	};

	// delete all data
	end:
	group = chain.firstGroup;
	while (group != NULL)
	{
		CmdGroup *next = group->next;
		
		CmdMember *member = group->firstMember;
		while (member != NULL)
		{
			CmdMember *nextMember = member->next;
			
			free(member->tokens);
			
			CmdRedir *redir = member->redir;
			while (redir != NULL)
			{
				CmdRedir *nextRedir = redir->next;
				
				free(redir);
				redir = nextRedir;
			};
			
			free(member);
			member = nextMember;
		};
		
		free(group);
		group = next;
	};

	if (WIFEXITED(status))
	{
		return WEXITSTATUS(status);
	}
	else if (WIFSIGNALED(status))
	{
		return 128 + WTERMSIG(status);
	}
	else
	{
		return 255;
	};
};
