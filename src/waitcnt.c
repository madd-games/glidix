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

#include <glidix/waitcnt.h>

void wcInit(WaitCounter *wc)
{
	spinlockRelease(&wc->lock);
	wc->server = NULL;
	wc->count = 0;
};

void wcUp(WaitCounter *wc)
{
	// we must disable interrupts to make sure the locking is interrupt-safe.
	ASM("cli");
	spinlockAcquire(&wc->lock);
	wc->count++;
	if (wc->server != NULL) /*wc->server->flags &= ~THREAD_WAITING;*/ signalThread(wc->server);
	spinlockRelease(&wc->lock);
	ASM("sti");
};

void wcDown(WaitCounter *wc)
{
	ASM("cli");
	spinlockAcquire(&wc->lock);
	wc->server = getCurrentThread();

	while (wc->count == 0)
	{
		//getCurrentThread()->flags |= THREAD_WAITING;
		waitThread(getCurrentThread());
		spinlockRelease(&wc->lock);
		kyield();
		ASM("cli");
		spinlockAcquire(&wc->lock);
	};

	wc->count--;
	spinlockRelease(&wc->lock);
	ASM("sti");
};
