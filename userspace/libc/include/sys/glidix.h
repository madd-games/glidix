/*
	Glidix Runtime

	Copyright (c) 2014-2015, Madd Games.
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

#ifndef _SYS_GLIDIX_H
#define _SYS_GLIDIX_H

/**
 * Non-standard declarations for libglidix functions are here.
 */

#include <sys/types.h>
#include <netinet/in.h>
#include <stddef.h>

typedef struct
{
	uint64_t rdi, rsi, rbp, rbx, rdx, rcx, rax;
	uint64_t r8, r9, r10, r11, r12, r13, r14, r15;
	uint64_t rip, rsp;
} __attribute__ ((packed)) _glidix_mstate;

typedef struct
{
	uint64_t		dynSize;
	void*			dynSection;
	uint64_t		loadAddr;
	uint64_t		nextLoadAddr;
	uint64_t		textIndex;
	uint64_t		dataIndex;
} _glidix_libinfo;

typedef struct
{
	char				ifname[16];
	struct in_addr			dest;
	struct in_addr			mask;
	struct in_addr			gateway;
	uint64_t			flags;
} _glidix_in_route;

typedef struct
{
	char				ifname[16];
	struct in6_addr			dest;
	struct in6_addr			mask;
	struct in6_addr			gateway;
	uint64_t			flags;
} _glidix_in6_route;

typedef struct
{
	char				ifname[16];
	char				data[3*16];
	uint64_t			flags;
} _glidix_gen_route;

typedef union
{
	int				type;	
	struct
	{
		int			type;
	} loopback;

	struct
	{
		int			type;
		uint8_t			mac[6];
	} ethernet;

	struct
	{
		int			type;
	} tunnel;
} _glidix_ifconfig;

typedef struct
{
	char				ifname[16];
	int				numTrans;
	int				numRecv;
	int				numDropped;
	int				numErrors;
	int				numAddrs4;
	int				numAddrs6;
	_glidix_ifconfig		ifconfig;
} _glidix_netstat;

typedef struct
{
	struct in_addr			addr;
	struct in_addr			mask;
} _glidix_ifaddr4;

typedef struct
{
	struct in6_addr			addr;
	struct in6_addr			mask;
} _glidix_ifaddr6;

typedef struct
{
	void*						ignore1;
	uint8_t						bus;
	uint8_t						slot;
	uint8_t						func;
	int						id;
	uint16_t					vendor;
	uint16_t					device;
	uint16_t					type;
	uint8_t						intpin;
	void*						ignore2;
	char						driverName[128];
	char						deviceName[128];
} _glidix_pcidev;

#ifdef __cplusplus
extern "C" {
#endif

#define	_GLIDIX_CLONE_SHARE_MEMORY		(1 << 0)
#define	_GLIDIX_CLONE_SHARE_FTAB		(1 << 1)

#define	_GLIDIX_INSMOD_VERBOSE			(1 << 0)

#define	_GLIDIX_RMMOD_VERBOSE			(1 << 0)
#define	_GLIDIX_RMMOD_FORCE			(1 << 1)

#define	_GLIDIX_DOWN_POWEROFF				0
#define	_GLIDIX_DOWN_REBOOT				1
#define	_GLIDIX_DOWN_HALT				2

int		_glidix_exec(const char *path, const char *pars, size_t parsz);
int		_glidix_open(const char *path, int flags, mode_t mode);
uid_t		_glidix_getsuid();
gid_t		_glidix_getsgid();
void		_glidix_sighandler(void *handler);
void		_glidix_sigret(void *ret);
size_t		_glidix_getparsz();
void		_glidix_getpars(char *buffer, size_t size);
int		_glidix_geterrno();
void		_glidix_seterrno(int _errno);
pid_t		_glidix_clone(int flags, const _glidix_mstate *state);
pid_t		_glidix_pollpid(pid_t pid, int *stat_loc, int flags);
int		_glidix_insmod(const char *modname, const char *path, const char *opt, int flags);
int		_glidix_ioctl(int fd, unsigned long cmd, void *argp);
int		_glidix_fdopendir(const char *dirname);
void		_glidix_diag();
int		_glidix_mount(const char *filesystem, const char *image, const char *mountpoint, int flags);
void		_glidix_yield();
time_t		_glidix_time();
void		_glidix_seterrnoptr(int *ptr);
int*		_glidix_geterrnoptr();
int		_glidix_libopen(const char *path, uint64_t loadAddr, _glidix_libinfo *info);
void		_glidix_libclose(_glidix_libinfo *info);
uint64_t	_glidix_mmap(uint64_t addr, size_t len, int prot, int flags, int fd, off_t offset);
int		_glidix_rmmod(const char *modname, int flags);
int		_glidix_unmount(const char *prefix);
int		_glidix_down(int action);
int		_glidix_utime(const char *path, time_t atime, time_t mtime);
int		_glidix_routetab(sa_family_t family);
int		_glidix_route_add(int family, int pos, _glidix_gen_route *route);
ssize_t		_glidix_netconf_stat(const char *ifname, _glidix_netstat *buffer, size_t bufsize);
ssize_t		_glidix_netconf_getaddrs(const char *ifname, int family, void *buffer, size_t bufsize);
int		_glidix_setsockopt(int socket, int proto, int option, uint64_t value);
uint64_t	_glidix_getsockopt(int socket, int proto, int option);
ssize_t		_glidix_netconf_statidx(unsigned int index, _glidix_netstat *buffer, size_t bufsize);
ssize_t		_glidix_pcistat(int id, _glidix_pcidev *buffer, size_t bufsize);
int		_glidix_setgroups(size_t count, const gid_t *groups);

#ifdef __cplusplus
}	/* extern "C" */
#endif

#endif
