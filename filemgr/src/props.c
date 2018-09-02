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
#include <sys/xperm.h>
#include <sys/storage.h>
#include <sys/fsinfo.h>
#include <fcntl.h>
#include <libgwm.h>
#include <time.h>
#include <errno.h>

#include "props.h"
#include "filemgr.h"

typedef struct
{
	const char*		path;
	mode_t			mode;
} PropsData;

static void propPutTime(GWMTextField *txt, time_t time)
{
	char *str = ctime(&time);
	str[strlen(str)-1] = 0;		// get rid of the '\n' character, it's pointless
	gwmWriteTextField(txt, str);
};

static int propsHandler(GWMEvent *ev, GWMWindow *win, void *context)
{
	GWMCommandEvent *cmdev = (GWMCommandEvent*) ev;
	PropsData *data = (PropsData*) context;
	
	switch (ev->type)
	{
	case GWM_EVENT_COMMAND:
		if (cmdev->symbol >= PROP_CHMOD && cmdev->symbol < PROP_CHMOD_END)
		{
			int bitno = cmdev->symbol - PROP_CHMOD;
			mode_t mask = (mode_t) 1 << bitno;
			mode_t newMode = data->mode ^ mask;
			
			if (chmod(data->path, newMode) != 0)
			{
				char *errmsg;
				asprintf(&errmsg, "Failed to change permissions: %s", strerror(errno));
				gwmMessageBox(win, "Error", errmsg, GWM_MBICON_ERROR | GWM_MBUT_OK);
				free(errmsg);
				return GWM_EVSTATUS_OK;		// do not flip checkbox
			};
			
			// change was successful, flip the checkbox
			return GWM_EVSTATUS_CONT;
		};
		return GWM_EVSTATUS_CONT;
	default:
		return GWM_EVSTATUS_CONT;
	};
};

