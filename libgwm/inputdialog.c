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

#include <libgwm.h>
#include <stdlib.h>
#include <stdio.h>
#include <string.h>

static int inputDialogHandler(GWMEvent *ev, GWMWindow *window, void *context)
{
	int *answerPtr = (int*) context;
	GWMCommandEvent *cmd = (GWMCommandEvent*) ev;
	
	switch (ev->type)
	{
	case GWM_EVENT_COMMAND:
		*answerPtr = cmd->symbol;
		return GWM_EVSTATUS_BREAK;
	default:
		return GWM_EVSTATUS_CONT;
	};
};

char* gwmGetInput(GWMWindow *parent, const char *caption, const char *prompt, const char *initialText)
{
	GWMModal *modal = gwmNewModal(parent);
	gwmSetWindowCaption(modal, caption);
	
	GWMLayout *mainBox = gwmCreateBoxLayout(GWM_BOX_VERTICAL);
	gwmSetWindowLayout(modal, mainBox);
	
	GWMLabel *label = gwmNewLabel(modal);
	gwmSetLabelText(label, prompt);
	gwmBoxLayoutAddWindow(mainBox, label, 0, 5, GWM_BOX_ALL | GWM_BOX_FILL);
	
	GWMTextField *txt = gwmNewTextField(modal);
	gwmWriteTextField(txt, initialText);
	gwmBoxLayoutAddWindow(mainBox, txt, 0, 5, GWM_BOX_LEFT | GWM_BOX_RIGHT | GWM_BOX_DOWN | GWM_BOX_FILL);
	
	GWMSeparator *sep = gwmNewSeparator(modal);
	gwmBoxLayoutAddWindow(mainBox, sep, 0, 5, GWM_BOX_LEFT | GWM_BOX_RIGHT | GWM_BOX_FILL);
	
	GWMLayout *btnBox = gwmCreateBoxLayout(GWM_BOX_HORIZONTAL);
	gwmBoxLayoutAddLayout(mainBox, btnBox, 0, 5, GWM_BOX_ALL | GWM_BOX_FILL);
	gwmBoxLayoutAddSpacer(btnBox, 1, 0, 0);
	
	GWMButton *btnOK = gwmCreateStockButton(modal, GWM_SYM_OK);
	gwmBoxLayoutAddWindow(btnBox, btnOK, 0, 5, GWM_BOX_LEFT);
	
	GWMButton *btnCancel = gwmCreateStockButton(modal, GWM_SYM_CANCEL);
	gwmBoxLayoutAddWindow(btnBox, btnCancel, 0, 5, GWM_BOX_LEFT);
	
	int answer;
	
	gwmPushEventHandler(modal, inputDialogHandler, &answer);
	gwmFit(modal);
	gwmTextFieldSelectAll(txt);
	gwmFocus(txt);
	gwmRunModal(modal, GWM_WINDOW_NOTASKBAR | GWM_WINDOW_NOSYSMENU);
	gwmSetWindowFlags(modal, GWM_WINDOW_HIDDEN | GWM_WINDOW_NOTASKBAR);
	
	char *result = NULL;
	if (answer == GWM_SYM_OK)
	{
		result = strdup(gwmReadTextField(txt));
	};
	
	gwmDestroyButton(btnCancel);
	gwmDestroyButton(btnOK);
	gwmDestroyBoxLayout(btnBox);
	gwmDestroySeparator(sep);
	gwmDestroyTextField(txt);
	gwmDestroyLabel(label);
	gwmDestroyBoxLayout(mainBox);
	gwmDestroyModal(modal);
	
	return result;
};