/*
	Glidix GUI

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

#include <libddi.h>
#include <libgwm.h>
#include <stdio.h>
#include <assert.h>

enum
{
	SYM_BUTTON1 = GWM_SYM_USER,
	SYM_BUTTON2,
	SYM_BUTTON3,
	
	SYM_CHECKBOX,
	SYM_CB_MASK,
	
	SYM_BUTTON_FRAME1,
	SYM_BUTTON_FRAME2,
};

enum
{
	COL_A,
	COL_B,
	COL_C
};

GWMWindow *txtfield;
GWMWindow *cbMask;

int myCommandHandler(GWMCommandEvent *ev)
{
	switch (ev->symbol)
	{
	case SYM_BUTTON2:
		gwmMessageBox(NULL, "Example", "You clicked button 2!", GWM_MBUT_OK);
		return GWM_EVSTATUS_CONT;
	case SYM_CHECKBOX:
		if (gwmMessageBox(NULL, "Example", "Are you sure you want to flip this checkbox?", GWM_MBUT_YESNO | GWM_MBICON_QUEST) == GWM_SYM_YES)
		{
			return GWM_EVSTATUS_CONT;
		};
		return GWM_EVSTATUS_OK;
	case SYM_BUTTON_FRAME2:
		gwmMessageBox(NULL, "Example", gwmReadTextField(txtfield), GWM_MBUT_OK | GWM_MBICON_WARN);
		return GWM_EVSTATUS_CONT;
	default:
		return GWM_EVSTATUS_CONT;
	};
};

int myToggledHandler(GWMCommandEvent *ev)
{
	switch (ev->symbol)
	{
	case SYM_CB_MASK:
		if (gwmGetCheckboxState(cbMask))
		{
			gwmSetTextFieldFlags(txtfield, GWM_TXT_MASKED);
		}
		else
		{
			gwmSetTextFieldFlags(txtfield, 0);
		};
		return GWM_EVSTATUS_CONT;
	default:
		return GWM_EVSTATUS_CONT;
	};
};

int myHandler(GWMEvent *ev, GWMWindow *win, void *context)
{
	DDISurface *canvas = gwmGetWindowCanvas(win);
	static DDIColor blue = {0x00, 0x00, 0xFF, 0xFF};
	
	GWMDataEvent *dataEvent = (GWMDataEvent*) ev;
	
	switch (ev->type)
	{
	case GWM_EVENT_COMMAND:
		return myCommandHandler((GWMCommandEvent*) ev);
	case GWM_EVENT_TOGGLED:
		return myToggledHandler((GWMCommandEvent*) ev);
	case GWM_EVENT_UP:
		if (ev->keycode == GWM_KC_MOUSE_LEFT)
		{
			printf("left click\n");
		};
		return GWM_EVSTATUS_CONT;
	case GWM_EVENT_DOUBLECLICK:
		printf("double-click\n");
		return GWM_EVSTATUS_CONT;
	case GWM_EVENT_RESIZED:
		ddiFillRect(canvas, 0, 0, canvas->width, canvas->height, &blue);
		return GWM_EVSTATUS_OK;
	case GWM_EVENT_DATA_EXPANDING:
		printf("Expanding node `%s' (B=`%s')\n", gwmGetDataString(win, dataEvent->node, COL_A), gwmGetDataString(win, dataEvent->node, COL_B));
		return GWM_EVSTATUS_CONT;
	case GWM_EVENT_DATA_COLLAPSING:
		printf("Collapsing node `%s' (B=`%s')\n", gwmGetDataString(win, dataEvent->node, COL_A), gwmGetDataString(win, dataEvent->node, COL_B));
		return GWM_EVSTATUS_CONT;
	case GWM_EVENT_DATA_ACTIVATED:
		printf("Activated node `%s' (B=`%s')\n", gwmGetDataString(win, dataEvent->node, COL_A), gwmGetDataString(win, dataEvent->node, COL_B));
		return GWM_EVSTATUS_CONT;
	default:
		return GWM_EVSTATUS_CONT;
	};
};

int main()
{
	if (gwmInit() != 0)
	{
		fprintf(stderr, "Failed to initialize GWM!\n");
		return 1;
	};
	
	GWMWindow *topWindow = gwmCreateWindow(
		NULL,						// window parent (none)
		"Tutorial Part 3",				// window caption
		GWM_POS_UNSPEC, GWM_POS_UNSPEC,			// window coordinates unspecified
		0, 0,						// window size (will be set later by gwmFit() )
		GWM_WINDOW_HIDDEN | GWM_WINDOW_NOTASKBAR	// hidden, no taskbar icon
	);
	assert(topWindow != NULL);
	
	gwmPushEventHandler(topWindow, myHandler, NULL);
	
	GWMLayout *boxLayout = gwmCreateBoxLayout(GWM_BOX_VERTICAL);
	gwmSetWindowLayout(topWindow, boxLayout);
	
	GWMDataCtrl *ctrl = gwmNewDataCtrl(topWindow);
	gwmBoxLayoutAddWindow(boxLayout, ctrl, 1, 0, GWM_BOX_FILL);
	
	gwmSetDataFlags(ctrl, GWM_DATA_SHOW_HEADERS);
	
	gwmAddDataColumn(ctrl, "Column A", GWM_DATA_STRING, COL_A);
	gwmAddDataColumn(ctrl, "Column B", GWM_DATA_STRING, COL_B);
	gwmAddDataColumn(ctrl, "Column C", GWM_DATA_STRING, COL_C);
	
	GWMDataNode *nodeTest = gwmAddDataNode(ctrl, GWM_DATA_ADD_BOTTOM_CHILD, NULL);
	gwmSetDataString(ctrl, nodeTest, COL_A, "Hello");
	gwmSetDataString(ctrl, nodeTest, COL_B, "world");
	
	GWMDataNode *nodeApple = gwmAddDataNode(ctrl, GWM_DATA_ADD_BOTTOM_CHILD, nodeTest);
	gwmSetDataString(ctrl, nodeApple, COL_A, "Apple");
	
	GWMDataNode *nodeOrange = gwmAddDataNode(ctrl, GWM_DATA_ADD_BOTTOM_CHILD, nodeTest);
	gwmSetDataString(ctrl, nodeOrange, COL_A, "Orange");
	
	GWMDataNode *nodeStrawberry = gwmAddDataNode(ctrl, GWM_DATA_ADD_BOTTOM_CHILD, nodeTest);
	gwmSetDataString(ctrl, nodeStrawberry, COL_A, "Strawberry");
	
	GWMDataNode *nodeSecond = gwmAddDataNode(ctrl, GWM_DATA_ADD_BOTTOM_CHILD, NULL);
	gwmSetDataString(ctrl, nodeSecond, COL_B, "Second test");
	
	gwmLayout(topWindow, 300, 300);
	gwmSetWindowFlags(topWindow, GWM_WINDOW_MKFOCUSED | GWM_WINDOW_RESIZEABLE);

	DDISurface *canvas = gwmGetWindowCanvas(topWindow);
	DDISurface *icon = ddiLoadAndConvertPNG(&canvas->format, "/usr/share/images/calc.png", NULL);
	
	int i, j, k;
	for (i=0; i<30; i++)
	{
		GWMDataNode *inode = gwmAddDataNode(ctrl, GWM_DATA_ADD_BOTTOM_CHILD, NULL);
		
		char ibuf[16];
		sprintf(ibuf, "%d", i);
		
		gwmSetDataString(ctrl, inode, COL_A, ibuf);
		gwmSetDataNodeIcon(ctrl, inode, icon);
		
		for (j=0; j<5; j++)
		{
			GWMDataNode *jnode = gwmAddDataNode(ctrl, GWM_DATA_ADD_BOTTOM_CHILD, inode);
			
			char jbuf[16];
			sprintf(jbuf, "%d", j);
			
			gwmSetDataString(ctrl, jnode, COL_A, jbuf);
			gwmSetDataNodeIcon(ctrl, jnode, icon);
			
			for (k=0; k<10; k++)
			{
				GWMDataNode *knode = gwmAddDataNode(ctrl, GWM_DATA_ADD_BOTTOM_CHILD, jnode);
				
				char kbuf[16];
				sprintf(kbuf, "%d", k);
				
				gwmSetDataString(ctrl, knode, COL_A, kbuf);
				gwmSetDataNodeIcon(ctrl, knode, icon);
			};
		};
	};
	
	gwmMainLoop();
	gwmQuit();
	return 0;
};
