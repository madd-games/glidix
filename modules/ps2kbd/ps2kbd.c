/*
	Glidix kernel

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

#include <glidix/module/module.h>
#include <glidix/display/console.h>
#include <glidix/hw/idt.h>
#include <glidix/util/common.h>
#include <glidix/hw/port.h>
#include <glidix/thread/sched.h>
#include <glidix/term/ktty.h>
#include <glidix/thread/waitcnt.h>
#include <glidix/term/term.h>
#include <glidix/util/string.h>
#include <glidix/humin/humin.h>
#include <glidix/humin/ptr.h>

#include "scancodes.h"

#define	MOUSE_BL			(1 << 0)
#define	MOUSE_BR			(1 << 1)
#define	MOUSE_BM			(1 << 2)
#define	MOUSE_XS			(1 << 4)
#define	MOUSE_YS			(1 << 5)
#define	MOUSE_XO			(1 << 6)
#define	MOUSE_YO			(1 << 7)

#define	MOUSE_BUTTONS			(MOUSE_BL | MOUSE_BR | MOUSE_BM)

static IRQHandler oldHandler;
static IRQHandler oldHandler12;
static HuminDevice *hups;

static WaitCounter wcKeyboard = WC_INIT;

static void onKeyboardIRQ(int irq)
{
	wcUp(&wcKeyboard);
};

static void onMouseIRQ(int irq)
{
	wcUp(&wcKeyboard);
};

static uint8_t mouseStateWas = 0;

static void kbdThread(void *data)
{
	thnice(NICE_UIN);
	
	while (1)
	{
		wcDown(&wcKeyboard);
		uint8_t status = inb(0x64);
		if ((status & 1) == 0)
		{
			// sporious interrupt?
			continue;
		};
		
		uint8_t code = inb(0x60);
		if (status & 0x20)
		{
			uint8_t flags = code;
			uint8_t xmot = inb(0x60);
			uint8_t ymot = inb(0x60);
			
			if ((xmot != 0) || (ymot != 0))
			{
				// motion
				int relx = (int) xmot;
				int rely = (int) ymot;
				
				if (flags & MOUSE_XS)
				{
					relx = relx - 256;
				};
				
				if (flags & MOUSE_YS)
				{
					rely = rely - 256;
				};
				
				rely = -rely;
				ptrRelMotion(relx, rely);
			};
			
			// check each of the buttons
			HuminEvent ev;
			memset(&ev, 0, sizeof(HuminEvent));
			
			uint8_t buttons = flags & MOUSE_BUTTONS;
			if (mouseStateWas != buttons)
			{
				if ((buttons & MOUSE_BL) != (mouseStateWas & MOUSE_BL))
				{
					if (buttons & MOUSE_BL) ev.button.type = HUMIN_EV_BUTTON_DOWN;
					else ev.button.type = HUMIN_EV_BUTTON_UP;
					ev.button.scancode = HUMIN_SC_MOUSE_LEFT;
					ev.button.keycode = HUMIN_KC_MOUSE_LEFT;
					huminPostEvent(hups, &ev);
				};

				if ((buttons & MOUSE_BM) != (mouseStateWas & MOUSE_BM))
				{
					if (buttons & MOUSE_BM) ev.button.type = HUMIN_EV_BUTTON_DOWN;
					else ev.button.type = HUMIN_EV_BUTTON_UP;
					ev.button.scancode = HUMIN_SC_MOUSE_MIDDLE;
					ev.button.keycode = HUMIN_KC_MOUSE_MIDDLE;
					huminPostEvent(hups, &ev);
				};

				if ((buttons & MOUSE_BR) != (mouseStateWas & MOUSE_BR))
				{
					if (buttons & MOUSE_BR) ev.button.type = HUMIN_EV_BUTTON_DOWN;
					else ev.button.type = HUMIN_EV_BUTTON_UP;
					ev.button.scancode = HUMIN_SC_MOUSE_RIGHT;
					ev.button.keycode = HUMIN_KC_MOUSE_RIGHT;
					huminPostEvent(hups, &ev);
				};
			};
			mouseStateWas = buttons;
			
			continue;
		};
		
		if (code == 0xE0)
		{
			// extension
			scancodeGotoExt();
		}
		else
		{
			// report it as an event for the humin interface.
			HuminEvent ev;
			memset(&ev, 0, sizeof(HuminEvent));
			ev.button.type = HUMIN_EV_BUTTON_DOWN;
			if (code & 0x80) ev.button.type = HUMIN_EV_BUTTON_UP;
			ev.button.scancode = scancodeTransform(code & 0x7F);
			ev.button.keycode = scancodeTranslate(code);
			huminPostEvent(hups, &ev);
		};
	};
};

void mouse_wait(uint8_t a_type) //unsigned char
{
	uint32_t _time_out=100000; //unsigned int
	if(a_type==0)
	{
		while(_time_out--) //Data
		{
			if((inb(0x64) & 1)==1)
			{
				return;
			}
		}
		return;
	}
	else
	{
		while(_time_out--) //Signal
		{
			if((inb(0x64) & 2)==0)
			{
				return;
			}
		}
		return;
	}
}

void mouse_write(uint8_t a_write) //unsigned char
{
	//Wait to be able to send a command
	mouse_wait(1);
	//Tell the mouse we are sending a command
	outb(0x64, 0xD4);
	//Wait for the final part
	mouse_wait(1);
	//Finally write
	outb(0x60, a_write);
}

uint8_t mouse_read()
{
	//Get response from mouse
	mouse_wait(0); 
	return inb(0x60);
}

void mouse_install()
{
	uint8_t _status;	//unsigned char

	//Enable the auxiliary mouse device
	mouse_wait(1);
	outb(0x64, 0xA8);
	
	//Enable the interrupts
	mouse_wait(1);
	outb(0x64, 0x20);
	mouse_wait(0);
	_status=(inb(0x60) | 2);
	mouse_wait(1);
	outb(0x64, 0x60);
	mouse_wait(1);
	outb(0x60, _status);
	
	//Tell the mouse to use default settings
	mouse_write(0xF6);
	mouse_read();	//Acknowledge
	
	//Enable the mouse
	mouse_write(0xF4);
	mouse_read();	//Acknowledge
};

MODULE_INIT()
{
	kprintf("ps2kbd: initializing the PS/2 keyboard/mouse\n");
	oldHandler = registerIRQHandler(1, onKeyboardIRQ);
	oldHandler12 = registerIRQHandler(12, onMouseIRQ);
	hups = huminCreateDevice("PS/2 Keyboard and Mouse");
	
	// initialize the mouse
	mouse_install();

	// init mouse
	mouse_write(0xF6);
	mouse_read();
	mouse_write(0xF4);
	mouse_read();
	
	int extracted = 0;
	while (inb(0x64) & 1)
	{
		if (extracted == 512)
		{
			return 0;
		};

		extracted++;
		inb(0x60);
	};

	KernelThreadParams kbdPars;
	memset(&kbdPars, 0, sizeof(KernelThreadParams));
	kbdPars.stackSize = 0x4000;
	kbdPars.name = "PS/2 keyboard driver";
	CreateKernelThread(kbdThread, &kbdPars, NULL);
	return 0;
};

MODULE_FINI()
{
	kprintf("ps2kbd: shutting down PS/2 keyboard\n");
	registerIRQHandler(1, oldHandler);
	registerIRQHandler(12, oldHandler12);
	huminDeleteDevice(hups);
	return 0;
};
