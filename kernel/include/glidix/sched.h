/*
	Glidix kernel

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

#ifndef __glidix_sched_h
#define __glidix_sched_h

/**
 * The thread scheduler.
 */

#include <glidix/common.h>
#include <glidix/procmem.h>
#include <glidix/ftab.h>
#include <glidix/signal.h>
#include <glidix/vfs.h>
#include <glidix/fpu.h>
#include <glidix/time.h>
#include <stdint.h>
#include <stddef.h>

#define	DEFAULT_STACK_SIZE		0x200000
#define	CLONE_THREAD			(1 << 0)
#define	CLONE_DETACHED			(1 << 1)

/**
 * Standard priorities.
 */
#define	NICE_NORMAL			0		/* normal priority */
#define	NICE_NETRECV			-2		/* network receiver */
#define	NICE_UIN			-4		/* user input handlers */

/**
 * Executable permissions.
 */
#define	XP_RAWSOCK			(1 << 0)
#define	XP_NETCONF			(1 << 1)
#define	XP_MODULE			(1 << 2)
#define	XP_MOUNT			(1 << 3)
#define	XP_CHXPERM			(1 << 4)
#define	XP_NICE				(1 << 5)
#define	XP_DISPCONF			(1 << 6)

#define	XP_ALL				0x7FFFFFFFFFFFFFFF
#define	XP_NCHG				0xFFFFFFFFFFFFFFFF

/**
 * If this flag is set, then the thread is waiting for a signal to be received.
 */
#define	THREAD_WAITING			(1 << 0)

/**
 * If this flag is set, then the thread has terminated and is a zombie process,
 * and must be removed by a waitpid().
 */
#define	THREAD_TERMINATED		(1 << 2)

/**
 * If this flag is set, the thread is detached and shall not produce a scheduler notification
 * when it dies.
 */
#define	THREAD_DETACHED			(1 << 3)

/**
 * If this flag is set, then the thread is a "rebel" and cannot be controlled by its parent.
 * This happens due to suid/sgid attributes when exec() is called.
 */
#define	THREAD_REBEL			(1 << 4)

/**
 * If this flag is set, the thread is traced (stopped for debugging purposes). Rebels are never
 * traced.
 */
#define	THREAD_TRACED			(1 << 5)

/**
 * A bitwise-OR of all flags which stop the process from being scheduled.
 */
#define	THREAD_NOSCHED			(THREAD_WAITING | THREAD_TERMINATED | THREAD_TRACED)

/**
 * Flags for signalPidEx().
 */
#define	SP_NOPERM			(1 << 0)		/* no permission checking */

/**
 * Flags for pollThread().
 */
#define	WNOHANG				(1 << 0)
#define	WDETACH				(1 << 1)
#define	WUNTRACED			(1 << 2)
#define	WCONTINUED			(1 << 3)

/**
 * Debug flags.
 */
#define	DBG_STOP_ON_EXEC		(1 << 0)
#define	DBG_DEBUGGER			(1 << 1)		/* i am the debugger, set stop-on-exec in children */
#define	DBG_DEBUG_MODE			(1 << 2)		/* i am being debugged */
#define DBG_SIGNALS			(1 << 3)		/* trap when dispatching signals */

/**
 * Number of priority queues. Valid nice values, 'n', are:
 * -(NUM_PRIO_Q/2) < n < (NUM_PRIO_Q/2)
 */
#define	NUM_PRIO_Q			16

typedef struct
{
	FPURegs		fpuRegs;
	uint64_t	rflags;
	uint64_t	rip;
	uint64_t	rdi, rsi, rbp, rbx, rdx, rcx, rax;
	uint64_t	r8, r9, r10, r11, r12, r13, r14, r15;
	uint64_t	rsp;
	uint64_t	fsbase, gsbase;
} MachineState;

/**
 * Describes the state of a running process. Used by _glidix_procstat().
 */
typedef struct
{
	/**
	 * Number of ticks used in total.
	 */
	uint64_t			ps_ticks;
	
	/**
	 * Number of times this process was scheduled.
	 */
	uint64_t			ps_entries;
	
	/**
	 * Number of ticks in the maximum quantum (quantumTicks).
	 */
	uint64_t			ps_quantum;
} ProcStat;

