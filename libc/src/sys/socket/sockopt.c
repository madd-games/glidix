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

#include <sys/socket.h>
#include <sys/call.h>
#include <sys/time.h>
#include <time.h>
#include <errno.h>

#define	GSO_RCVTIMEO			0
#define	GSO_SNDTIMEO			1
#define	GSO_SNDFLAGS			2
#define	GSO_UNICAST_HOPS		3
#define	GSO_MULTICAST_HOPS		4
#define	GSO_COUNT			5

int setsockopt(int sockfd, int level, int optname, const void *optval, socklen_t optlen)
{
	if ((level == SOL_SOCKET) && (optname == SO_RCVTIMEO))
	{
		if (optlen < sizeof(struct timeval))
		{
			errno = EINVAL;
			return -1;
		};
		
		const struct timeval *tv = (const struct timeval*) optval;
		uint64_t nanotimeout = (uint64_t)tv->tv_sec * 1000000000UL + (uint64_t)tv->tv_usec * 1000UL;
		return _glidix_setsockopt(sockfd, SOL_SOCKET, GSO_RCVTIMEO, nanotimeout);
	}
	else if ((level == SOL_SOCKET) && (optname == SO_SNDTIMEO))
	{
		if (optlen < sizeof(struct timeval))
		{
			errno = EINVAL;
			return -1;
		};
		
		const struct timeval *tv = (const struct timeval*) optval;
		uint64_t nanotimeout = (uint64_t)tv->tv_sec * 1000000000UL + (uint64_t)tv->tv_usec * 1000UL;
		return _glidix_setsockopt(sockfd, SOL_SOCKET, GSO_SNDTIMEO, nanotimeout);
	}
	else if ((level == IPPROTO_IPV6) && ((optname == IPV6_JOIN_GROUP) || (optname == IPV6_LEAVE_GROUP)))
	{
		// IPV6_JOIN_GROUP and IPV6_LEAVE_GROUP in libc have the same values as
		// MCAST_JOIN_GROUP and MCAST_LEAVE_GROUP in the kernel
		if (optlen < sizeof(struct ipv6_mreq))
		{
			errno = EINVAL;
			return -1;
		};
		
		const struct ipv6_mreq *req = (const struct ipv6_mreq*) optval;
		const uint64_t *addrptr = (const uint64_t*) &req->ipv6mr_multiaddr;
		
		return _glidix_mcast(sockfd, optname, req->ipv6mr_interface, addrptr[0], addrptr[1]);
	}
	else if ((level == IPPROTO_IPV6) && ((optname == IPV6_UNICAST_HOPS) || (optname == IPV6_MULTICAST_HOPS)))
	{
		if (optlen != sizeof(uint_t))
		{
			errno = EINVAL;
			return -1;
		};
		
		uint64_t hops = (uint64_t) (*((uint_t*)optval));
		if (hops > 255)
		{
			errno = EINVAL;
			return -1;
		};
		
		if (optname == IPV6_UNICAST_HOPS)
		{
			return _glidix_setsockopt(sockfd, 0, GSO_UNICAST_HOPS, hops);
		}
		else
		{
			return _glidix_setsockopt(sockfd, 0, GSO_MULTICAST_HOPS, hops);
		};
	}
	else if ((level == IPPROTO_IPV6) && (optname == IPV6_V6ONLY))
	{
		if (optlen != sizeof(uint_t))
		{
			errno = EINVAL;
			return -1;
		};
		
		uint_t value = *((uint_t*)optval);
		if (value != 0)
		{
			// glidix does not support restricting IPv6 sockets to IPv6 only
			errno = EINVAL;
			return -1;
		};
		
		return 0;
	}
	else
	{
		errno = ENOPROTOOPT;
		return -1;
	};
};

int getsockopt(int sockfd, int level, int optname, void *optval, socklen_t *optlen)
{
	if ((level == SOL_SOCKET) && (optname == SO_RCVTIMEO))
	{
		if ((*optlen) < sizeof(struct timeval))
		{
			errno = EINVAL;
			return -1;
		};
		
		*optlen = sizeof(struct timeval);
		uint64_t nanotimeout = _glidix_getsockopt(sockfd, SOL_SOCKET, GSO_RCVTIMEO);
		struct timeval *tv = (struct timeval*) optval;
		tv->tv_sec = nanotimeout / 1000000000UL;
		tv->tv_usec = (nanotimeout % 1000000000UL) / 1000UL;
		return 0;
	}
	else if ((level == SOL_SOCKET) && (optname == SO_SNDTIMEO))
	{
		if ((*optlen) < sizeof(struct timeval))
		{
			errno = EINVAL;
			return -1;
		};
		
		*optlen = sizeof(struct timeval);
		uint64_t nanotimeout = _glidix_getsockopt(sockfd, SOL_SOCKET, GSO_SNDTIMEO);
		struct timeval *tv = (struct timeval*) optval;
		tv->tv_sec = nanotimeout / 1000000000UL;
		tv->tv_usec = (nanotimeout % 1000000000UL) / 1000UL;
		return 0;
	}
	else if ((level == IPPROTO_IPV6) && ((optname == IPV6_UNICAST_HOPS) || (optname == IPV6_MULTICAST_HOPS)))
	{
		if ((*optlen) < sizeof(uint_t))
		{
			errno = EINVAL;
			return -1;
		};
		
		*optlen = sizeof(uint_t);
		
		uint64_t hops;
		if (optname == IPV6_UNICAST_HOPS)
		{
			hops = _glidix_getsockopt(sockfd, 0, GSO_UNICAST_HOPS);
			if (hops == 0) hops = 64;
		}
		else
		{
			hops = _glidix_getsockopt(sockfd, 0, GSO_MULTICAST_HOPS);
			if (hops == 0) hops = 1;
		};
		
		*((uint_t*)optval) = (uint_t) hops;
		return 0;
	}
	else if ((level == IPPROTO_IPV6) && (optname == IPV6_V6ONLY))
	{
		if ((*optlen) < sizeof(uint_t))
		{
			errno = EINVAL;
			return -1;
		};
		
		*optlen = sizeof(uint_t);
		*((uint_t*)optval) = 0;
		return 0;
	}
	else if ((level == SOL_SOCKET) && (optname == SO_ERROR))
	{
		if ((*optlen) < sizeof(int))
		{
			errno = EINVAL;
			return -1;
		};
		
		int result = __syscall(__SYS_sockerr, sockfd);
		if (result == -1)
		{
			// errno was set by the system call
			return -1;
		};
		
		*optlen = sizeof(int);
		*((int*)optval) = result;
		return 0;
	}
	else
	{
		errno = ENOPROTOOPT;
		return -1;
	};
};
