/*
	Glidix Runtime

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

#ifndef _TIME_H
#define _TIME_H

#ifdef __cplusplus
extern "C" {
#endif

#include <sys/types.h>
#include <inttypes.h>

#define	CLK_TCK				1000000
#define	CLOCKS_PER_SEC			1000000				/* value required by POSIX */
#define	TIME_MAX			9223372036854775807L

struct tm
{
	int		tm_sec;
	int		tm_min;
	int		tm_hour;
	int		tm_mday;
	int		tm_mon;
	int		tm_year;
	int		tm_wday;
	int		tm_yday;
	int		tm_isdst;
};

struct timespec
{
	time_t		tv_sec;
	long		tv_nsec;
};

struct itimerspec
{
	struct timespec	it_interval;
	struct timespec	it_value;
};

extern char* tzname[];
extern long int timezone;
extern int daylight;

/* implemented by the runtime */
time_t		time(time_t*);
char*		asctime(const struct tm *timeptr);
char*		asctime_r(const struct tm *timeptr, char *result);
void		tzset();
struct tm*	localtime(const time_t *timer);
struct tm*	localtime_r(const time_t *timer, struct tm *result);
struct tm*	gmtime(const time_t *timer);
struct tm*	gmtime_r(const time_t *timer, struct tm *result);
char*		ctime(const time_t *timer);
char*		ctime_r(const time_t *timer, char *buf);
time_t		mktime(struct tm *tm);
size_t		strftime(char *s, size_t maxsize, const char *format, const struct tm *timeptr);
double		difftime(time_t time1, time_t time0);
clock_t		clock();

/* implemented by libglidix directly */
uint64_t	_glidix_nanotime();

#ifdef __cplusplus
};		/* extern "C" */
#endif

#endif
