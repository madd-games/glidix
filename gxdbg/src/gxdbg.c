/*
	Glidix Debugger

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

#include <sys/call.h>
#include <sys/stat.h>
#include <sys/wait.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <signal.h>
#include <errno.h>
#include <pthread.h>
#include <unistd.h>
#include <termios.h>

#include "gxdbg.h"

#define	SPACE_CHARS		" \t"

typedef struct CmdHistory_ 
{
	char *line;
	struct CmdHistory_ *next;
	struct CmdHistory_ *prev;
} CmdHistory;

CmdHistory *cmdHead;
volatile int traceReason = -1;
volatile pthread_t mainThread;
volatile int trapValue;

void on_signal(int signo, siginfo_t *si, void *context)
{
	if (signo == SIGTRACE)
	{
		traceReason = si->si_code;
		trapValue = si->si_value.sival_int;
	
		if (mainThread == 0)
		{
			mainThread = si->si_pid;
		};
	};
};

void printReason()
{
	switch (traceReason)
	{
	case TR_EXEC:
		printf("Child performed 'exec'\n");
		break;
	case TR_EXIT:
		printf("Terminated\n");
		break;
	case TR_SIGNAL:
		printf("Child caught signal %s, %s\n", sys_signame[trapValue], sys_siglist[trapValue]);
		break;
	};
};

char* prompt()
{
	printf("gxdbg> ");
	
	struct termios tc;
	tcgetattr(0, &tc);
	struct termios old_tc = tc;
	tc.c_lflag &= ~(ECHO);
	tc.c_lflag &= ~ICANON;
	tcsetattr(0, TCSANOW, &tc);
	
	char linbuff [1024];
	char pendbuff [1024];
	char c;
	size_t numChars = 0;
	CmdHistory *currCmd = NULL;
	
	while(1)
	{	
		ssize_t retval = read(0, &c, 1);
		
		if(retval == -1)
		{
			if (errno == EINTR)
			{
				printf("\n");
				return strdup("");
			}
			else return NULL;
		}
		
		if(retval == 0) return NULL;
		else
		{
			if(c == '\n')
			{
				if(numChars != 0)
				{
					CmdHistory *cmd = (CmdHistory*) malloc(sizeof(CmdHistory));
					cmd->prev = cmdHead;
					cmd->next = NULL;
					if (cmdHead != NULL) cmdHead->next = cmd;
					cmdHead = cmd;
					linbuff[numChars] = 0;
					cmd->line = strdup(linbuff);
				}
				linbuff[numChars] = 0;
				tcsetattr(0, TCSANOW, &old_tc);
				return strdup(linbuff);
			}
			else if((uint8_t)c == 0x8B)
			{
				if((currCmd == NULL) && (cmdHead != NULL))
				{
					currCmd = cmdHead;
					linbuff[numChars] = 0;
					strcpy(pendbuff, linbuff);
					while (numChars--) printf("\b");
					numChars = strlen(currCmd->line);
					strcpy(linbuff, currCmd->line);
					printf("%s", linbuff);
				}
				else if(currCmd != NULL)
				{
					if(currCmd->prev != NULL)
					{
						currCmd = currCmd->prev;
						while (numChars--) printf("\b");
						numChars = strlen(currCmd->line);
						strcpy(linbuff, currCmd->line);
						printf("%s", linbuff);
					}
				}
			}
			else if((uint8_t)c == 0x8C)
			{
				if(currCmd != NULL)
				{
					currCmd = currCmd->next;
					while (numChars--) printf("\b");
					if(currCmd == NULL)
					{
						strcpy(linbuff, pendbuff);
						numChars = strlen(linbuff);
						printf("%s", linbuff);
					}
					else
					{
						numChars = strlen(currCmd->line);
						strcpy(linbuff, currCmd->line);
						printf("%s", linbuff);
					}
				}
			}
			else if(c == '\b')
			{
				if(numChars > 0)
				{
					numChars--;
					printf("\b");
				}
			}
			else if(numChars < 1023) 
			{
				linbuff[numChars++] = c;
				write(1, &c, 1);
			} 
		}
	}
};

int yesno()
{
	struct termios tc;
	tcgetattr(0, &tc);
	struct termios old_tc = tc;
	tc.c_lflag &= ~(ECHO | ECHONL);
	tc.c_lflag &= ~ICANON;
	tcsetattr(0, TCSANOW, &tc);
	
	char c;
	while (1)
	{
		read(0, &c, 1);
		
		if (c == 'y') break;
		if (c == 'n') break;
	};
	
	tcsetattr(0, TCSANOW, &old_tc);
	write(1, "\n", 1);
	return c == 'y';
};

void printMState(MachineState *mstate)
{
	printf("%%rdi: 0x%016lX  %%rsi: 0x%016lX\n", mstate->rdi, mstate->rsi);
	printf("%%rax: 0x%016lX  %%rbx: 0x%016lX\n", mstate->rax, mstate->rbx);
	printf("%%rcx: 0x%016lX  %%rdx: 0x%016lX\n", mstate->rcx, mstate->rdx);
	printf("%%r8:  0x%016lX  %%r9:  0x%016lX\n", mstate->r8,  mstate->r9);
	printf("%%r10: 0x%016lX  %%r11: 0x%016lX\n", mstate->r10, mstate->r11);
	printf("%%r12: 0x%016lX  %%r13: 0x%016lX\n", mstate->r12, mstate->r13);
	printf("%%r14: 0x%016lX  %%r15: 0x%016lX\n", mstate->r14, mstate->r15);
	printf("%%rsp: 0x%016lX  %%rbp: 0x%016lX\n", mstate->rsp, mstate->rbp);
	printf("%%fs:  0x%016lX  %%gs:  0x%016lX\n", mstate->fsbase, mstate->gsbase);
	printf("%%rip: 0x%016lX\n", mstate->rip);
	printf("%%rflags: 0x%016lX\n", mstate->rflags);
};

int main(int argc, char *argv[])
{
	if (argc < 2)
	{
		fprintf(stderr, "USAGE:\t%s <executable> <arguments...>\n", argv[0]);
		fprintf(stderr, "\tDebug an executable.\n");
		return 1;
	};
	
	if (__syscall(__SYS_trace, TC_DEBUG, -1, NULL) != 0)
	{
		fprintf(stderr, "%s: cannot enter debug mode: %s\n", argv[0], strerror(errno));
		return 1;
	};
	
	struct sigaction sa;
	memset(&sa, 0, sizeof(struct sigaction));
	sa.sa_sigaction = on_signal;
	sa.sa_flags = SA_SIGINFO;
	if (sigaction(SIGTRACE, &sa, NULL) != 0)
	{
		fprintf(stderr, "%s: cannot set sigaction for SIGTRACE: %s\n", argv[0], strerror(errno));
		return 1;
	};

	memset(&sa, 0, sizeof(struct sigaction));
	sa.sa_sigaction = on_signal;
	sa.sa_flags = SA_SIGINFO;
	if (sigaction(SIGCHLD, &sa, NULL) != 0)
	{
		fprintf(stderr, "%s: cannot set sigaction for SIGCHLD: %s\n", argv[0], strerror(errno));
		return 1;
	};
	
	MachineState mstate __attribute__ ((aligned(16)));
	pid_t pid = fork();
	if (pid == -1)
	{
		fprintf(stderr, "%s: cannot fork: %s\n", argv[0], strerror(errno));
		return 1;
	};
	
	if (pid == 0)
	{
		execv(argv[1], &argv[1]);
		fprintf(stderr, "exec %s: %s\n", argv[1], strerror(errno));
		_exit(1);
	};
	
	while (1)
	{
		pause();
		if (traceReason != -1)
		{
			printReason();
			if (traceReason == TR_EXIT)
			{
				traceReason = -1;
				continue;
			};
			
			traceReason = -1;
		}
		else
		{
			int status;
			if (waitpid(pid, &status, WNOHANG) == pid)
			{
				if (WIFEXITED(status))
				{
					printf("Normal exit with status %d\n", WEXITSTATUS(status));
					return 0;
				}
				else
				{
					int signum = WTERMSIG(status);
					printf("Terminated by signal %s, %s\n", sys_signame[signum], strsignal(signum));
					return 0;
				};
			};
			
			continue;
		};
		
		char *cmdline = NULL;
		while (1)
		{
			free(cmdline);
			
			char *cmdline = prompt();
			char *cmd = strtok(cmdline, SPACE_CHARS);
			if (cmd == NULL) continue;
		
			if (strcmp(cmd, "regs") == 0)
			{
				if (__syscall(__SYS_trace, TC_GETREGS, mainThread, &mstate) != 0)
				{
					fprintf(stderr, "cannot get registers: %s\n", strerror(errno));
					continue;
				};
				
				printMState(&mstate);
			}
			else if ((strcmp(cmd, "exit") == 0) || (strcmp(cmd, "quit") == 0))
			{
				char pidpath[256];
				sprintf(pidpath, "/proc/%d", pid);
				struct stat st;
				
				if (stat(pidpath, &st) == 0)
				{
					printf("Child (pid %d) will be terminated. Exit anyway? (y/n) ", pid);
					if (yesno())
					{
						kill(pid, SIGKILL);
					}
					else
					{
						continue;
					};
				};
				
				return 0;
			}
			else if (strcmp(cmd, "cont") == 0)
			{
				if (__syscall(__SYS_trace, TC_CONT, mainThread) != 0)
				{
					fprintf(stderr, "cannot continue: %s\n", strerror(errno));
					continue;
				}
				else
				{
					break;
				};
			}
			else
			{
				fprintf(stderr, "unknown command '%s'\n", cmd);
				continue;
			};
		};
		
		free(cmdline);
	};
	
	return 0;
};
