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

#ifndef __glidix_cpu_h
#define __glidix_cpu_h

#include <glidix/common.h>
#include <glidix/spinlock.h>

/**
 * This structure describes a CPU.
 */
typedef struct
{
	/**
	 * Glidix-assigned CPU ID. This is the index into the CPU array that this
	 * structure is stored at.
	 */
	int id;
	
	/**
	 * The CPU's APIC ID.
	 */
	uint32_t apicID;
	
	/**
	 * Number of tasks this CPU is currently executing. This does NOT include the idle
	 * task.
	 */
	int numTasks;
} CPU;

/**
 * Trampoline support structure. This structure is loaded into memory, at a known address,
 * so that the AP trampoline may use the data in it to initialize its CPU.
 * Please specify the location of each field in a comment.
 */
typedef struct
{
	/**
	 * The GDT pointer structure, pointing to low virtual memory.
	 */
	uint8_t gdtPtr[10];			// 0xB000
	
	/**
	 * The IDT pointer structure, pointing to high virtual memory
	 * (and therefore must be loaded from within long mode!)
	 */
	uint8_t idtPtr[10];			// 0xB00A
	
	/**
	 * Physical address of the PML4 to be used by the AP.
	 * When the trampoline code runs, it can't load a 64-bit value
	 * into CR3 yet, so we store a temporary copy at 0xC000.
	 */
	uint64_t pageMapPhys;			// 0xB014
	
	/**
	 * Startup stack pointer for this CPU. We cannot use per-CPU
	 * space for the stack since initPerCPU() must be alled first.
	 */
	uint64_t initStack;			// 0xB01C
	
	/**
	 * This spinlock shall be released by the trampoline once the proper
	 * PML4 (instead of the copy) is loaded, the stack pointer is set up
	 * and all trampoline data is ready to be replaced.
	 */
	Spinlock exitLock;			// 0xB024
	
	/**
	 * The trampoline releases flagAP2BSP early on to signal it booted,
	 * and the BSP releases flagBSP2AP to tell the trampoline that it
	 * was seen and may continue booting.
	 */
	Spinlock flagAP2BSP;			// 0xB025
	Spinlock flagBSP2AP;			// 0xB026
} PACKED TrampolineData;

/**
 * Set up the kernel ready for per-CPU variables on the calling CPU.
 */
void initPerCPU();

/**
 * Second stage of per-CPU initialization; done on ALL CPUs (including the BSP),
 * initializes system calls and stuff.
 */
void initPerCPU2();

/**
 * Boot up other CPUs. This must be called after the scheduler is initialised.
 */
void initMultiProc();

/**
 * Halt all CPUs except the calling CPU. Call with interrupts disabled.
 */
void haltAllCPU();

/**
 * Returns the current CPU.
 */
CPU* getCurrentCPU();

/**
 * Allocates a CPU to run a task on.
 */
int allocCPU();

/**
 * Decrease a CPU's task count.
 */
void downrefCPU(int cpuno);

/**
 * Send the scheduler hint to a CPU.
 */
void sendHintToCPU(int id);

/**
 * Send the scheduler hitn to all CPUs.
 */
void sendHintToEveryCPU();

#endif
