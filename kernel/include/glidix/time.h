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

#ifndef __glidix_time_h
#define __glidix_time_h

#include <glidix/common.h>

#define	NANO_PER_SEC			1000000000UL

struct _Thread;
typedef struct TimedEvent_
{
	/**
	 * The nanotime at which the event shall occur.
	 */
	uint64_t			nanotime;
	
	/**
	 * Which thread to wake up.
	 */
	struct _Thread*			thread;
	
	struct TimedEvent_*		prev;
	struct TimedEvent_*		next;
} TimedEvent;

uint64_t getUptime();			// idt.c
#define	getTicks getUptime
uint64_t getNanotime();
void sleep(int ticks);
time_t makeUnixTime(int64_t year, int64_t month, int64_t day, int64_t hour, int64_t minute, int64_t second);
time_t time();
void initRTC();

/**
 * Add a timed event. Only call this when the scheduler is locked (lockSched()). The thread will be woken up when the
 * system timer reaches "nanotime". It may be woken up before that, for other reasons. You must always call timedCancel()
 * on the event, even if the deadline passed. The TimedEvent structure may be allocated on the stack; it shall not be
 * initialized (this function performs initialization).
 *
 * Seeting nanotime to zero causes the event to be ignored.
 */
void timedPost(TimedEvent *ev, uint64_t nanotime);

/**
 * Remove the event from the queue if it's still there. Always call this after timedPost().
 */
void timedCancel(TimedEvent *ev);

/**
 * Called on each tick, with interrupts disabled.
 */
void onTick();

#endif
