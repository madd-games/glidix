/*
	Glidix kernel

	Copyright (c) 2014, Madd Games.
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

#ifndef __glidix_sched_h
#define __glidix_sched_h

/**
 * The thread scheduler.
 */

#include <glidix/common.h>
#include <glidix/procmem.h>
#include <glidix/ftab.h>
#include <glidix/signal.h>
#include <stdint.h>
#include <stddef.h>

typedef struct
{
	uint64_t	rdi, rsi, rbp, rbx, rdx, rcx, rax;
	uint64_t	r8, r9, r10, r11, r12, r13, r14, r15;
	uint64_t	rip, rsp;
	int		therrno;
} PACKED MachineState;

#define	DEFAULT_STACK_SIZE		0x200000
#define	CLONE_SHARE_MEMORY		(1 << 0)
#define	CLONE_SHARE_FTAB		(1 << 1)

/**
 * If this flag is set, then the thread is waiting for a signal to be received.
 */
#define	THREAD_WAITING			(1 << 0)

/**
 * If this flag is set, the thread is currently handling a signal.
 */
#define	THREAD_SIGNALLED		(1 << 1)

/**
 * If this flag is set, then the thread has terminated and is a zombie process,
 * and must be removed by a waitpid().
 */
#define	THREAD_TERMINATED		(1 << 2)

/**
 * If this flag is set, then the thread is in the middle of an interruptable system call.
 */
#define	THREAD_INT_SYSCALL		(1 << 3)

/**
 * A bitwise-OR of all flags which stop the process from being scheduled.
 */
#define	THREAD_NOSCHED			(THREAD_WAITING | THREAD_TERMINATED)

/**
 * Flags for pollThread().
 */
#define	WNOHANG				(1 << 0)

typedef struct _Thread
{
	/**
	 * The thread's registers.
	 */
	Regs				regs;

	/**
	 * The thread's stack and its size. Must be kfree()'d when the thread terminates.
	 */
	void				*stack;
	size_t				stackSize;

	/**
	 * Thread name (for debugging).
	 */
	const char			*name;

	/**
	 * Flags (see above).
	 */
	uint64_t			flags;

	/**
	 * The ProcMem object attached to this thread, or NULL if no process memory is
	 * assigned (i.e. it's a kernel thread).
	 */
	ProcMem				*pm;

	/**
	 * The pid is 0 for all kernel threads. New pids are assigned by _glidix_clone() and fork().
	 */
	int				pid;

	/**
	 * The parent's pid. This value must be ignored for kernel threads (which have pid 0). If
	 * a process dies, its children's parent pid shall be set to 1.
	 */
	int				pidParent;

	/**
	 * The file table used by this process.
	 */
	FileTable			*ftab;

	/**
	 * UID/GID stuff.
	 */
	uid_t				euid, suid, ruid;
	gid_t				egid, sgid, rgid;

	/**
	 * This RIP is jumped to when a signal is caught and shall be a C function with the prototype:
	 * void rootSigHandler(void *retptr, singinfo_t *siginfo);
	 * It must never return! Instead, it shall call _glidix_sysret() to do the returning.
	 * The retptr argument must be passed on to _glidix_sysret().
	 */
	uint64_t			rootSigHandler;

	/**
	 * This thread's signal queue.
	 */
	SignalQueue			*sigq;

	/**
	 * Points to this thread's argv and initial environment string.
	 * The format of this string is as follows:
	 * First, there is a list of command-line arguments, separated by NUL chars.
	 * This is terminated by a double-NUL. Then, there is a list of var=value pairs,
	 * separated by NUL chars, finally terminated by a double-NUL.
	 */
	char				*execPars;
	size_t				szExecPars;

	/**
	 * Current error.
	 */
	int				therrno;

	/**
	 * Process exit status (only valid if this is a zombie process).
	 */
	int				status;

	/**
	 * Registers to restore if the current system call is interrupted by a signal;
	 * This is only valid if the THREAD_INT_SYSCALL flag is said.
	 */
	Regs				intSyscallRegs;

	/**
	 * Previous and next thread. Threads are stored in a circular list; this is never NULL.
	 */
	struct _Thread			*prev;
	struct _Thread			*next;
} Thread;

/**
 * We need this to switch kernel stacks.
 */
typedef struct
{
	uint32_t ignore;
	uint64_t rsp0;
} PACKED TSS;

extern TSS _tss;

void initSched();
void switchContext(Regs *regs);
void dumpRunqueue();
void switchTask(Regs *regs);

/**
 * Prototype for a kernel thread entry point.
 */
typedef void (*KernelThreadEntry)(void *data);

/**
 * Parameters for a new kernel thread (passed to CreateKernelThread() ).
 */
typedef struct
{
	size_t				stackSize;
	const char			*name;
} KernelThreadParams;

/**
 * Create a new kernel thread.
 * @param entry Thread entry point.
 * @param params Thread initialization params (may be NULL).
 * @param data An arbitrary pointer to pass to the thread.
 */
void CreateKernelThread(KernelThreadEntry entry, KernelThreadParams *params, void *data);

/**
 * Return the current thread on this CPU.
 */
Thread *getCurrentThread();

/**
 * Mark a thread as waiting.
 */
void waitThread(Thread *thread);

/**
 * Signal a thread to stop waiting.
 */
void signalThread(Thread *thread);

/**
 * This function is used to create new user threads and processes, it can be used to implement
 * pthread_create() as well as fork(), by passing the appropriate options.
 * The passed regs structure is for the parent.
 * This function returns 0 on success, or negative numbers on error.
 */
int threadClone(Regs *regs, int flags, MachineState *state);

/**
 * Exit the thread.
 */
void threadExit(Thread *thread, int status);

/**
 * Poll a thread.
 */
int pollThread(Regs *regs, int pid, int *stat_loc, int flags);

/**
 * Send a signal to a specific pid.
 */
int signalPid(int pid, int signal);

#endif
