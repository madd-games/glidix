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

#include <sys/wait.h>
#include <sys/stat.h>
#include <stdio.h>
#include <libgwm.h>
#include <time.h>
#include <sys/time.h>
#include <stdlib.h>
#include <string.h>
#include <signal.h>
#include <unistd.h>
#include <dirent.h>
#include <pthread.h>
#include <poll.h>
#include <errno.h>
#include <fcntl.h>

enum
{
	COL_NAME,
};

GWMDataCtrl *dcThemes;

int main()
{
	if (gwmInit() != 0)
	{
		fprintf(stderr, "Failed to initialize GWM\n");
		return 1;
	};
	
	GWMWindow *topWindow = gwmCreateWindow(NULL, "Theme Manager", GWM_POS_UNSPEC, GWM_POS_UNSPEC, 0, 0,
					GWM_WINDOW_HIDDEN | GWM_WINDOW_NOTASKBAR);
					
	GWMLayout *box = gwmCreateBoxLayout(GWM_BOX_VERTICAL);
	gwmSetWindowLayout(topWindow, box);
	
	dcThemes = gwmNewDataCtrl(topWindow);
	gwmBoxLayoutAddWindow(box, dcThemes, 1, 0, GWM_BOX_FILL);
	
	GWMDataColumn *col = gwmAddDataColumn(dcThemes, "Name", GWM_DATA_STRING, COL_NAME);
	gwmSetDataColumnWidth(dcThemes, col, 300);
	
	DIR *dirp = opendir("/usr/share/themes");
	if (dirp == NULL)
	{
		char *errmsg;
		asprintf(&errmsg, "Failed to scan /usr/share/themes: %s", strerror(errno));
		gwmMessageBox(NULL, "Theme Manager", errmsg, GWM_MBICON_ERROR | GWM_MBUT_OK);
		return 1;
	};
	
	struct dirent *ent;
	while ((ent = readdir(dirp)) != NULL)
	{
		if (strlen(ent->d_name) < 4) continue;
		if (strcmp(&ent->d_name[strlen(ent->d_name)-4], ".thm") != 0) continue;
		
		char *fullpath;
		asprintf(&fullpath, "/usr/share/themes/%s", ent->d_name);
		
		GWMDataNode *node = gwmAddDataNode(dcThemes, GWM_DATA_ADD_BOTTOM_CHILD, NULL);
		gwmSetDataString(dcThemes, node, COL_NAME, ent->d_name);
		gwmSetDataNodeDesc(dcThemes, node, fullpath);
	};
	
	closedir(dirp);
	
	gwmFit(topWindow);
	gwmSetWindowFlags(topWindow, GWM_WINDOW_MKFOCUSED | GWM_WINDOW_RESIZEABLE);
	gwmMainLoop();
	gwmQuit();
	return 0;
};