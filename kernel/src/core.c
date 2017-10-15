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

#include <glidix/core.h>
#include <glidix/vfs.h>
#include <glidix/console.h>
#include <glidix/sched.h>
#include <glidix/memory.h>
#include <glidix/syscall.h>
#include <glidix/string.h>
#include <glidix/pagetab.h>

static off_t dumpPage(uint64_t addr, off_t *offAlloc, CoreSegment *seglist, size_t numSegs, File *fp)
{
	// find the segment containing this address
	size_t i;
	for (i=0; i<numSegs; i++)
	{
		CoreSegment *seg = &seglist[i];
		if (seg->cs_base <= addr && seg->cs_base+seg->cs_size > addr)
		{
			// this is the segment!
			if (seg->cs_offset != 0)
			{
				// page already loaded into the file
				return 0;
			}
			else
			{
				off_t offset = *offAlloc;
				(*offAlloc) += 0x1000;
				
				char buffer[0x1000];
				memcpy_u2k(buffer, (void*) addr, 0x1000);
				
				vfsPWrite(fp, buffer, 0x1000, offset);
				return offset;
			};
		};
	};
	
	return 0;
};

static off_t dumpPT(PT *pt, off_t *offAlloc, CoreSegment *seglist, size_t numSegs, File *fp)
{
	uint64_t entries[512];
	memset(entries, 0, sizeof(entries));
	
	size_t i;
	for (i=0; i<512; i++)
	{
		if (pt->entries[i].present)
		{
			uint64_t pageAddr = ((uint64_t)&pt->entries[i]) << 9;
			entries[i] = dumpPage(pageAddr & 0x0000FFFFFFFFFFFF, offAlloc, seglist, numSegs, fp);
		};
	};
	
	off_t offset = *offAlloc;
	(*offAlloc) += sizeof(entries);
	vfsPWrite(fp, entries, sizeof(entries), offset);
	return offset;
};

static off_t dumpPD(PD *pd, off_t *offAlloc, CoreSegment *seglist, size_t numSegs, File *fp)
{
	uint64_t entries[512];
	memset(entries, 0, sizeof(entries));
	
	size_t i;
	for (i=0; i<512; i++)
	{
		if (pd->entries[i].present)
		{
			uint64_t ptAddr = ((uint64_t)&pd->entries[i]) << 9;
			entries[i] = dumpPT((PT*) ptAddr, offAlloc, seglist, numSegs, fp);
		};
	};
	
	off_t offset = *offAlloc;
	(*offAlloc) += sizeof(entries);
	vfsPWrite(fp, entries, sizeof(entries), offset);
	return offset;
};

static off_t dumpPDPT(PDPT *pdpt, off_t *offAlloc, CoreSegment *seglist, size_t numSegs, File *fp)
{
	uint64_t entries[512];
	memset(entries, 0, sizeof(entries));
	
	size_t i;
	for (i=0; i<512; i++)
	{
		if (pdpt->entries[i].present)
		{
			uint64_t pdAddr = ((uint64_t)&pdpt->entries[i]) << 9;
			entries[i] = dumpPD((PD*) pdAddr, offAlloc, seglist, numSegs, fp);
		};
	};
	
	off_t offset = *offAlloc;
	(*offAlloc) += sizeof(entries);
	vfsPWrite(fp, entries, sizeof(entries), offset);
	return offset;
};

