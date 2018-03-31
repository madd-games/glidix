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

#ifndef _TERMIOS_H
#define _TERMIOS_H

#include <sys/ioctl.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

#define	__IOCTL_TTY_GETATTR	__IOCTL_ARG(struct termios, 5, 0)
#define	__IOCTL_TTY_SETATTR	__IOCTL_ARG(struct termios, 5, 1)
#define	__IOCTL_TTY_GETPGID	__IOCTL_ARG(struct termios, 5, 2)
#define	__IOCTL_TTY_SETPGID	__IOCTL_ARG(struct termios, 5, 3)
#define	__IOCTL_TTY_GRANTPT	__IOCTL_NOARG(5, 4)
#define	__IOCTL_TTY_UNLOCKPT	__IOCTL_NOARG(5, 5)
#define	__IOCTL_TTY_PTSNAME	__IOCTL_ARG(__ptsname, 5, 6)
#define	__IOCTL_TTY_ISATTY	__IOCTL_NOARG(5, 7)
#define	__IOCTL_TTY_GETSIZE	__IOCTL_ARG(struct winsize, 5, 8)

#define	TIOCGWINSZ		__IOCTL_TTY_GETSIZE

typedef struct
{
	char __ign[256];
} __ptsname;

// input modes
#define	BRKINT			(1 << 0)
#define	ICRNL			(1 << 1)
#define	IGNBRK			(1 << 2)
#define	IGNCR			(1 << 3)
#define	IGNPAR			(1 << 4)
#define	INLCR			(1 << 5)
#define	INPCK			(1 << 6)
#define	ISTRIP			(1 << 7)
#define	IXANY			(1 << 8)
#define	IXOFF			(1 << 9)
#define	IXON			(1 << 10)
#define	PARMRK			(1 << 11)

// output modes
#define	OPOST			(1 << 0)
#define	ONLCR			(1 << 1)
#define	OCRNL			(1 << 2)
#define	ONOCR			(1 << 3)
#define	ONLRET			(1 << 4)
#define	OFILL			(1 << 5)

// local modes
#define	ECHO			(1 << 0)
#define	ECHOE			(1 << 1)
#define	ECHOK			(1 << 2)
#define	ECHONL			(1 << 3)
#define	ICANON			(1 << 4)
#define	IEXTEN			(1 << 5)
#define	ISIG			(1 << 6)
#define	NOFLSH			(1 << 7)
#define	TOSTOP			(1 << 8)

#define	TCSANOW			0
#define	TCSADRAIN		1
#define	TCSAFLUSH		2

// control characters
enum
{
	CC_VEOF = 0x80,
	CC_VEOL,
	CC_VERASE,
	CC_VINTR,
	CC_VKILL,
	CC_VMIN,
	CC_VQUIT,
	CC_VSTART,
	CC_VSTOP,
	CC_VSUSP,
	CC_VTIME
};

enum
{
	VEOF,
	VEOL,
	VERASE,
	VINTR,
	VKILL,
	VMIN,
	VQUIT,
	VSTART,
	VSTOP,
	VSUSP,
	VTIME,
	NCCS
};

typedef	uint16_t		tcflag_t;
typedef	uint8_t			cc_t;
typedef	uint16_t		speed_t;

struct termios
{
	tcflag_t		c_iflag;
	tcflag_t		c_oflag;
	tcflag_t		c_cflag;
	tcflag_t		c_lflag;
	cc_t			c_cc[NCCS];
};

struct winsize
{
	unsigned short		ws_row;
	unsigned short		ws_col;
	unsigned short		ws_xpixel;
	unsigned short		ws_ypixel;
};

int tcgetattr(int fildes, struct termios *tc);
int tcsetattr(int fildes, int optional_actions, const struct termios *tc);

#ifdef __cplusplus
};			/* extern "C" */
#endif

#endif
