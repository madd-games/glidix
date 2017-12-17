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

#ifndef __glidix_fsdriver_h
#define __glidix_fsdriver_h

/**
 * An interface for filesystem drivers.
 */

#include <glidix/vfs.h>

/**
 * Represents a linked list of filesystem drivers.
 */
typedef struct _FSDriver FSDriver;
struct _FSDriver
{
	/**
	 * Links.
	 */
	FSDriver*					prev;
	FSDriver*					next;
	
	/**
	 * Name of the filesystem, as used by mount() etc.
	 */
	const char*					fsname;
	
	/**
	 * This function is called when someone attempts to mount the filesystem. This function shall
	 * look at the filesystem image, 'image', and set up the filesystem structure, and load the root
	 * inode and then return it. If that fails, it shall return NULL, and if 'error' is not NULL,
	 * it must set it to an error number.
	 *
	 * 'options' and 'optlen' defines a range of memory where a filesystem-driver-specific options
	 * structure is stored, specifies by the application. Each driver defines their own option structure.
	 */
	Inode* (*openroot)(const char *image, const void *options, size_t optlen, int *error);
};

/**
 * Initialize the filesystem driver subsystem.
 */
void initFSDrivers();

/**
 * Register a new filesystem driver.
 */
void registerFSDriver(FSDriver *drv);

/**
 * Implements the system call for mounting a filesystem.
 *	fsname		Name of the filesystem e.g. "isofs"
 *	image		Path to the filesystem image, if the driver takes this into account.
 *	mountpoint	Path to the mountpoint. Must be an already-existing directory or file.
 *	flags		Mount flags. Currently none accepted.
 *	options		Pointer to an "options" structure, specific to the filesystem type. May be NULL (and optlen would
 *			then have to be 0)
 *	optlen		Size of the options structure.
 *
 * Returns 0 on success, or -1 on error and sets ERRNO.
 */
int sys_mount(const char *fsname, const char *image, const char *mountpoint, int flags, const void *options, size_t optlen);

/**
 * Return a list of filesystem drivers. Takes an array of "num" names (each an array of 16 bytes) which will be filled
 * with strings naming filesystem drivers. Returns the amount of drivers actually returned.
 */
int sys_fsdrv(char *buffer, int num);

#endif
