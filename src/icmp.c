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

#include <glidix/icmp.h>
#include <glidix/errno.h>
#include <glidix/string.h>

static int sendErrorPacket4(struct sockaddr *src, const struct sockaddr *dest, int errnum, const void *packet, size_t packetlen)
{
	if (packetlen < 28)
	{
		// don't bother replying to extremely short packets made of less than 8 bytes
		// after the IPv4 header.
		return 0;
	};
	
	/**
	 * We must not respond to ICMP messages 3, 11 and 12 as they are error messages,
	 * and we should not reply to an error message with an error messages.
	 */
	IPHeader4 *header = (IPHeader4*) packet;
	if (header->proto == (uint8_t)IPPROTO_ICMP)
	{
		ErrorPacket4 *icmp = (ErrorPacket4*) &header[1];
		if ((icmp->type == 3) || (icmp->type == 11) || (icmp->type == 12))
		{
			return 0;
		};
	};
	
	if (packetlen > 28)
	{
		packetlen = 28;
	};
	
	ErrorPacket4 errpack;

	switch (errnum)
	{
	case ETIMEDOUT:
		errpack.type = 11;
		errpack.code = 0;
		break;
	case ENETUNREACH:
		errpack.type = 3;
		errpack.code = 0;
		break;
	case EHOSTUNREACH:
		errpack.type = 3;
		errpack.code = 1;
		break;
	default:
		return -EINVAL;
	};
	
	errpack.checksum = 0;
	errpack.zero = 0;
	memcpy(errpack.payload, packet, packetlen);
	
	errpack.checksum = ipv4_checksum(&errpack, sizeof(ErrorPacket4));
	
	return sendPacket(src, dest, &errpack, sizeof(ErrorPacket4), IPPROTO_ICMP | PKT_DONTFRAG, NT_SECS(1), NULL);
};

int sendErrorPacket(struct sockaddr *src, const struct sockaddr *dest, int errnum, const void *packet, size_t packetlen)
{
	if (dest->sa_family == AF_INET)
	{
		return sendErrorPacket4(src, dest, errnum, packet, packetlen);
	}
	else
	{
		// TODO: IPv6
		return 0;
	};
};
