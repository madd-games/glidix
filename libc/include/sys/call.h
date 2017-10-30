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

#ifndef _SYS_CALL_H
#define _SYS_CALL_H

#include <inttypes.h>

#define __SYS_exit				0
#define __SYS_write				1
#define __SYS_exec				2
#define __SYS_read				3
#define __SYS_open				4
#define __SYS_close				5
#define __SYS_getpid				6
#define __SYS_getuid				7
#define __SYS_geteuid				8
#define __SYS_getsuid				9
#define __SYS_getgid				10
#define __SYS_getegid				11
#define __SYS_getsgid				12
#define __SYS_sigaction				13
#define __SYS_sigprocmask			14
#define __SYS_stat				15
#define __SYS_getparsz				16
#define __SYS_getpars				17
#define __SYS_raise				18
#define __SYS_fsbase				19
#define __SYS_gsbase				20
#define __SYS_mprotect				21
#define __SYS_lseek				22
#define __SYS_fork				23
#define __SYS_pause				24
#define __SYS_waitpid				25
#define __SYS_kill				26
#define __SYS_insmod				27
#define __SYS_ioctl				28
#define __SYS_fdopendir				29
#define __SYS_mount				31
#define __SYS_yield				32
#define __SYS_time				33
#define __SYS_realpath				34
#define __SYS_chdir				35
#define __SYS_getcwd				36
#define __SYS_fstat				37
#define __SYS_chmod				38
#define __SYS_fchmod				39
#define __SYS_fsync				40
#define __SYS_chown				41
#define __SYS_fchown				42
#define __SYS_mkdir				43
#define __SYS_ftruncate				44
#define __SYS_unlink				45
#define __SYS_dup				46
#define __SYS_dup2				47
#define __SYS_pipe				48
#define __SYS_seterrnoptr			49
#define __SYS_geterrnoptr			50
#define __SYS_nanotime				51
#define __SYS_pread				52
#define __SYS_pwrite				53
#define __SYS_mmap				54
#define __SYS_setuid				55
#define __SYS_setgid				56
#define __SYS_seteuid				57
#define __SYS_setegid				58
#define __SYS_setreuid				59
#define __SYS_setregid				60
#define __SYS_rmmod				61
#define __SYS_link				62
#define __SYS_unmount				63
#define __SYS_lstat				64
#define __SYS_symlink				65
#define __SYS_readlink				66
#define __SYS_down				67
#define __SYS_sleep				68
#define __SYS_utime				69
#define __SYS_umask				70
#define __SYS_routetable			71
#define __SYS_socket				72
#define __SYS_bind				73
#define __SYS_sendto				74
#define __SYS_send				75
#define __SYS_recvfrom				76
#define __SYS_recv				77
#define __SYS_route_add				78
#define __SYS_netconf_stat			79
#define __SYS_netconf_getaddrs			80
#define __SYS_getsockname			81
#define __SYS_shutdown				82
#define __SYS_connect				83
#define __SYS_getpeername			84
#define __SYS_setsockopt			85
#define __SYS_getsockopt			86
#define __SYS_netconf_statidx			87
#define __SYS_pcistat				88
#define __SYS_getgroups				89
#define __SYS_setgroups				90
#define __SYS_uname				91
#define __SYS_netconf_addr			92
#define __SYS_fcntl_getfd			93
#define __SYS_fcntl_setfd			94
#define __SYS_unique				95
#define __SYS_isatty				96
#define __SYS_bindif				97
#define __SYS_route_clear			98
#define __SYS_munmap				99
#define __SYS_pipe2				100
#define __SYS_getppid				101
#define __SYS_alarm				102
#define __SYS_block_on				103
#define __SYS_dup3				104
#define	__SYS_unblock				105
#define	__SYS_trace				106
#define __SYS_listen				107
#define __SYS_accept				108
#define __SYS_accept4				109
#define __SYS_setsid				110
#define __SYS_setpgid				111
#define __SYS_getsid				112
#define __SYS_getpgid				113
#define __SYS_pthread_exit			114
#define __SYS_pthread_create			115
#define __SYS_pthread_self			116
#define __SYS_pthread_join			117
#define __SYS_pthread_detach			118
#define __SYS_pthread_kill			119
#define __SYS_kopt				120
#define __SYS_sigwait				121
#define __SYS_sigsuspend			122
#define	__SYS_sockerr				123
#define __SYS_mcast				124
#define __SYS_fpoll				125
#define __SYS_oxperm				126
#define __SYS_dxperm				127
#define __SYS_fsinfo				128
#define __SYS_chxperm				129
#define __SYS_haveperm				130
#define __SYS_sync				131
#define __SYS_nice				132
#define __SYS_procstat				133
#define __SYS_cpuno				134
#define	__SYS_undef				135	/* the reserved "undefined" number */
#define	__SYS_systat				136
#define	__SYS_fsdrv				137
#define	__SYS_flock_set				138
#define	__SYS_flock_get				139
#define	__SYS_fcntl_setfl			140
#define	__SYS_fcntl_getfl			141
#define	__SYS_aclput				142
#define	__SYS_aclclear				143

#ifdef __cplusplus
extern "C" {
#endif

/**
 * Invoke an arbitrary system call. This function is not completly efficient; it should be used for rare syscalls.
 * For the standard calls, there are wrappers defined in assembly.
 */
uint64_t __syscall(int __sysno, ...);

#ifdef __cplusplus
};	/* extern "C" */
#endif

#endif	/* _SYS_CALL_H */
