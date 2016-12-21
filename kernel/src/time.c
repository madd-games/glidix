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

#include <glidix/time.h>
#include <glidix/console.h>
#include <glidix/sched.h>
#include <glidix/port.h>
#include <glidix/semaphore.h>
#include <glidix/string.h>

#define	SECONDS_PER_HOUR				3600
#define	SECONDS_PER_MINUTE				60
#define	SECONDS_PER_DAY					86400
#define	SECONDS_PER_NONLEAP_YEAR			31536000

// time between system time updates from RTC
#define	RTC_UPDATE_INTERVAL				10*1000

// each entry is the number of days into the year that the given month starts
// so 1 january is the first day of the year, so the value for january is 0.
// this is for non-leap years.
static int64_t monthOffsets[12] = {
	0,								// January
	31,								// February
	31 + 28,							// March
	31 + 28 + 31,							// April
	31 + 28 + 31 + 30,						// May
	31 + 28 + 31 + 30 + 31,						// June
	31 + 28 + 31 + 30 + 31 + 30,					// July
	31 + 28 + 31 + 30 + 31 + 30 + 31,				// August
	31 + 28 + 31 + 30 + 31 + 30 + 31 + 31,				// September
	31 + 28 + 31 + 30 + 31 + 30 + 31 + 31 + 30,			// October
	31 + 28 + 31 + 30 + 31 + 30 + 31 + 31 + 30 + 31,		// November
	31 + 28 + 31 + 30 + 31 + 30 + 31 + 31 + 30 + 31 + 30,		// December
};

static volatile ATOMIC(time_t) currentTime;
static volatile ATOMIC(int) timeUpdateStamp;
static Spinlock timeLock;

void sleep(int ticks)
{
	if (getCurrentThread() == NULL)
	{
		// not ready to wait!
		uint64_t then = getUptime() + (uint64_t)ticks;
		while (getUptime() < then);
	}
	else
	{
		uint64_t then = getUptime() + (uint64_t)ticks;
		uint64_t nanoThen = then * (uint64_t)1000000;

		cli();
		lockSched();
		TimedEvent ev;
		timedPost(&ev, nanoThen);
		
		while (getNanotime() <= nanoThen)
		{
			waitThread(getCurrentThread());
			unlockSched();
			kyield();
			
			cli();
			lockSched();
		};
		
		timedCancel(&ev);
		unlockSched();
		sti();
	};
};

time_t makeUnixTime(int64_t year, int64_t month, int64_t day, int64_t hour, int64_t minute, int64_t second)
{
	int64_t secondsToday = hour * SECONDS_PER_HOUR + minute * SECONDS_PER_MINUTE + second;
	int64_t dayOfYear = monthOffsets[month-1] + (day-1);

	// on leap years, we must add 1 day if the month is march or later
	if (((year % 4) == 0) && (month >= 3))
	{
		dayOfYear++;
	};

	int64_t secondsThisYear = dayOfYear * SECONDS_PER_DAY + secondsToday;
	int64_t relativeYear = year - 1970;
	int64_t secondsBeforeThisYear = relativeYear * SECONDS_PER_NONLEAP_YEAR + (relativeYear+2)/4 * SECONDS_PER_DAY;

	return secondsBeforeThisYear + secondsThisYear;
};

time_t time()
{
	spinlockAcquire(&timeLock);
	time_t out = currentTime + (time_t) (getUptime()-timeUpdateStamp)/1000;
	spinlockRelease(&timeLock);
	return out;
};

static uint8_t getRTCRegister(uint8_t reg)
{
	outb(0x70, reg);
	return inb(0x71);
};

static int getUpdateInProgress()
{
	return getRTCRegister(0x0A) & 0x80;
};

