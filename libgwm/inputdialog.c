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

#define	DIALOG_WIDTH							200

int inputOK(void *context)
{
	GWMWindow *dialog = (GWMWindow*) context;
	GWMWindow *txtInput = (GWMWindow*) dialog->data;
	
	size_t size = gwmGetTextFieldSize(txtInput);
	if (size == 0) return 0;
	
	char *buffer = (char*) malloc(size+1);
	buffer[size] = 0;
	gwmReadTextField(txtInput, buffer, 0, (off_t)size);
	
	dialog->data = buffer;
	return -1;
};

int inputCancel(void *context)
{
	GWMWindow *dialog = (GWMWindow*) context;
	dialog->data = NULL;
	return -1;
};

char* gwmGetInput(const char *caption, const char *prompt, const char *initialText)
{
	DDIPixelFormat screenFormat;
	gwmGetScreenFormat(&screenFormat);
	
	DDIPen *pen = ddiCreatePen(&screenFormat, gwmGetDefaultFont(), 2, 2, DIALOG_WIDTH-4, 20, 0, 0, NULL);
	ddiWritePen(pen, prompt);
	
	int width, height;
	ddiGetPenSize(pen, &width, &height);
	
	GWMWindow *dialog = gwmCreateModal(caption, GWM_POS_UNSPEC, GWM_POS_UNSPEC, DIALOG_WIDTH, height + 20 + 30 + 8);
	DDISurface *canvas = gwmGetWindowCanvas(dialog);
	
	ddiExecutePen(pen, canvas);
	ddiDeletePen(pen);
	
	GWMWindow *txtInput = gwmCreateTextField(dialog, initialText, 2, height+4, DIALOG_WIDTH-4, 0);

	int buttonWidth = (DIALOG_WIDTH-6)/2;
	GWMWindow *btnOK = gwmCreateButton(dialog, "OK", 2, height+26, buttonWidth, 0);
	gwmSetButtonCallback(btnOK, inputOK, dialog);
	GWMWindow *btnCancel = gwmCreateButton(dialog, "Cancel", 4+buttonWidth, height+26, buttonWidth, 0);
	gwmSetButtonCallback(btnCancel, inputCancel, dialog);
	
	dialog->data = txtInput;
	gwmSetWindowFlags(txtInput, GWM_WINDOW_MKFOCUSED);
	gwmPostDirty(dialog);
	gwmRunModal(dialog, GWM_WINDOW_NOTASKBAR | GWM_WINDOW_NOSYSMENU | GWM_WINDOW_NOICON);
	
	gwmDestroyTextField(txtInput);
	gwmDestroyButton(btnOK);
	gwmDestroyButton(btnCancel);
	
	char *result = (char*) dialog->data;
	gwmDestroyWindow(dialog);
	
	return result;
};
