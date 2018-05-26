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
	char fpu[512];
	uint64_t rflags;
	uint64_t rip;
	uint64_t rdi, rsi, rbp, rbx, rdx, rcx, rax;
	uint64_t r8, r9, r10, r11, r12, r13, r14, r15;
	uint64_t rsp;
} __attribute__ ((packed)) _glidix_mstate;

typedef struct
{
	char				ifname[16];
	struct in_addr			dest;
	struct in_addr			mask;
	struct in_addr			gateway;
	int				domain;
	uint64_t			flags;
} _glidix_in_route;

typedef struct
{
	char				ifname[16];
	struct in6_addr			dest;
	struct in6_addr			mask;
	struct in6_addr			gateway;
	int				domain;
	uint64_t			flags;
} _glidix_in6_route;

typedef struct
{
	char				ifname[16];
	char				data[3*16+4];
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
	char				resv_[1024-(40+sizeof(_glidix_ifconfig))];
	uint32_t			scopeID;
} _glidix_netstat;

typedef struct
{
	struct in_addr			addr;
	struct in_addr			mask;
	int				domain;
} _glidix_ifaddr4;

typedef struct
{
	struct in6_addr			addr;
	struct in6_addr			mask;
	int				domain;
} _glidix_ifaddr6;

typedef struct
{
	void*				ignore1;
	uint8_t				bus;
	uint8_t				slot;
	uint8_t				func;
	int				id;
	uint16_t			vendor;
	uint16_t			device;
	uint16_t			type;
	uint8_t				intpin;
	void*				ignore2;
	char				driverName[128];
	char				deviceName[128];
	uint32_t			bar[6];
	uint32_t			barsz[6];
	uint8_t				progif;
	int				intNo;
} _glidix_pcidev;

struct sockaddr_cap
{
	uint16_t			scap_family;		/* AF_CAPTURE */
	char				scap_ifname[16];
	int				scap_proto_flags;
	/* without the padding, sizeof(sockaddr_cap)=24. Pad to 28 by adding 4 */
	char				scap_pad[4];
};

struct ether_header
{
	uint8_t				ether_dest[6];
	uint8_t				ether_src[6];
	uint16_t			ether_type;
};

typedef struct
{
	int						width;
	int						height;
	int						posX;
	int						posY;
} _glidix_ptrstate;

#ifdef __cplusplus
extern "C" {
#endif

/* types of interfaces */
#define	IF_LOOPBACK			0		/* loopback interface (localhost) */
#define	IF_ETHERNET			1		/* ethernet controller */
#define	IF_TUNNEL			2		/* software tunnel */

#define	_GLIDIX_INSMOD_VERBOSE				(1 << 0)

#define	_GLIDIX_RMMOD_VERBOSE				(1 << 0)
#define	_GLIDIX_RMMOD_FORCE				(1 << 1)

#define	_GLIDIX_DOWN_POWEROFF				0
#define	_GLIDIX_DOWN_REBOOT				1
#define	_GLIDIX_DOWN_HALT				2

#define	_GLIDIX_IOCTL_NOARG(intf, cmd)			((intf << 16) | cmd)
#define	_GLIDIX_IOCTL_ARG(type, intf, cmd)		((sizeof(type) << 32) | _GLIDIX_IOCTL_NOARG(intf, cmd))

#define	_GLIDIX_IOCTL_INT_PCI				0x0001
#define	_GLIDIX_IOCTL_INT_SDI				0x0002
#define	_GLIDIX_IOCTL_INT_VIDEO				0x0003
#define	_GLIDIX_IOCTL_INT_GPU				0x0004
#define	_GLIDIX_IOCTL_INT_TERM				0x0005

#define	_GLIDIX_MQ_CONNECT				0
#define	_GLIDIX_MQ_INCOMING				1
#define	_GLIDIX_MQ_HANGUP				2

#define	_GLIDIX_KOPT_GFXTERM				0

#define	_GLIDIX_DOM_GLOBAL				0	/* global (internet) */
#define	_GLIDIX_DOM_LINK				1	/* link-local (LAN only) */
#define	_GLIDIX_DOM_LOOPBACK				2	/* loopback (host only) */
#define	_GLIDIX_DOM_SITE				3	/* site-local (organization scope only) */
#define	_GLIDIX_DOM_MULTICAST				4	/* multicast (used for addresses and NEVER routes) */
#define	_GLIDIX_DOM_NODEFAULT				5	/* non-default address (never selected for any route) */

struct __siginfo;

int		_glidix_exec(const char *path, const char *pars, size_t parsz);
int		_glidix_open(const char *path, int flags, mode_t mode);
uid_t		_glidix_getsuid();
gid_t		_glidix_getsgid();
size_t		_glidix_getparsz();
void		_glidix_getpars(char *buffer, size_t size);
int		_glidix_insmod(const char *modname, const char *path, const char *opt, int flags);
int		_glidix_ioctl(int fd, unsigned long cmd, void *argp);
int		_glidix_pathctl(const char *path, unsigned long cmd, void *argp);
int		_glidix_pathctlat(int dirfd, const char *path, unsigned long cmd, void *argp);
void		_glidix_diag();
int		_glidix_mount(const char *fsname, const char *image, const char *mountpoint, int flags, const void *options, size_t optlen);
void		_glidix_yield();
time_t		_glidix_time();
void		_glidix_seterrnoptr(int *ptr);
int*		_glidix_geterrnoptr();
int		_glidix_rmmod(const char *modname, int flags);
int		_glidix_unmount(const char *path, int flags);
int		_glidix_down(int action);
int		_glidix_routetab(sa_family_t family);
int		_glidix_route_add(int family, int pos, _glidix_gen_route *route);
ssize_t		_glidix_netconf_stat(const char *ifname, _glidix_netstat *buffer, size_t bufsize);
ssize_t		_glidix_netconf_getaddrs(const char *ifname, int family, void *buffer, size_t bufsize);
int		_glidix_setsockopt(int socket, int proto, int option, uint64_t value);
uint64_t	_glidix_getsockopt(int socket, int proto, int option);
ssize_t		_glidix_netconf_statidx(unsigned int index, _glidix_netstat *buffer, size_t bufsize);
ssize_t		_glidix_pcistat(int id, _glidix_pcidev *buffer, size_t bufsize);
int		_glidix_setgroups(size_t count, const gid_t *groups);
int		_glidix_netconf_addr(int family, const char *ifname, const void *buffer, size_t size);
int		_glidix_fcntl_getfd(int fd);
int		_glidix_fcntl_setfd(int fd, int flags);
uint32_t	_glidix_unique();
int		_glidix_bindif(int fd, const char *ifname);
int		_glidix_route_clear(int family, const char *ifname);
int		_glidix_kopt(int option, int value);
int		_glidix_sigwait(uint64_t sigset, struct __siginfo *info, uint64_t nanotimeout);
int		_glidix_sigsuspend(uint64_t mask);
int		_glidix_mcast(int sockfd, int op, uint32_t scope, uint64_t addr0, uint64_t addr1);
int		_glidix_fpoll(const uint8_t *bitmapReq, uint8_t *bitmapRes, int flags, uint64_t nanotimeout);
int		_glidix_cpuno();

// some runtime stuff
uint64_t	__alloc_pages(size_t len);

#ifdef __cplusplus
}	/* extern "C" */
#endif

#endif
