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

#include <sys/glidix.h>
#include <net/if.h>
#include <stdlib.h>
#include <errno.h>

uint32_t if_nametoindex(const char *ifname)
{
	_glidix_netstat info;
	if (_glidix_netconf_stat(ifname, &info, sizeof(_glidix_netstat)) == -1)
	{
		errno = ENXIO;
		return 0;
	};
	
	return info.scopeID;
};

char* if_indextoname(uint32_t scope, char *buffer)
{
	int i;
	_glidix_netstat info;
	
	for (i=0;;i++)
	{
		if (_glidix_netconf_statidx(i, &info, sizeof(_glidix_netstat)) == -1)
		{
			errno = ENXIO;
			return NULL;
		};
		
		if (info.scopeID == scope)
		{
			strcpy(buffer, info.ifname);
			return buffer;
		};
	};
};

struct if_nameindex *if_nameindex()
{
	struct if_nameindex *list = NULL;
	size_t num = 0;
	
	int i;
	_glidix_netstat info;
	
	for (i=0;;i++)
	{
		if (_glidix_netconf_statidx(i, &info, sizeof(_glidix_netstat)) == -1)
		{
			size_t idx = num++;
			list = (struct if_nameindex*) realloc(list, sizeof(struct if_nameindex)*num);
			list[idx].if_name = NULL;
			list[idx].if_index = 0;
			return list;
		};
		
		size_t idx = num++;
		list = (struct if_nameindex*) realloc(list, sizeof(struct if_nameindex)*num);
		strcpy(list[idx].__if_namebuf, info.ifname);
		list[idx].if_index = info.scopeID;
		list[idx].if_name = list[idx].__if_namebuf;
	};
};

void if_freenameindex(struct if_nameindex *list)
{
	free(list);
};
