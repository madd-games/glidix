/*
	Glidix Network Manager

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

#ifndef GXNETMAN_H_
#define	GXNETMAN_H_

#include <sys/socket.h>
#include <signal.h>

#define	NETMAN_STATUS_OK			0	/* no errors */
#define	NETMAN_STATUS_ERROR			1	/* configuration error */

#define	NETMAN_DAEMON_OK			0	/* daemon started correctly */
#define	NETMAN_DAEMON_NOLOCK			1	/* failed to lock */
#define	NETMAN_DAEMON_NOFORK			2	/* second fork failed */
#define	NETMAN_DAEMON_NONULL			3	/* failed to open /dev/null */
#define	NETMAN_DAEMON_NOLOG			4	/* failed to open log file */
#define	NETMAN_DAEMON_NOSIG			5	/* sigaction failed */

struct NetmanIfConfig_;

typedef int (*netman_conf_init_t)(struct NetmanIfConfig_ *config);
typedef void (*netman_conf_line_t)(struct NetmanIfConfig_ *config, int lineno, char *line);
typedef void (*netman_conf_ifup_t)(struct NetmanIfConfig_ *config);

typedef struct NetmanIfConfig_
{
	int			family;
	char			ifname[16];
	char			method[32];
	int			status;			/* NETMAN_STATUS_* */
	void*			data;
	netman_conf_line_t	linefunc;
	netman_conf_ifup_t	ifupfunc;
} NetmanIfConfig;

int netman_running();

#endif
