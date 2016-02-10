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

#ifndef __glidix_term_h
#define __glidix_term_h

/**
 * POSIX terminal interface.
 */

#include <stdint.h>
#include <glidix/ioctl.h>

#define	IOCTL_TTY_GETATTR	IOCTL_ARG(struct termios, IOCTL_INT_TERM, 0)
#define	IOCTL_TTY_SETATTR	IOCTL_ARG(struct termios, IOCTL_INT_TERM, 1)

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

// control characters
enum
{
	CC_VEOF = 0x80,
	CC_VEOL = 0x81,
	CC_VERASE = 0x82,
	CC_VINTR = 0x83,
	CC_VKILL = 0x84,
	CC_VMIN = 0x85,
	CC_VQUIT = 0x86,
	CC_VSTART = 0x87,
	CC_VSTOP = 0x88,
	CC_VSUSP = 0x89,
	CC_VTIME = 0x8A,
	
	CC_ARROW_UP = 0x8B,
	CC_ARROW_DOWN = 0x8C,
	CC_ARROW_LEFT = 0x8D,
	CC_ARROW_RIGHT = 0x8E
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

#endif