/**
 * Scheduler notification. This data structure is stored in a doubly-linked list
 * and is used to leave information that may be obtained using wait(), waitpid(),
 * pthread_join(), etc.
 *
 * For SHN_PROCESS_DEAD, the "source" is the pid of the child process that died, and
 * "dest" is its parent pid. The parent may collect the status from the "info" union
 * using wait() or waitpid(). All threads of the parent process are woken up when the
 * event is posted in case they are performing a blocking wait()/waitpid().
 *
 * For SHN_THREAD_DEAD, the "source" is the thid of the thread that died, and "dest"
 * is the pid. Any other thread in the process may call pthread_join() to collect the
 * return value from the "info" union. All other threads are woken up in case one is
 * performing a blocking pthread_join().
 *
 * In any case, "dest" is the pid of the process which is supposed to receive the
 * notification. When the process with that pid dies, all notifications directed to
 * it are deleted.
 */
#define	SHN_PROCESS_DEAD		0
#define	SHN_THREAD_DEAD			1
typedef struct SchedNotif_
{
	int				type;
	int				dest;
	int				source;
	
	union
	{
		int			status;
		uint64_t		retval;
	} info;
	
	struct SchedNotif_*		prev;
	struct SchedNotif_*		next;
} SchedNotif;

/**
 * Thread credentials. This object is shared by all threads that constitue a "process".
 * When a new thread is created within the same process, they use the same credentials
 * object.
 * When fork() is called to create a new process, the initial thread has a COPY of the
 * calling thread's credentials object, but it is now a new object and changes made by
 * the 2 processes are independent of each other (furthermore, the pid is changed for
 * the new process).
 * Kernel threads do not have credentials.
 */
typedef struct
{
	/**
	 * Reference count.
	 */
	int				refcount;
	
	/**
	 * Process ID.
	 */
	int				pid;
	
	/**
	 * Parent process ID.
	 */
	int				ppid;

	/**
	 * UID/GID stuff.
	 */
	uid_t				euid, suid, ruid;
	gid_t				egid, sgid, rgid;

	/**
	 * Session and process group IDs.
	 */
	int				sid;
	int				pgid;

	/**
	 * File mode creation mask as set by umask().
	 */
	mode_t				umask;

	/**
	 * Supplementary group IDs (max 16) and the number that is actually set.
	 */
	gid_t				groups[16];
	int				numGroups;
	
	/**
	 * Exit status to be sent to the parent of the process when all the threads
	 * have terminated. This is set to zero initially; this way, if all threads
	 * terminate by calling pthread_exit(), it will be as if the entire process
	 * exited with exit() with a status of 0. Otherwise, if a thread calls exit(),
	 * all threads get terminated and this is set to the status specified in the
	 * exit() call.
	 */
	int				status;
	
	/**
	 * Process-wide statistics.
	 */
	ProcStat			ps;
} Creds;

Creds*	credsNew();
Creds*	credsDup(Creds *creds);
void	credsUpref(Creds *creds);
void	credsDownref(Creds *creds);

/**
 * Represents a runqueue entry. An entry is pre-allocated within the Thread structure, and they
 * are linked by the "next" field into a queue, from which the scheduler gets next commands to
 * run.
 */
struct _Thread;
typedef struct _RunqueueEntry
{
	struct _Thread*			thread;
	struct _RunqueueEntry*		next;
} RunqueueEntry;

/**
 * WARNING: The first few fields of this structure must not be reordered as they are accessed by
 * assembly code. The end of this "assembly region" is marked with a comment.
 */