void propShow(const char *path, FSMimeType *mime)
{
	struct stat st;
	if (stat(path, &st) != 0)
	{
		gwmMessageBox(NULL, "Error", "Stat failed", GWM_MBICON_ERROR | GWM_MBUT_OK);
		return;
	};
	
	PropsData *data = (PropsData*) malloc(sizeof(PropsData));
	data->path = path;
	data->mode = st.st_mode & 0xFFF;
	
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

	// == PERMISSIONS TAB ==
	GWMTab *tabPerms = gwmNewTab(notebook);
	gwmSetWindowCaption(tabPerms, "Permissions");
	
	GWMLayout *boxPerms = gwmCreateBoxLayout(GWM_BOX_VERTICAL);
	gwmSetWindowLayout(tabPerms, boxPerms);
	
	GWMFrame *frUnix = gwmNewFrame(tabPerms);
	gwmBoxLayoutAddWindow(boxPerms, frUnix, 0, 5, GWM_BOX_FILL | GWM_BOX_ALL);
	gwmSetFrameCaption(frUnix, "Basic permissions");
	
	GWMWindow *pnlUnix = gwmGetFramePanel(frUnix);
	
	GWMLayout *boxUnixMain = gwmCreateBoxLayout(GWM_BOX_HORIZONTAL);
	gwmSetWindowLayout(pnlUnix, boxUnixMain);
	
	GWMLayout *flexUnix = gwmCreateFlexLayout(4);
	gwmBoxLayoutAddLayout(boxUnixMain, flexUnix, 0, 0, GWM_BOX_FILL);
	
	gwmFlexLayoutAddLayout(flexUnix, NULL, 0, 0, GWM_FLEX_FILL, GWM_FLEX_CENTER);
	static const char *permHeads[3] = {"Read", "Write", "Execute"};
	GWMLabel *lblPermHeads[3];
	
	int i;
	for (i=0; i<3; i++)
	{
		lblPermHeads[i] = gwmNewLabel(pnlUnix);
		gwmFlexLayoutAddWindow(flexUnix, lblPermHeads[i], 0, 0, GWM_FLEX_FILL, GWM_FLEX_CENTER);
		gwmSetLabelAlignment(lblPermHeads[i], DDI_ALIGN_CENTER);
		gwmSetLabelText(lblPermHeads[i], permHeads[i]);
	};
	
	static const char *classNames[3] = {"Owner", "Group", "World"};
	GWMLabel *lblClass[3];
	GWMCheckbox *cbUnix[12];
	
	for (i=0; i<3; i++)
	{
		lblClass[i] = gwmNewLabel(pnlUnix);
		gwmFlexLayoutAddWindow(flexUnix, lblClass[i], 0, 0, GWM_FLEX_FILL, GWM_FLEX_CENTER);
		gwmSetLabelAlignment(lblClass[i], DDI_ALIGN_RIGHT);
		gwmSetLabelText(lblClass[i], classNames[i]);
		
		// make the checkboxes, setting their initial state to whether or not the bit in question
		// is set. then make the checkbox disabled if the current user does not have the right to
		// change it.
		int j;
		for (j=0; j<3; j++)
		{
			int bitno = 8 - (3 * i + j);
			mode_t mask = (mode_t) 1 << bitno;
			
			GWMCheckbox *cb = gwmNewCheckbox(pnlUnix);
			cbUnix[bitno] = cb;
			
			gwmFlexLayoutAddWindow(flexUnix, cb, 0, 0, GWM_FLEX_CENTER, GWM_FLEX_CENTER);
			gwmSetCheckboxState(cb, (st.st_mode & mask) != 0);
			
			if (!_glidix_haveperm(XP_FSADMIN) && st.st_uid != geteuid())
			{
				gwmSetCheckboxFlags(cb, GWM_CB_DISABLED);
			};
			
			gwmSetCheckboxSymbol(cb, PROP_CHMOD + bitno);
		};
	};
	
	GWMLayout *boxUnixMisc = gwmCreateBoxLayout(GWM_BOX_VERTICAL);
	gwmBoxLayoutAddLayout(boxUnixMain, boxUnixMisc, 0, 5, GWM_BOX_ALL | GWM_BOX_FILL);
	
	static const char *miscLabels[3] = {"Execute as owner (SUID)", "Execute as group (SGID)", "Shared directory (sticky)"};
	for (i=0; i<3; i++)
	{
		GWMCheckbox *cb = gwmNewCheckbox(pnlUnix);
		cbUnix[9+i] = cb;
		
		gwmBoxLayoutAddWindow(boxUnixMisc, cb, 0, 1, GWM_BOX_ALL | GWM_BOX_FILL);
		gwmSetCheckboxLabel(cb, miscLabels[i]);
		
		int bitno = 11 - i;
		mode_t mask = (mode_t) 1 << bitno;
		
		gwmSetCheckboxSymbol(cb, PROP_CHMOD + bitno);
		gwmSetCheckboxState(cb, (st.st_mode & mask) != 0);
		
		if (!_glidix_haveperm(XP_FSADMIN) && st.st_uid != geteuid())
		{
			gwmSetCheckboxFlags(cb, GWM_CB_DISABLED);
		};
	};
	
	// == END TABS ==
	gwmLayout(props, 400, 600);
	gwmPushEventHandler(props, propsHandler, data);
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
	gwmDestroyFrame(frUnix);
	gwmDestroyNotebook(notebook);
	
	for (i=0; i<3; i++)
	{
		gwmDestroyLabel(lblPermHeads[i]);
	};
	
	for (i=0; i<3; i++)
	{
		gwmDestroyLabel(lblClass[i]);
	};
	
	for (i=0; i<12; i++)
	{
		gwmDestroyCheckbox(cbUnix[i]);
	};
	
	gwmDestroyBoxLayout(mainBox);
	gwmDestroyBoxLayout(btnBox);
	gwmDestroyFlexLayout(flexGeneral);
	gwmDestroyBoxLayout(boxPerms);
	gwmDestroyBoxLayout(boxUnixMain);
	gwmDestroyFlexLayout(flexUnix);
	gwmDestroyBoxLayout(boxUnixMisc);
	
	gwmDestroyWindow(props);
};
