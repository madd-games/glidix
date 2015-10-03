/*
	Glidix kernel
	
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

#include <glidix/ipreasm.h>
#include <glidix/netif.h>
#include <glidix/string.h>
#include <glidix/time.h>
#include <glidix/memory.h>
#include <glidix/sched.h>
#include <glidix/console.h>
#include <glidix/socket.h>

static IPFragmentList *firstFragList = NULL;
static IPFragmentList *lastFragList = NULL;
static Spinlock reasmLock;
static Thread *reasmHandle;

static int isFragmentListComplete(IPFragmentList *list)
{
	uint32_t nextOffset = 0;
	
	IPFragment *frag;
	for (frag=list->fragList; frag!=NULL; frag=frag->next)
	{
		if (frag->fragOffset != nextOffset)
		{
			// hole
			return 0;
		};
		
		nextOffset += frag->fragSize;
		if (frag->lastFragment)
		{
			if (frag->next != NULL)
			{
				// "last fragment" is not last; render the list invalid
				list->deadlineTicks = 1;
				return 0;
			};
			
			// we have no holes
			return 1;
		};
	};
	
	return 0;
};

static size_t calculatePacketSize(IPFragmentList *list)
{
	size_t size = 0;
	
	IPFragment *frag;
	for (frag=list->fragList; frag!=NULL; frag=frag->next)
	{
		size += frag->fragSize;
	};
	
	return size;
};

static void ipreasmThread(void *ignore)
{
	// notice that we start in the waiting state, so as soon as we wake up,
	// scan the lists
	
	while (1)
	{
		spinlockAcquire(&reasmLock);
		//kprintf("REASSEMBLER TRIGGERED\n");
		
		if (firstFragList == NULL)
		{
			ASM("cli");
			reasmHandle->wakeTime = 0;
			reasmHandle->flags |= THREAD_WAITING;
			kyield();
		};
		
		IPFragmentList *list = firstFragList;
		int earliestDeadline = firstFragList->deadlineTicks;
		reasmHandle->wakeTime = 0;
		
		while (list != NULL)
		{
			if (getTicks() >= list->deadlineTicks)
			{
				// passed the deadline, just delete all fragments
				kprintf_debug("DEADLINE PASSED!\n");
				IPFragment *frag = list->fragList;
				while (frag != NULL)
				{
					kprintf("FRAGMENT: offset=%d, size=%d, last=%d\n", (int) frag->fragOffset, (int) frag->fragSize, frag->lastFragment);
					IPFragment *next = frag->next;
					kfree(frag);
					frag = next;
				};
				
				if (list == firstFragList)
				{
					firstFragList = list->next;
				};
				
				if (list == lastFragList)
				{
					lastFragList = list->prev;
				};
				
				if (list->prev != NULL) list->prev->next = list->next;
				if (list->next != NULL) list->next->prev = list->prev;
				IPFragmentList *nextList = list->next;
				kfree(list);
				list = nextList;
			}
			else
			{
				if (isFragmentListComplete(list))
				{
					size_t packetSize = calculatePacketSize(list);
					char *buffer = (char*) kmalloc(packetSize);
					
					IPFragment *frag = list->fragList;
					while (frag != NULL)
					{
						memcpy(&buffer[frag->fragOffset], frag->data, frag->fragSize);
						IPFragment *next = frag->next;
						kfree(frag);
						frag = next;
					};
					
					if (list->family == AF_INET)
					{
						struct sockaddr_in src;
						memset(&src, 0, sizeof(struct sockaddr_in));
						src.sin_family = AF_INET;
						memcpy(&src.sin_addr, list->srcaddr, 4);
						
						struct sockaddr_in dest;
						memset(&dest, 0, sizeof(struct sockaddr_in));
						dest.sin_family = AF_INET;
						memcpy(&dest.sin_addr, list->dstaddr, 4);
						
						onTransportPacket((struct sockaddr*)&src, (struct sockaddr*)&dest,
									sizeof(struct sockaddr_in), buffer, packetSize, list->proto);
					}
					else
					{
						struct sockaddr_in6 src;
						memset(&src, 0, sizeof(struct sockaddr_in6));
						src.sin6_family = AF_INET6;
						memcpy(&src.sin6_addr, list->srcaddr, 16);
						
						struct sockaddr_in6 dest;
						memset(&dest, 0, sizeof(struct sockaddr_in6));
						dest.sin6_family = AF_INET6;
						memcpy(&dest.sin6_addr, list->dstaddr, 16);
						
						onTransportPacket((struct sockaddr*) &src, (struct sockaddr*) &dest,
									sizeof(struct sockaddr_in6), buffer, packetSize, list->proto);
					};
					
					if (list == firstFragList)
					{
						firstFragList = list->next;
					};
				
					if (list == lastFragList)
					{
						lastFragList = list->prev;
					};
					
					if (list->prev != NULL) list->prev->next = list->next;
					if (list->next != NULL) list->next->prev = list->prev;
					IPFragmentList *nextList = list->next;
					kfree(list);
					list = nextList;
					
					kprintf_debug("IPREASM: reassembled fragmented packet of size %d\n", (int) packetSize);
					kfree(buffer);
				}
				else
				{
					if (list->deadlineTicks < earliestDeadline)
					{
						earliestDeadline = list->deadlineTicks;
					};
					
					list = list->next;
				};
			};
		};
		
		ASM("cli");
		reasmHandle->wakeTime = earliestDeadline;
		reasmHandle->flags |= THREAD_WAITING;
		spinlockRelease(&reasmLock);
		kyield();
	};
};

void ipreasmInit()
{
	spinlockRelease(&reasmLock);
	
	KernelThreadParams params;
	memset(&params, 0, sizeof(KernelThreadParams));
	params.stackSize = DEFAULT_STACK_SIZE;
	params.name = "IP Reassembler";
	params.flags = THREAD_WAITING;
	reasmHandle = CreateKernelThread(ipreasmThread, &params, NULL);
};

static int isMatchingFragmentList(IPFragmentList *list, int family, char *srcaddr, char *dstaddr, uint32_t id)
{
	if (memcmp(list->srcaddr, srcaddr, 16) != 0)
	{
		return 0;
	};
	
	if (memcmp(list->dstaddr, dstaddr, 16) != 0)
	{
		return 0;
	};
	
	return (list->id == id) && (list->family == family);
};

void ipreasmPass(int family, char *srcaddr, char *dstaddr, int proto, uint32_t id, uint32_t fragOff, char *data, size_t fragSize, int moreFrags)
{
	spinlockAcquire(&reasmLock);
	
	int found = 0;
	
	IPFragmentList *list;
	for (list=firstFragList; list!=NULL; list=list->next)
	{
		if (isMatchingFragmentList(list, family, srcaddr, dstaddr, id))
		{
			if (list->proto != proto)
			{
				// ignore this packet because it's faulty
				kprintf_debug("OVERLAP\n");
				spinlockRelease(&reasmLock);
				return;
			};
			
			// see if this fragment is supposed to come before any that have so far arrived
			if (list->fragList->fragOffset > fragOff)
			{
				if (fragOff+fragSize > list->fragList->fragOffset)
				{
					// overlaps, reject
					kprintf_debug("OVERLAP\n");
					spinlockRelease(&reasmLock);
					return;
				};
				
				IPFragment *frag = NEW_EX(IPFragment, fragSize);
				frag->lastFragment = !moreFrags;
				frag->fragOffset = fragOff;
				frag->fragSize = fragSize;
				frag->prev = NULL;
				list->fragList->prev = frag;
				frag->next = list->fragList;
				memcpy(frag->data, data, fragSize);
				list->fragList = frag;
			}
			else
			{
				//kprintf_debug("here though\n");
				// find where to place this fragment
				IPFragment *scan = list->fragList;
				while (scan != NULL)
				{
					if (scan->fragOffset == fragOff)
					{
						// duplicate; reject
						kprintf_debug("OVERLAP\n");
						spinlockRelease(&reasmLock);
						return;
					};
				
					int doit = 0;
					if (scan->next == NULL)
					{
						if (fragOff > scan->fragOffset)
						{
							if (scan->fragOffset+scan->fragSize > fragOff)
							{
								// overlaps
								kprintf_debug("OVERLAP\n");
								spinlockRelease(&reasmLock);
								return;
							};
						
							doit = 1;
						};
					}
					else
					{
						if ((scan->fragOffset < fragOff) && (scan->next->fragOffset > fragOff))
						{
							// reject this packet if it is overlapping
							if (scan->fragOffset+scan->fragSize > fragOff)
							{
								// overlaps previous fragment
								kprintf_debug("OVERLAP\n");
								spinlockRelease(&reasmLock);
								return;
							};
						
							if (fragOff+fragSize > scan->next->fragOffset)
							{
								// overlaps next fragment
								kprintf_debug("OVERLAP\n");
								spinlockRelease(&reasmLock);
								return;
							};
						
							doit = 1;
						};
					};
				
					if (doit)
					{
						// OK, we can finally place the fragment
						IPFragment *frag = NEW_EX(IPFragment, fragSize);
						frag->lastFragment = !moreFrags;
						frag->fragOffset = fragOff;
						frag->fragSize = fragSize;
						frag->prev = scan;
						frag->next = scan->next;
						if (scan->next != NULL) scan->next->prev = frag;
						scan->next = frag;
						memcpy(frag->data, data, fragSize);
						break;
					}
					else
					{
						scan = scan->next;
					};
				};
			};
			
			found = 1;
		};
	};
	
	if (!found)
	{
		// create a new fragment list
		IPFragment *frag = NEW_EX(IPFragment, fragSize);
		frag->lastFragment = !moreFrags;
		frag->fragOffset = fragOff;
		frag->fragSize = fragSize;
		frag->prev = frag->next = NULL;
		memcpy(frag->data, data, fragSize);
		
		IPFragmentList *list = NEW(IPFragmentList);
		list->family = family;
		list->deadlineTicks = getTicks() + 60000;		// deadline after 60 seconds
		memcpy(list->srcaddr, srcaddr, 16);
		memcpy(list->dstaddr, dstaddr, 16);
		list->id = id;
		list->proto = proto;
		list->fragList = frag;
		
		if (firstFragList == NULL)
		{
			firstFragList = lastFragList = list;
			list->prev = list->next = NULL;
		}
		else
		{
			lastFragList->next = list;
			list->next = NULL;
			list->prev = lastFragList;
			lastFragList = list;
		};
	};
	
	signalThread(reasmHandle);
	spinlockRelease(&reasmLock);
};