typedef struct _Thread
{
	/**
	 * The thread's FPU registers. This must be the first member of this structure
	 * so that they are 16-byte-aligned without any waste.
	 */
	FPURegs				fpuRegs;				// 0

	/**
	 * The value to load into RSP upon a syscall.
	 */
	uint64_t			syscallStackPointer;			// 0x200
	
	/**
	 * Registers that must be preserved across system calls. Those are stored
	 * by the system call dispatcher and may be read by functions like fork()
	 * which need to know the return state.
	 */
	uint64_t			urbx;					// 0x208
	uint64_t			ursp;					// 0x210
	uint64_t			urbp;					// 0x218
	uint64_t			ur12;					// 0x220
	uint64_t			ur13;					// 0x228
	uint64_t			ur14;					// 0x230
	uint64_t			ur15;					// 0x238
	uint64_t			urip;					// 0x240

	/**
	 * Current error. Set to 0 by _syscall_entry(), and only stored at userspace
	 * errno address if set to nonzero.
	 */
	int				therrno;				// 0x248

	/**
	 * The 8 registers: RBX, RSP, RBP, R12, R13, R14, R15, RIP, preserved for
	 * exception catching.
	 * If RIP != 0, then we are catching exceptions.
	 */
	uint64_t			catchRegs[8];				// 0x250

	/**
	 * Debug flags.
	 */
	int				debugFlags;				// 0x258
	
	// --- END OF ASSEMBLY REGION --- //
	
	/**
	 * The thread's stack and its size. Must be kfree()'d when the thread terminates.
	 */
	void				*stack;
	size_t				stackSize;
	
	/**
	 * The thread's registers.
	 */
	Regs				regs;

	/**
	 * Thread name (for debugging of kernel threads), or the path to the executable in
	 * any other case.
	 */
	char				name[256];

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
	 * The file table used by this process.
	 */
	FileTable			*ftab;
	
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
	 * Process exit status (only valid if this is a zombie process).
	 */
	int				status;

	/**
	 * The current working directory. Does not end with a slash unless it's the root directory.
	 */
	char				cwd[256];

	/**
	 * Pointer to where the errno shall be stored after syscalls complete.
	 */
	int				*errnoptr;
	
	/**
	 * The time at which to send the SIGALRM signal; 0 means the signal shall not be sent.
	 * Given in nanoseconds.
	 */
	uint64_t			alarmTime;
	
	/**
	 * The TimedEvent representing the wakeup time.
	 */
	TimedEvent			alarmTimer;
	
	/**
	 * The thread's signal disposition list.
	 */
	SigDisp				*sigdisp;
	
	/**
	 * This thread's pending signal list. The list is unordered.
	 * The bits in pendingSet indicate which entries are filled in.
	 */
	uint64_t			pendingSet;
	siginfo_t			pendingSigs[64];
	
	/**
	 * The thread's credentials. NULL for kernel threads.
	 */
	Creds*				creds;
	
	/**
	 * The thread ID, globally unique.
	 */
	int				thid;
	
	/**
	 * The thread's signal mask.
	 */
	uint64_t			sigmask;
	
	/**
	 * Number of times the thread was woken up while it wasn't sleeping.
	 */
	int				wakeCounter;
	
	/**
	 * The own, and delegatable permissions of this thread.
	 */
	uint64_t			oxperm;
	uint64_t			dxperm;
	
	/**
	 * Runqueue entry structure. Linked/unlinked from the runqueue.
	 */
	RunqueueEntry			runq;
	
	/**
	 * Nice value of the thread.
	 */
	int				niceVal;
	
	/**
	 * Thread-local statistics.
	 */
	ProcStat			ps;
	
	/**
	 * The physical address on which this thread is blocking. When sys_unblock() is called on
	 * an address which maps to this, the thread will be woken up.
	 */
	uint64_t			blockPhys;
	
	/**
	 * Spinlock for controlling coredumps. Only one thread will ever acquire this lock, and
	 * it guarantees that there aren't multiple threads coredumping all at once.
	 */
	Spinlock			slCore;
	
	/**
	 * Previous and next thread. Threads are stored in a circular list; this is never NULL.
	 */
	struct _Thread			*prev;
	struct _Thread			*next;
} Thread;

typedef struct
{
	/**
	 * This is currently ignored by the kernel.
	 */
	int	scope;
	
	/**
	 * Detach state.
	 */
	int	detachstate;
	
	/**
	 * Scheduler inheritance mode; actually ignored.
	 */
	int	inheritsched;
	
	/**
	 * Stack position and size.
	 */
	void*	stack;
	size_t	stacksize;
	
	/**
	 * Make sure we are padded to at least 256 bytes.
	 */
	char	pad[256];
} ThreadAttr;

void initSched();
void initSched2();
void initSchedAP();				// initialize scheduling on an AP, when the main sched is already inited
void switchContext(Regs *regs);
void dumpRunqueue();
void switchTask(Regs *regs);
void switchTaskUnlocked(Regs *regs);
int haveReadySigs(Thread *thread);
int wasSignalled();				// returns 1 if the current thread has signals ready

void lockSched();				// only call with IF=0!
int isSchedLocked();
void unlockSched();

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
	int				flags;
} KernelThreadParams;

