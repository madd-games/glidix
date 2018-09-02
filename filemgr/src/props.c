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

#define _GLIDIX_SOURCE
#include <fstools.h>
#include <assert.h>
#include <sys/stat.h>
#include <sys/call.h>
#include <sys/storage.h>
#include <sys/fsinfo.h>
#include <fcntl.h>
#include <libgwm.h>
#include <time.h>

#include "props.h"

static void propPutTime(GWMTextField *txt, time_t time)
{
	char *str = ctime(&time);
	str[strlen(str)-1] = 0;		// get rid of the '\n' character, it's pointless
	gwmWriteTextField(txt, str);
};

void propShow(const char *path, FSMimeType *mime)
{
	struct stat st;
	if (stat(path, &st) != 0)
	{
		gwmMessageBox(NULL, "Error", "Stat failed", GWM_MBICON_ERROR | GWM_MBUT_OK);
		return;
	};
	
	char *lastSlash = strrchr(path, '/');
	const char *fname;
	if (lastSlash == NULL)
	{
		fname = path;
	}
	else
	{
		fname = lastSlash+1;
	};
	
	if (strcmp(path, "/") == 0)
	{
		fname = "/";
	};
	
	char *caption;
	asprintf(&caption, "%s - Properties", fname);
	
	GWMWindow *props = gwmCreateModal(caption, GWM_POS_UNSPEC, GWM_POS_UNSPEC, 0, 0);
	free(caption);
	
	GWMLayout *mainBox = gwmCreateBoxLayout(GWM_BOX_VERTICAL);
	gwmSetWindowLayout(props, mainBox);
	
	GWMNotebook *notebook = gwmNewNotebook(props);
	gwmBoxLayoutAddWindow(mainBox, notebook, 1, 0, GWM_BOX_FILL);
	
	GWMLayout *btnBox = gwmCreateBoxLayout(GWM_BOX_HORIZONTAL);
	gwmBoxLayoutAddLayout(mainBox, btnBox, 0, 0, GWM_BOX_FILL);
	
	gwmBoxLayoutAddSpacer(btnBox, 1, 0, 0);
	
	GWMButton *btnClose = gwmCreateStockButton(props, GWM_SYM_CLOSE);
	gwmBoxLayoutAddWindow(btnBox, btnClose, 0, 5, GWM_BOX_ALL);
	
	// == GENERAL TAB ==
	GWMTab *tabGeneral = gwmNewTab(notebook);
	gwmSetWindowCaption(tabGeneral, "General");
	
	GWMLayout *flexGeneral = gwmCreateFlexLayout(2);
	gwmSetWindowLayout(tabGeneral, flexGeneral);
	
	GWMLabel *lblFileName = gwmNewLabel(tabGeneral);
	gwmFlexLayoutAddWindow(flexGeneral, lblFileName, 0, 0, GWM_FLEX_FILL, GWM_FLEX_CENTER);
	gwmSetLabelText(lblFileName, "Name:");
	
	GWMTextField *txtFileName = gwmNewTextField(tabGeneral);
	gwmFlexLayoutAddWindow(flexGeneral, txtFileName, 1, 0, GWM_FLEX_FILL, GWM_FLEX_CENTER);
	gwmWriteTextField(txtFileName, fname);
	gwmSetTextFieldFlags(txtFileName, GWM_TXT_DISABLED);
	
	GWMLabel *lblType = gwmNewLabel(tabGeneral);
	gwmFlexLayoutAddWindow(flexGeneral, lblType, 0, 0, GWM_FLEX_FILL, GWM_FLEX_CENTER);
	gwmSetLabelText(lblType, "Type:");
	
	char *typespec;
	asprintf(&typespec, "%s (%s)", mime->label, mime->mimename);
	GWMTextField *txtType = gwmNewTextField(tabGeneral);
	gwmFlexLayoutAddWindow(flexGeneral, txtType, 1, 0, GWM_FLEX_FILL, GWM_FLEX_CENTER);
	gwmWriteTextField(txtType, typespec);
	gwmSetTextFieldFlags(txtType, GWM_TXT_DISABLED);
	free(typespec);
	
	GWMLabel *lblLocation = gwmNewLabel(tabGeneral);
	gwmFlexLayoutAddWindow(flexGeneral, lblLocation, 0, 0, GWM_FLEX_FILL, GWM_FLEX_CENTER);
	gwmSetLabelText(lblLocation, "Location:");
	
	GWMTextField *txtLocation = gwmNewTextField(tabGeneral);
	gwmFlexLayoutAddWindow(flexGeneral, txtLocation, 1, 0, GWM_FLEX_FILL, GWM_FLEX_CENTER);
	gwmWriteTextField(txtLocation, path);
	gwmSetTextFieldFlags(txtLocation, GWM_TXT_DISABLED);
	
	GWMLabel *lblCreated = gwmNewLabel(tabGeneral);
	gwmFlexLayoutAddWindow(flexGeneral, lblCreated, 0, 0, GWM_FLEX_FILL, GWM_FLEX_CENTER);
	gwmSetLabelText(lblCreated, "Created:");
	
	GWMTextField *txtCreated = gwmNewTextField(tabGeneral);
	gwmFlexLayoutAddWindow(flexGeneral, txtCreated, 1, 0, GWM_FLEX_FILL, GWM_FLEX_CENTER);
	propPutTime(txtCreated, st.st_btime);
	gwmSetTextFieldFlags(txtCreated, GWM_TXT_DISABLED);
	
	GWMLabel *lblMeta = gwmNewLabel(tabGeneral);
	gwmFlexLayoutAddWindow(flexGeneral, lblMeta, 0, 0, GWM_FLEX_FILL, GWM_FLEX_CENTER);
	gwmSetLabelText(lblMeta, "Metadata changed:");
	
	GWMTextField *txtMeta = gwmNewTextField(tabGeneral);
	gwmFlexLayoutAddWindow(flexGeneral, txtMeta, 1, 0, GWM_FLEX_FILL, GWM_FLEX_CENTER);
	propPutTime(txtMeta, st.st_ctime);
	gwmSetTextFieldFlags(txtMeta, GWM_TXT_DISABLED);
	
	GWMLabel *lblMod = gwmNewLabel(tabGeneral);
	gwmFlexLayoutAddWindow(flexGeneral, lblMod, 0, 0, GWM_FLEX_FILL, GWM_FLEX_CENTER);
	gwmSetLabelText(lblMod, "Modified:");
	
	GWMTextField *txtMod = gwmNewTextField(tabGeneral);
	gwmFlexLayoutAddWindow(flexGeneral, txtMod, 1, 0, GWM_FLEX_FILL, GWM_FLEX_CENTER);
	propPutTime(txtMod, st.st_mtime);
	gwmSetTextFieldFlags(txtMod, GWM_TXT_DISABLED);

	GWMLabel *lblAccess = gwmNewLabel(tabGeneral);
	gwmFlexLayoutAddWindow(flexGeneral, lblAccess, 0, 0, GWM_FLEX_FILL, GWM_FLEX_CENTER);
	gwmSetLabelText(lblAccess, "Accessed:");
	
	GWMTextField *txtAccess = gwmNewTextField(tabGeneral);
	gwmFlexLayoutAddWindow(flexGeneral, txtAccess, 1, 0, GWM_FLEX_FILL, GWM_FLEX_CENTER);
	propPutTime(txtAccess, st.st_atime);
	gwmSetTextFieldFlags(txtAccess, GWM_TXT_DISABLED);

	// == END TABS ==
	gwmLayout(props, 400, 600);
	gwmRunModal(props, GWM_WINDOW_MKFOCUSED | GWM_WINDOW_NOSYSMENU | GWM_WINDOW_NOTASKBAR);
	gwmSetWindowFlags(props, GWM_WINDOW_HIDDEN | GWM_WINDOW_NOTASKBAR);
	
	gwmDestroyLabel(lblFileName);
	gwmDestroyTextField(txtFileName);
	gwmDestroyLabel(lblType);
	gwmDestroyTextField(txtType);
	gwmDestroyLabel(lblLocation);
	gwmDestroyTextField(txtLocation);
	gwmDestroyLabel(lblCreated);
	gwmDestroyTextField(txtCreated);
	gwmDestroyLabel(lblMeta);
	gwmDestroyTextField(txtMeta);
	gwmDestroyLabel(lblMod);
	gwmDestroyTextField(txtMod);
	gwmDestroyLabel(lblAccess);
	gwmDestroyTextField(txtAccess);
	gwmDestroyNotebook(notebook);
	gwmDestroyWindow(props);
	
	gwmDestroyBoxLayout(mainBox);
	gwmDestroyBoxLayout(btnBox);
	gwmDestroyFlexLayout(flexGeneral);
};
