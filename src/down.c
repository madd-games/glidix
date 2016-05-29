/*
	Glidix kernel

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

#include <glidix/down.h>
#include <glidix/sched.h>
#include <glidix/signal.h>
#include <glidix/module.h>
#include <glidix/common.h>
#include <glidix/console.h>
#include <glidix/time.h>
#include <glidix/errno.h>
#include <glidix/mount.h>
#include <glidix/idt.h>
#include <glidix/acpi.h>
#include <glidix/port.h>
#include <glidix/cpu.h>

static const char *actionNames[] = {
	"POWER OFF",
	"REBOOT",
	"HALT"
};

void reboot()
{
	ASM("cli");
	uint8_t good = 0x02;
	while (good & 0x02)
		good = inb(0x64);
	outb(0x64, 0xFE);
	while (1) ASM("hlt");
}

int systemDown(int action)
{
	Thread *ct = getCurrentThread();
	if (ct->creds->pid != 1)
	{
		ct->therrno = EPERM;
		return -1;
	};

	if ((action < 0) || (action > 2))
	{
		ct->therrno = EINVAL;
		return -1;
	};

	kprintf("Asking any remaining processes to terminate...\n");

	siginfo_t termsig;
	termsig.si_signo = SIGTERM;

	cli();
	lockSched();
	kernelStatus = KERNEL_STOPPING;

	Thread *thread = ct->next;
	while (thread != ct)
	{
		if (thread->creds != NULL)
		{
			if (thread->creds->pid > 1) sendSignal(thread, &termsig);
		};
		thread = thread->next;
	};

	unlockSched();
	sti();

	// after 10 seconds, force all processes to terminate!
	int timeToForce = getUptime() + 10000;

	int allStopped = 0;
	while (!allStopped)
	{
		allStopped = 1;

		thread = ct->next;
		while (thread != ct)
		{
			if (thread->creds != NULL)
			{
				if (thread->creds->pid > 1)
				{
					if ((thread->flags & THREAD_TERMINATED) == 0) allStopped = 0;
				};
			};

			thread = thread->next;
		};

		if (getUptime() > timeToForce)
		{
			kprintf("Stopping remaining processes by force!\n");
			break;
		};
	};

	// mark all processes as terminated.
	cli();
	lockSched();

	thread = ct->next;
	while (thread != ct)
	{
		if (thread->creds != NULL)
		{
			if (thread->creds->pid > 1) thread->flags |= THREAD_TERMINATED;
		};
		thread = thread->next;
	};

	unlockSched();
	sti();

	// don't bother releasing memory because the system is going down.
	thread = ct->next;
	while (thread != ct)
	{
		if (thread->creds != NULL)
		{
			if (thread->creds->pid != 0)
			{
				DownrefProcessMemory(thread->pm);
				if (thread->ftab != NULL) ftabDownref(thread->ftab);
			};
		};

		thread = thread->next;
	};

	kprintf("Cleaning up the init process...\n");
	DownrefProcessMemory(ct->pm);
	ftabDownref(ct->ftab);

	kprintf("Unmounting remaining filesystems...\n");
	unmountAll();

	kprintf("Shutting down kernel modules...\n");
	rmmodAll();

	kprintf("Kernel going down for %s\n", actionNames[action]);
	haltAllCPU();
	
	switch (action)
	{
	case DOWN_POWEROFF:
		AcpiEnterSleepStatePrep(5);
		cli();					// AcpiEnterSleepState() must be called with interrupts disabled
		AcpiEnterSleepState(5);
		// in case we didn't succeed with the poweroff, panic
		panic("power off!");
	case DOWN_REBOOT:
		reboot();
	case DOWN_HALT:
		panic("halt");
	};

	return 0;
};
