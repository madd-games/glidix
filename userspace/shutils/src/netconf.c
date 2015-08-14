/*
	Glidix Shell Utilities

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

#ifndef __glidix__
#	error This program works on Glidix only!
#endif

#include <sys/glidix.h>
#include <arpa/inet.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <errno.h>

char *progName;

void usage()
{
	fprintf(stderr, "USAGE:\t%s <ifname>\n", progName);
	fprintf(stderr, "\tConfigure network interfaces.\n");
	exit(1);
};

int main(int argc, char *argv[])
{
	progName = argv[0];
	
	if (argc != 2)
	{
		usage();
		return 1;
	};
	
	_glidix_netstat netstat;
	if (_glidix_netconf_stat(argv[1], &netstat, sizeof(_glidix_netstat)) == -1)
	{
		fprintf(stderr, "%s: %s: %s\n", argv[0], argv[1], strerror(errno));
		return 1;
	};
	
	_glidix_ifaddr4 *addrs = (_glidix_ifaddr4*) malloc(sizeof(_glidix_ifaddr4) * netstat.numAddrs4);
	_glidix_netconf_getaddrs(argv[1], AF_INET, addrs, sizeof(_glidix_ifaddr4) * netstat.numAddrs4);
	
	printf("Interface: %s\n", netstat.ifname);
	printf("\tpackets sent: %d\n", netstat.numTrans);
	printf("\tpackets received: %d\n", netstat.numRecv);
	printf("\tpacket errors: %d\n", netstat.numErrors);
	printf("\tpackets dropped: %d\n", netstat.numDropped);
	printf("\tAddresses:\n");
	char netaddr[INET_ADDRSTRLEN];
	int netsize;
	int i;
	for (i=0; i<netstat.numAddrs4; i++)
	{
		inet_ntop(AF_INET, &addrs[i].addr, netaddr, INET_ADDRSTRLEN);
		netsize = 0;
		uint8_t *bytes = (uint8_t*) &addrs[i].mask;
		int j;
		netsize = 0;
		for (j=0; j<4; j++)
		{
			if (bytes[j] == 0xFF)
			{
				netsize += 8;
			}
			else
			{
				uint8_t byte = bytes[j];
				while (byte & 0x80)
				{
					netsize++;
					byte <<= 1;
				};
				break;
			};
		};
		printf("\t\t%s/%d\n", netaddr, netsize);
	};
	
	printf("\n");
	
	return 0;
};
