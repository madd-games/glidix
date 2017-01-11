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

#include <sys/glidix.h>
#include <time.h>
#include <stdio.h>

#define SECS_DAY								(24L * 60L * 60L)
#define YEAR0									1900

struct tm __localtime_buf;
char __asctime_buf[26];

char *tzname[2];
long int timezone;
int daylight;
long _dstbias = 0;                  // Offset for Daylight Saving Time

const char *_days[] = {
	"Sunday", "Monday", "Tuesday", "Wednesday",
	"Thursday", "Friday", "Saturday"
};

const char *_days_abbrev[] = {
	"Sun", "Mon", "Tue", "Wed", 
	"Thu", "Fri", "Sat"
};

const char *_months[] = {
	"January", "February", "March",
	"April", "May", "June",
	"July", "August", "September",
	"October", "November", "December"
};

const char *_months_abbrev[] = {
	"Jan", "Feb", "Mar",
	"Apr", "May", "Jun",
	"Jul", "Aug", "Sep",
	"Oct", "Nov", "Dec"
};

const int _ytab[2][12] = {
	{31, 28, 31, 30, 31, 30, 31, 31, 30, 31, 30, 31},
	{31, 29, 31, 30, 31, 30, 31, 31, 30, 31, 30, 31}
};

time_t time(time_t *timer)
{
	time_t ret = _glidix_time();
	if (timer != NULL) *timer = ret;
	return ret;
};

char *asctime(const struct tm *timeptr)
{
	return asctime_r(timeptr, __asctime_buf);
};

char *asctime_r(const struct tm *timeptr, char *result)
{
	static const char *wday_name[7] = {
		"Sun", "Mon", "Tue", "Wed", "Thu", "Fri", "Sat"
	};
	static const char* mon_name[12] = {
		"Jan", "Feb", "Mar", "Apr", "May", "Jun",
		"Jul", "Aug", "Sep", "Oct", "Nov", "Dec"
	};

	sprintf(result, "%.3s %.3s%3d %.2d:%.2d:%.2d %d\n",
		wday_name[timeptr->tm_wday],
		mon_name[timeptr->tm_mon],
		timeptr->tm_mday, timeptr->tm_hour,
		timeptr->tm_min, timeptr->tm_sec,
		1900 + timeptr->tm_year);

	return result;
};

void tzset()
{
	tzname[0] = "UTC";
	tzname[1] = "UTC";
	timezone = 0;
	daylight = 0;
};

struct tm *localtime(const time_t *timer)
{
	return localtime_r(timer, &__localtime_buf);
};

struct tm *localtime_r(const time_t *timer, struct tm *tmbuf)
{
	time_t ltime = (*timer) + timezone;
	return gmtime_r(&ltime, tmbuf);
};

struct tm *gmtime(const time_t *timer)
{
	return gmtime_r(timer, &__localtime_buf);
};

#define LEAPYEAR(year)					(!((year) % 4) && (((year) % 100) || !((year) % 400)))
#define YEARSIZE(year)					(LEAPYEAR(year) ? 366 : 365)
struct tm *gmtime_r(const time_t *timer, struct tm *tmbuf)
{
	time_t time = *timer;
	unsigned long dayclock, dayno;
	int year = 1970;

	dayclock = (unsigned long) time % (24L * 60L * 60L);
	dayno = (unsigned long) time / (24L * 60L * 60L);

	tmbuf->tm_sec = dayclock % 60;
	tmbuf->tm_min = (dayclock % 3600) / 60;
	tmbuf->tm_hour = dayclock / 3600;
	tmbuf->tm_wday = (dayno + 4) % 7; // Day 0 was a thursday
	while (dayno >= (unsigned long) YEARSIZE(year)) {
		dayno -= YEARSIZE(year);
		year++;
	}
	tmbuf->tm_year = year - 1900;
	tmbuf->tm_yday = dayno;
	tmbuf->tm_mon = 0;
	while (dayno >= (unsigned long) _ytab[LEAPYEAR(year)][tmbuf->tm_mon]) {
		dayno -= _ytab[LEAPYEAR(year)][tmbuf->tm_mon];
		tmbuf->tm_mon++;
	}
	tmbuf->tm_mday = dayno + 1;
	tmbuf->tm_isdst = 0;
	return tmbuf;
};

char *ctime(const time_t *timer)
{
	return asctime(localtime(timer));
};

