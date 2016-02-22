/*
	Glidix GUI

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

#include <sys/glidix.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <pthread.h>
#include "gui.h"

static int guiPid;
static int guiFD;
static int queueFD;
static volatile int initFinished = 0;
static uint64_t nextWindowID;
static uint64_t nextSeq;
static pthread_t listenThread;

typedef struct GWMWaiter_
{
	struct GWMWaiter_*		prev;
	struct GWMWaiter_*		next;
	uint64_t			seq;
	GWMMessage			resp;
	pthread_spinlock_t		lock;
} GWMWaiter;

static pthread_spinlock_t waiterLock;
static GWMWaiter* waiters = NULL;

static void gwmPostWaiter(uint64_t seq, GWMMessage *resp, const GWMCommand *cmd)
{
	GWMWaiter *waiter = (GWMWaiter*) malloc(sizeof(GWMWaiter));
	waiter->prev = NULL;
	waiter->next = NULL;
	waiter->seq = seq;
	pthread_spin_lock(&waiter->lock);
	
	pthread_spin_lock(&waiterLock);
	if (_glidix_mqsend(queueFD, guiPid, guiFD, cmd, sizeof(GWMCommand)) != 0)
	{
		perror("_glidix_mqsend");
	};
	waiter->next = waiters;
	waiter->prev = NULL;
	waiters = waiter;
	pthread_spin_unlock(&waiterLock);
	
	// wait for response
	pthread_spin_lock(&waiter->lock);
	memcpy(resp, &waiter->resp, sizeof(GWMMessage));
	free(waiter);
};

static void* listenThreadFunc(void *ignore)
{
	(void)ignore;
	queueFD = _glidix_mqclient(guiPid, guiFD);
	char msgbuf[65536];
	
	initFinished = 1;
	while (1)
	{
		_glidix_msginfo info;
		ssize_t size = _glidix_mqrecv(queueFD, &info, msgbuf, 65536);
		if (size == -1) continue;

		if (info.type == _GLIDIX_MQ_INCOMING)
		{
			if (size < sizeof(GWMMessage))
			{
				continue;
			};
		
			GWMMessage *msg = (GWMMessage*) msgbuf;
			pthread_spin_lock(&waiterLock);
			GWMWaiter *waiter;
			for (waiter=waiters; waiter!=NULL; waiter=waiter->next)
			{
				if (waiter->seq == msg->generic.seq)
				{
					memcpy(&waiter->resp, msg, sizeof(GWMWaiter));
					if (waiter->prev != NULL) waiter->prev->next = waiter->next;
					if (waiter->next != NULL) waiter->next->prev = waiter->prev;
					if (waiter->prev == NULL) waiters = waiter->next;
					pthread_spin_unlock(&waiter->lock);
					break;
				};
			};
			pthread_spin_unlock(&waiterLock);
		};
	};
};

int gwmInit()
{
	FILE *fp = fopen("/usr/share/gui.pid", "rb");
	if (fp == NULL) return -1;
	if (fscanf(fp, "%d.%d", &guiPid, &guiFD) != 2)
	{
		return -1;
	};
	
	nextWindowID = 1;
	nextSeq = 1;
	
	if (pthread_create(&listenThread, NULL, listenThreadFunc, NULL) != 0)
	{
		return -1;
	};
	
	while (!initFinished) __sync_synchronize();
	return 0;
};

GWMWindowHandle gwmCreateWindow(
	GWMWindowHandle parent,
	const char *caption,
	unsigned int x, unsigned int y,
	unsigned int width, unsigned int height,
	int flags)
{
	uint64_t id = __sync_fetch_and_add(&nextWindowID, 1);
	uint64_t seq = __sync_fetch_and_add(&nextSeq, 1);
	
	GWMCommand cmd;
	cmd.createWindow.cmd = GWM_CMD_CREATE_WINDOW;
	cmd.createWindow.id = id;
	cmd.createWindow.parent = (uint64_t) parent;
	strcpy(cmd.createWindow.pars.caption, caption);
	strcpy(cmd.createWindow.pars.iconName, caption);
	cmd.createWindow.pars.flags = flags;
	cmd.createWindow.pars.width = width;
	cmd.createWindow.pars.height = height;
	cmd.createWindow.pars.x = x;
	cmd.createWindow.pars.y = y;
	cmd.createWindow.seq = seq;
	
	GWMMessage resp;
	gwmPostWaiter(seq, &resp, &cmd);
	
	if (resp.createWindowResp.status == 0)
	{
		return (GWMWindowHandle) id;
	};
	
	return NULL;
};