static void rtcThread(void *data)
{
	uint8_t second, minute, hour, day, month, year=0;
	uint8_t lastSecond, lastMinute, lastHour, lastDay, lastMonth, lastYear;
	uint8_t registerB;

	while (1)
	{
		do
		{
			lastYear = year;
			lastMonth = month;
			lastDay = day;
			lastHour = hour;
			lastMinute = minute;
			lastSecond = second;

			while (getUpdateInProgress());
			second = getRTCRegister(0);
			minute = getRTCRegister(2);
			hour = getRTCRegister(4);
			day = getRTCRegister(7);
			month = getRTCRegister(8);
			year = getRTCRegister(9);
		} while ((second != lastSecond) || (minute != lastMinute) || (hour != lastHour)
			|| (month != lastMonth) || (year != lastYear) || (day != lastDay));

		registerB = getRTCRegister(0x0B);
		if (!(registerB & 0x04))
		{
			second = (second & 0xF) + (second / 16) * 10;
			minute = (minute & 0xF) + (minute / 16) * 10;
			hour = ( (hour & 0x0F) + (((hour & 0x70) / 16) * 10) ) | (hour & 0x80);
			day = (day & 0xF) + (day / 16) * 10;
			month = (month & 0xF) + (month / 16) * 10;
			year = (year & 0xF) + (year / 16) * 10;
		};

		if (!(registerB & 0x02) && (hour & 0x80))
		{
			hour = ((hour & 0x7F) + 12) % 24;
		};

		spinlockAcquire(&timeLock);
		currentTime = makeUnixTime(2000+year, month, day, hour, minute, second);
		timeUpdateStamp = getUptime();
		spinlockRelease(&timeLock);
		
		sleep(RTC_UPDATE_INTERVAL);
	};
};

void initRTC()
{
	spinlockRelease(&timeLock);
	KernelThreadParams rtcPars;
	memset(&rtcPars, 0, sizeof(KernelThreadParams));
	rtcPars.stackSize = 0x4000;
	rtcPars.name = "RTC reader daemon";
	CreateKernelThread(rtcThread, &rtcPars, NULL);
};

uint64_t getNanotime()
{
	uint64_t out = getUptime();
	return out * (uint64_t)1000000;		// 10^6 nanoseconds in a millisecond because 10^9 in a second
};

static TimedEvent *timedEvents = NULL;
void timedPost(TimedEvent *ev, uint64_t nanotime)
{
	ev->nanotime = nanotime;
	ev->thread = getCurrentThread();

	if (nanotime == 0)
	{
		ev->prev = ev->next = NULL;
		return;
	};
	
	if (timedEvents == NULL)
	{
		ev->prev = ev->next = NULL;
		timedEvents = ev;
	}
	else
	{
		if (nanotime <= timedEvents->nanotime)
		{
			//ev->prev = timedEvents;
			//ev->next = timedEvents->next;
			//timedEvents = ev;
			timedEvents->prev = ev;
			ev->prev = NULL;
			ev->next = timedEvents;
			timedEvents = ev;
			return;
		};
		
		TimedEvent *scan = timedEvents;
		while (1)
		{
			if (scan->next == NULL)
			{
				ev->prev = scan;
				ev->next = NULL;
				scan->next = ev;
				return;
			}
			else
			{
				if ((scan->nanotime <= nanotime) && (scan->next->nanotime >= nanotime))
				{
					ev->next = scan->next;
					ev->prev = scan;
					scan->next->prev = ev;
					scan->next = ev;
					return;
				};
				
				scan = scan->next;
			};
		};
	};
};

void timedCancel(TimedEvent *ev)
{
	if (timedEvents == ev) timedEvents = ev->next;
	if (ev->prev != NULL) ev->prev->next = ev->next;
	if (ev->next != NULL) ev->next->prev = ev->prev;
};

void onTick()
{
	lockSched();
	
	int doResched = 0;
	while (1)
	{
		if (timedEvents == NULL) break;
		if (getNanotime() <= timedEvents->nanotime) break;
		
		TimedEvent *ev = timedEvents;
		Thread *thread = ev->thread;
		timedEvents = ev->next;
		if (timedEvents != NULL) timedEvents->prev = NULL;
		ev->prev = ev->next = NULL;
		doResched = doResched || signalThread(thread);
	};
	
	unlockSched();
	if (doResched) kyield();
};