char *ctime_r(const time_t *timer, char *buf)
{
	struct tm tm;
	localtime_r(timer, &tm);
	return asctime_r(&tm, buf);
};

time_t mktime(struct tm *tmbuf) {
	long day, year;
	int tm_year;
	int yday, month;
	/*unsigned*/ long seconds;
	int overflow;
	long dst;

	tmbuf->tm_min += tmbuf->tm_sec / 60;
	tmbuf->tm_sec %= 60;
	if (tmbuf->tm_sec < 0) {
		tmbuf->tm_sec += 60;
		tmbuf->tm_min--;
	}
	tmbuf->tm_hour += tmbuf->tm_min / 60;
	tmbuf->tm_min = tmbuf->tm_min % 60;
	if (tmbuf->tm_min < 0) {
		tmbuf->tm_min += 60;
		tmbuf->tm_hour--;
	}
	day = tmbuf->tm_hour / 24;
	tmbuf->tm_hour= tmbuf->tm_hour % 24;
	if (tmbuf->tm_hour < 0) {
		tmbuf->tm_hour += 24;
		day--;
	}
	tmbuf->tm_year += tmbuf->tm_mon / 12;
	tmbuf->tm_mon %= 12;
	if (tmbuf->tm_mon < 0) {
		tmbuf->tm_mon += 12;
		tmbuf->tm_year--;
	}
	day += (tmbuf->tm_mday - 1);
	while (day < 0) {
		if(--tmbuf->tm_mon < 0) {
			tmbuf->tm_year--;
			tmbuf->tm_mon = 11;
		}
		day += _ytab[LEAPYEAR(YEAR0 + tmbuf->tm_year)][tmbuf->tm_mon];
	}
	while (day >= _ytab[LEAPYEAR(YEAR0 + tmbuf->tm_year)][tmbuf->tm_mon]) {
		day -= _ytab[LEAPYEAR(YEAR0 + tmbuf->tm_year)][tmbuf->tm_mon];
		if (++(tmbuf->tm_mon) == 12) {
			tmbuf->tm_mon = 0;
			tmbuf->tm_year++;
		}
	}
	tmbuf->tm_mday = day + 1;
	year = 1970;
	if (tmbuf->tm_year < year - YEAR0) return (time_t) -1;
	seconds = 0;
	day = 0;											// Means days since day 0 now
	overflow = 0;

	// Assume that when day becomes negative, there will certainly
	// be overflow on seconds.
	// The check for overflow needs not to be done for leapyears
	// divisible by 400.
	// The code only works when year (1970) is not a leapyear.
	tm_year = tmbuf->tm_year + YEAR0;

	if (TIME_MAX / 365 < tm_year - year) overflow++;
	day = (tm_year - year) * 365;
	if (TIME_MAX - day < (tm_year - year) / 4 + 1) overflow++;
	day += (tm_year - year) / 4 + ((tm_year % 4) && tm_year % 4 < year % 4);
	day -= (tm_year - year) / 100 + ((tm_year % 100) && tm_year % 100 < year % 100);
	day += (tm_year - year) / 400 + ((tm_year % 400) && tm_year % 400 < year % 400);

	yday = month = 0;
	while (month < tmbuf->tm_mon) {
		yday += _ytab[LEAPYEAR(tm_year)][month];
		month++;
	}
	yday += (tmbuf->tm_mday - 1);
	if (day + yday < 0) overflow++;
	day += yday;

	tmbuf->tm_yday = yday;
	tmbuf->tm_wday = (day + 4) % 7;							 // Day 0 was thursday (4)

	seconds = ((tmbuf->tm_hour * 60L) + tmbuf->tm_min) * 60L + tmbuf->tm_sec;

	if ((TIME_MAX - seconds) / SECS_DAY < day) overflow++;
	seconds += day * SECS_DAY;

	// Now adjust according to timezone and daylight saving time
	if (((timezone > 0) && (TIME_MAX - timezone < seconds)) || 
			((timezone < 0) && (seconds < -timezone))) {
					overflow++;
	}
	seconds += timezone;

	if (tmbuf->tm_isdst) {
		dst = _dstbias;
	} else {
		dst = 0;
	}

	if (dst > seconds) overflow++;				// dst is always non-negative
	seconds -= dst;

	if (overflow) return (time_t) -1;

	if ((time_t) seconds != seconds) return (time_t) -1;
	return (time_t) seconds;
};