/**
 * Create a new kernel thread.
 * @param entry Thread entry point.
 * @param params Thread initialization params (may be NULL).
 * @param data An arbitrary pointer to pass to the thread.
 * @return The thread handle to later pass to ReleaseKernelThread().
 */
Thread* CreateKernelThread(KernelThreadEntry entry, KernelThreadParams *params, void *data);

/**
 * Release a kernel thread. This must be called by the same thread that called CreateKernelThread()
 * to create it in the first place. This function will busy-wait until the thread terminates!
 */
void ReleaseKernelThread(Thread *thread);

/**
 * Return the current thread on this CPU.
 */
Thread *getCurrentThread();

/**
 * Mark a thread as waiting.
 */
void waitThread(Thread *thread);

/**
 * Signal a thread to stop waiting. Returns 1 if preemption is required (a higher-priority thread has
 * been awoken).
 */
int signalThread(Thread *thread);

/**
 * This function is used to create new user threads and processes, it can be used to implement
 * pthread_create() as well as fork(), by passing the appropriate options.
 *
 * regs = the thread's initial registers
 * flags = 
 *	CLONE_THREAD -		create a thread within the same process; otherwise a new process with
 *				an initial thread.
 *	CLONE_DETACHED -	create a detached thread.
 * state = use this when you need to set FPU registers basically. throwback to when this was a system
 *         call.
 */
int threadClone(Regs *regs, int flags, MachineState *state);

/**
 * Exit the thread.
 */
void threadExitEx(uint64_t retval);
void threadExit();

/**
 * Exit the entire process.
 */
void processExit(int status);

/**
 * Wait for a process to terminate.
 */
int processWait(int pid, int *stat_loc, int flags);

/**
 * Send a signal to a specific pid.
 */
int signalPidEx(int pid, siginfo_t *si, int flags);
int signalPid(int pid, int signal);

/**
 * Switch to the process with specified index. This is only for debugging purposes and can be used from debugging
 * mode only.
 */
void switchTaskToIndex(int index);

/**
 * Return a thread by pid of thid. The scheduler must be locked when calling this function. Requesting a thread by
 * pid will return an unspecified thread from the given process.
 * Return NULL if not found.
 */
Thread *getThreadByPID(int pid);
Thread *getThreadByTHID(int thid);

/**
 * Wake up all threads in a process
 */
void wakeProcess(int pid);

/**
 * Suspend all threads except the calling thread, by sending the SIGTHSUSP signal.
 */
void suspendOtherThreads();

/**
 * Kill all threads of the current process except the calling thread, by sending the SIGTHKILL signal, discard all inbound
 * scheduler notifications, and wait for total cleanup of the threads, making the calling thread the sole thread in the
 * process.
 */
void killOtherThreads();

/**
 * Join a thread. Returns 0 on success, -1 on failure (EDEADLK), or -2 if it was interrupted by a signal,
 * in which case the reset procedure must occur.
 */
int joinThread(int thid, uint64_t *retval);

/**
 * Detach a thread. If the already died while joinable, its return status is also removed from the notification list.
 */
int detachThread(int thid);

/**
 * Send a signal to a thread in the current process. All signals may be sent (unlike signalPid()).
 */
int signalThid(int thid, int sig);

/**
 * Returns nonzero if the calling thread has the specified permission.
 */
int havePerm(uint64_t xperm);

/**
 * Set the relative nice value of the calling thread.
 */
int thnice(int incr);

/**
 * Map a different frame into the "temporary page", and return the previous frame number. You MUST map the old
 * page number in before the calling function returns. Most importantly, since the temporary page is at the
 * top of the kernel stack, and that area is used by signal dispatching, it MUST be returned to normal before
 * returning to userspace.
 */
uint64_t mapTempFrame(uint64_t frame);

/**
 * Returns a virtual pointer which may be used to access the "temporary page".
 */
void* tmpframe();

/**
 * Send a debug signal to my parent and go into trace mode with the specified registers. It looks a scheduler, unlike traceTrapEx()
 */
void traceTrap(Regs *regs, int reason);

/**
 * Send a debug signal to my parent and go into trace mode with the specified registers. The "value" is a reason-dependent extra information field. It doesn't lock the scheduler, unlike traceTrap()
 */
void traceTrapEx(Regs *regs, int reason, int value);

/**
 * Called by a kernel thread to detach itself; meaning that it will not be necessary to call ReleaseKernelThread() on it
 * to deallocate its resources.
 */
void detachMe();

#endif
