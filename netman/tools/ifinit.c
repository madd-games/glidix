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

#include <sys/stat.h>
#include <sys/wait.h>
#include <stdio.h>
#include <stdlib.h>
#include <fcntl.h>
#include <unistd.h>
#include <string.h>
#include <errno.h>
#include <dlfcn.h>
#include <gxnetman.h>

const char *progName;

void bringUp(const char *ifname, int pass);

void doAutoConf(const char *ifname)
{
	_glidix_netstat info;
	if (_glidix_netconf_stat(ifname, &info, sizeof(_glidix_netstat)) == -1)
	{
		fprintf(stderr, "%s: cannot autoconfigure %s: interface disappeared unexpectedly\n", progName, ifname);
		return;
	};
	
	FILE *fp = fopen("/etc/if.conf", "a");
	if (fp == NULL)
	{
		fprintf(stderr, "%s: cannot append to /etc/if.conf: %s\n", progName, strerror(errno));
		return;
	};
	
	if (info.ifconfig.type == IF_ETHERNET)
	{
		fprintf(fp, "\n");
		fprintf(fp, "interface %s ipv4 dhcp\n", ifname);
		fprintf(fp, "interface %s ipv4 alic\n", ifname);
		fprintf(fp, "interface %s ipv6 slaac\n", ifname);
		bringUp(ifname, 1);
	}
	else
	{
		fprintf(stderr, "%s: cannot autoconfigure %s: unknown interface type %d\n", progName, ifname, info.ifconfig.type);
		fclose(fp);
	};
};

void bringUp(const char *ifname, int pass)
{
	pid_t pid = fork();
	if (pid == -1)
	{
		fprintf(stderr, "%s: cannot bring up %s: fork: %s\n", progName, ifname, strerror(errno));
		return;
	}
	else if (pid == 0)
	{
		execl("/usr/bin/ifup", "ifup", ifname, NULL);
		fprintf(stderr, "%s: cannot bring up %s: exec ifup: %s\n", progName, ifname, strerror(errno));
		exit(1);
	}
	else
	{
		int status;
		waitpid(pid, &status, 0);
		
		if (WIFEXITED(status))
		{
			if (WEXITSTATUS(status) == 2)
			{
				if (pass == 0)
				{
					fprintf(stderr, "%s: interface %s not configured: attempting defualt configuration\n",
						progName, ifname);
					
					doAutoConf(ifname);
				};
			};
		};
	};
};

int main(int argc, char *argv[])
{
	progName = argv[0];
	
	if (geteuid() != 0)
	{
		fprintf(stderr, "%s: you must be root", argv[0]);
		return 1;
	};
		
	// ensure that any necessary directories exist (ignore errors if they already exist)
	mkdir("/etc", 0755);
	mkdir("/etc/dns", 0755);
	mkdir("/etc/dns/ipv6", 0755);
	mkdir("/etc/dns/ipv4", 0755);
	mkdir("/var", 0755);
	mkdir("/var/log", 0755);
	mkdir("/var/log/netman", 0755);
	mkdir("/run", 0755);
	mkdir("/run/netman", 0755);
	
	// make sure that /etc/hosts exists
	struct stat st;
	if (stat("/etc/hosts", &st) != 0)
	{
		if (errno == ENOENT)
		{
			fprintf(stderr, "%s: /etc/hosts doesn't exist: attempting creation\n", argv[0]);
			
			FILE *fp = fopen("/etc/hosts", "w");
			if (fp == NULL)
			{
				fprintf(stderr, "%s: cannot create /etc/hosts: %s\n", argv[0], strerror(errno));
				return 1;
			};
			
			fprintf(fp, "# Standard IPv4 hostnames\n");
			fprintf(fp, "127.0.0.1	localhost\n");		// the one tab less NEEDS to be here
			fprintf(fp, "0.0.0.0		*\n");
			fprintf(fp, "255.255.255.255	ip4-broadcast\n\n");
			
			fprintf(fp, "# Standard IPv6 hostnames\n");
			fprintf(fp, "::1		ip6-localhost ip6-loopback\n");
			fprintf(fp, "fe00::		ip6-localnet\n");
			fprintf(fp, "ff00::		ip6-mcastprefix\n");
			fprintf(fp, "ff02::1		ip6-allnodes\n");
			fprintf(fp, "ff02::2		ip6-allrouters\n");
			fclose(fp);
			
			fprintf(stderr, "%s: created /etc/hosts with defualt settings\n", argv[0]);
		}
		else
		{
			fprintf(stderr, "%s: failed to stat /etc/hosts: %s\n", argv[0], strerror(errno));
			return 1;
		};
	};
	
	// ensure that /etc/if.conf exists before trying any "ifup"s
	if (stat("/etc/if.conf", &st) != 0)
	{
		if (errno == ENOENT)
		{
			fprintf(stderr, "%s: /etc/if.conf doesn't exist: attempting creation\n", argv[0]);
			
			FILE *fp = fopen("/etc/if.conf", "w");
			if (fp == NULL)
			{
				fprintf(stderr, "%s: cannot create /etc/if.conf: %s\n", argv[0], strerror(errno));
				return 1;
			};
			
			fprintf(fp, "# Network interface configuration used by ifup(1) and ifdown(1).\n");
			fprintf(fp, "# See if.conf(3) for more information.\n");
			fclose(fp);
			
			fprintf(stderr, "%s: /etc/if.conf created successfully\n", argv[0]);
		}
		else
		{
			fprintf(stderr, "%s: failed to stat /etc/if.conf: %s\n", argv[0], strerror(errno));
			return 1;
		};
	};
	
	// now we bring up all the interfaces
	int i;
	_glidix_netstat info;
	for (i=0;;i++)
	{
		if (_glidix_netconf_statidx(i, &info, sizeof(_glidix_netstat)) == -1)
		{
			break;
		};
		
		if (strcmp(info.ifname, "lo") != 0)
		{
			// it's not the loopback interface, bring it up
			bringUp(info.ifname, 0);
		};
	};
	
	return 0;
};