void coredump(siginfo_t *si, Regs *regs)
{
	ProcMem *pm = getCurrentThread()->pm;
	Segment *vmseg;
	
	// make sure that we can be the coredump thread; otherwise go to sleep
	if (spinlockTry(&getCurrentThread()->slCore) != 0)
	{
		cli();
		lockSched();
		waitThread(getCurrentThread());
		switchTaskUnlocked(regs);
	};
	
	// put other threads to sleep then get all the states
	suspendOtherThreads();
	
	size_t numThreads = 1;
	
	cli();
	lockSched();
	
	Thread *thread = getCurrentThread();
	do
	{
		if (thread->creds != NULL)
		{
			if (thread->creds->pid == getCurrentThread()->creds->pid)
			{
				if (thread != getCurrentThread())
				{
					numThreads++;
				};
			};
		};
		thread = thread->next;
	} while (thread != getCurrentThread());
	
	unlockSched();
	sti();
	
	CoreThread *thlist = (CoreThread*) kmalloc(sizeof(CoreThread) * numThreads);
	CoreThread *thput = thlist;
	
	cli();
	lockSched();
	
	thread = getCurrentThread();
	
	// it IS safe to do this when the scheduler is locked. those will be ignored by the scheduler anyway
	// and replaced with our own registers, but it helps us generalize the following loop for all threads.
	memcpy(&thread->regs, regs, sizeof(Regs));
	do
	{
		if (thread->creds != NULL)
		{
			if (thread->creds->pid == getCurrentThread()->creds->pid)
			{
				memcpy(thput->ct_fpu, &thread->fpuRegs, sizeof(FPURegs));
				thput->ct_thid = thread->thid;
				thput->ct_nice = thread->niceVal;
				thput->ct_rax = thread->regs.rax;
				thput->ct_rcx = thread->regs.rcx;
				thput->ct_rdx = thread->regs.rdx;
				thput->ct_rbx = thread->regs.rbx;
				thput->ct_rsp = thread->regs.rsp;
				thput->ct_rbp = thread->regs.rbp;
				thput->ct_rsi = thread->regs.rsi;
				thput->ct_rdi = thread->regs.rdi;
				thput->ct_r8  = thread->regs.r8;
				thput->ct_r9  = thread->regs.r9;
				thput->ct_r10 = thread->regs.r10;
				thput->ct_r11 = thread->regs.r11;
				thput->ct_r12 = thread->regs.r12;
				thput->ct_r13 = thread->regs.r13;
				thput->ct_r14 = thread->regs.r14;
				thput->ct_r15 = thread->regs.r15;
				thput->ct_rip = thread->regs.rip;
				thput->ct_rflags = thread->regs.rflags;
				thput->ct_fsbase = thread->regs.fsbase;
				thput->ct_gsbase = thread->regs.gsbase;
				thput->ct_errnoptr = (uint64_t) thread->errnoptr;
				
				thput++;
			};
		};
		thread = thread->next;
	} while (thread != getCurrentThread());
	
	unlockSched();
	sti();
	
	// open the core file
	int error;
	File *fp = vfsOpen("core", VFS_CHECK_ACCESS | VFS_CREATE | (0600 << 3), &error);
	if (fp == NULL)
	{
		// failed to make a coredump!
		processExit(-si->si_signo);
	};
	
	if (fp->truncate != NULL)
	{
		fp->truncate(fp, 0);
	};
	
	// keep track of the highest-used offset in the file (start by skipping the header)
	off_t nextOffset = sizeof(CoreHeader);
	
	// make the segment list
	size_t numSegs = 0;
	
	semWait(&pm->lock);
	for (vmseg=pm->segs; vmseg!=NULL; vmseg=vmseg->next)
	{
		if (vmseg->flags != 0) numSegs++;
	};
	
	CoreSegment *seglist = (CoreSegment*) kmalloc(sizeof(CoreSegment) * numSegs);
	CoreSegment *segput = seglist;
	
	off_t offSeglist = nextOffset;
	nextOffset += sizeof(CoreSegment) * numSegs;
	
	uint64_t base = 0;
	for (vmseg=pm->segs; vmseg!=NULL; vmseg=vmseg->next)
	{
		if (vmseg->flags != 0)
		{
			segput->cs_base = base;
			segput->cs_size = vmseg->numPages << 12;
			segput->cs_prot = vmseg->prot;
			segput->cs_flags = vmseg->flags;
			
			if (vmseg->flags & MAP_ANON)
			{
				segput->cs_offset = 0;
			}
			else
			{
				segput->cs_offset = nextOffset;
				nextOffset += segput->cs_size;
			};
			
			segput++;
		};
		
		base += (vmseg->numPages << 12);
	};
	
	semSignal(&pm->lock);
	
	// write the segment list
	vfsPWrite(fp, seglist, sizeof(CoreSegment) * numSegs, offSeglist);
	
	// write the segment data into the file where necessary
	size_t i;
	for (i=0; i<numSegs; i++)
	{
		if (seglist[i].cs_offset != 0)
		{
			void *buffer = kmalloc(seglist[i].cs_size);
			memcpy_u2k(buffer, (const void*) seglist[i].cs_base, seglist[i].cs_size);
			vfsPWrite(fp, buffer, seglist[i].cs_size, seglist[i].cs_offset);
			kfree(buffer);
		};
	};
	
	// write the thread list
	off_t offThreads = nextOffset;
	nextOffset += sizeof(CoreThread) * numThreads;
	vfsPWrite(fp, thlist, sizeof(CoreThread) * numThreads, offThreads);
	
	// now that we have a guarantee that nobody will edit our page tables, we can iterate through
	// it and write relevant parts to the core file.
	off_t offPageTree = dumpPDPT((PDPT*) 0xFFFFFFFFFFE00000, &nextOffset, seglist, numSegs, fp);
	
	// finally, the header
	CoreHeader header;
	header.ch_magic = CORE_MAGIC;
	header.ch_size = (uint32_t) sizeof(CoreHeader);
	header.ch_segoff = offSeglist;
	header.ch_segnum = numSegs;
	header.ch_segentsz = sizeof(CoreSegment);
	header.ch_pagetree = offPageTree;
	header.ch_thoff = offThreads;
	header.ch_thnum = numThreads;
	header.ch_thentsz = sizeof(CoreThread);
	header.ch_pid = getCurrentThread()->creds->pid;
	header.ch_sig = si->si_signo;
	header.ch_code = si->si_code;
	header.ch_resv = 0;		// set the reserved field to zero for future compatibility
	header.ch_addr = (uint64_t) si->si_addr;
	header.ch_uid = getCurrentThread()->creds->ruid;
	header.ch_gid = getCurrentThread()->creds->rgid;
	header.ch_euid = getCurrentThread()->creds->euid;
	header.ch_egid = getCurrentThread()->creds->egid;
	vfsPWrite(fp, &header, sizeof(CoreHeader), 0);
	
	// finally, terminate
	kfree(seglist);
	kfree(thlist);
	vfsClose(fp);
	processExit(-si->si_signo);
};
