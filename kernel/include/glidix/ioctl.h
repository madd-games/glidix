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

#ifndef __glidix_ioctl_h
#define __glidix_ioctl_h

/**
 * An ioctl() command is a 64-bit value, structured as follows:
 * 	Top 16 bits: reserved; should be 0.
 * 	Next 16 bits: size of the param structure.
 * 	Next 16 bits: type of interface; see below.
 *	Next 8 bits: reserved; should be 0.
 *	Next 8 bits: command number within the interface.
 * Please construct them with the following macros:
 *	IOCTL_NOARG(interfaceType, commandNo) for commands without arguments.
 *	IOCTL_ARG(type, interfaceType, commandNo) for commands with argument of type 'type'.
 */

#define	IOCTL_NOARG(intf, cmd)					((intf << 16) | cmd)
#define	IOCTL_ARG(type, intf, cmd)				((sizeof(type) << 32) | IOCTL_NOARG(intf, cmd))

/**
 * Interface types (see docs/ioctl.html for more info).
 */
#define	IOCTL_INT_PCI						0x0001
#define	IOCTL_INT_SDI						0x0002
#define	IOCTL_INT_VIDEO						0x0003
#define	IOCTL_INT_GPU						0x0004
#define	IOCTL_INT_TERM						0x0005
#define	IOCTL_INT_THSYNC					0x0006

#endif
