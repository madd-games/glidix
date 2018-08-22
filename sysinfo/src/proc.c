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
#include <libddi.h>
#include <sys/stat.h>
#include <pwd.h>
#include <grp.h>

enum
{
	COL_NULL,
	
	COL_EXE,
	COL_PID,
	COL_USER,
	COL_GROUP,
};

enum
{
	SYM_ = GWM_SYM_USER,
	SYM_TERM,
	SYM_KILL,
};

typedef struct ProcData_
{
	struct ProcData_*		next;
	char*				exe;
	pid_t				pid;
	int				found;
	char*				user;
	char*				group;
} ProcData;

extern GWMWindow* topWindow;

GWMDataCtrl* dcProc;

char* getUserName(uid_t uid)
{
	struct passwd *pwd = getpwuid(uid);
	if (pwd == NULL)
	{
		return strdup("?");
	}
	else
	{
		return strdup(pwd->pw_name);
	};
};

char* getGroupName(gid_t gid)
{
	struct group *grp = getgrgid(gid);
	if (grp == NULL)
	{
		return strdup("?");
	}
	else
	{
		return strdup(grp->gr_name);
	};
};

ProcData* procGetList()
{
	DIR *dirp = opendir("/proc");
	if (dirp == NULL) return NULL;
	
	ProcData *list = NULL;
	ProcData *last = NULL;
	
	struct dirent *ent;
	while ((ent = readdir(dirp)) != NULL)
	{
		pid_t pid;
		
		// only bother looking at that numbered directories (not ".", "..", "self", etc)
		if (sscanf(ent->d_name, "%d", &pid) == 1)
		{
			char path[PATH_MAX];
			sprintf(path, "/proc/%d", pid);
			
			struct stat st;
			if (stat(path, &st) == 0)
			{
				char exepath[PATH_MAX];
				sprintf(exepath, "/proc/%d/exe", pid);
				
				ssize_t sz = readlink(exepath, exepath, PATH_MAX);
				if (sz == -1)
				{
					continue;
				};
				
				exepath[sz] = 0;
				
				ProcData *data = (ProcData*) malloc(sizeof(ProcData));
				data->next = NULL;
				data->exe = strdup(exepath);
				data->pid = pid;
				data->found = 0;
				data->user = getUserName(st.st_uid);
				data->group = getGroupName(st.st_gid);
				
				if (last == NULL)
				{
					list = last = data;
				}
				else
				{
					last->next = data;
					last = data;
				};
			};
		};
	};
	
	closedir(dirp);
	return list;
};

void procDeleteList(ProcData *data)
{
	while (data != NULL)
	{
		ProcData *this = data;
		data = this->next;
		
		free(this->exe);
		free(this->user);
		free(this->group);
		free(this);
	};
};

void doSignal(int signo, const char *desc)
{
	GWMDataNode *node = gwmGetDataSelection(dcProc, 0);
	if (node == NULL) return;
	
	pid_t pid = (pid_t) (uint64_t) gwmGetDataNodeDesc(dcProc, node);
	
	char *msg;
	asprintf(&msg, "Stopping or interfering with some processes could cause the system to become unstable"
			" or unusable. Are you sure you want to %s process %d?", desc, pid);
	
	if (gwmMessageBox(topWindow, "System Information", msg, GWM_MBICON_WARN | GWM_MBUT_YESNO) == GWM_SYM_YES)
	{
		free(msg);
		
		if (kill(pid, signo) != 0)
		{
			gwmMessageBox(topWindow, "System Information", strerror(errno), GWM_MBICON_ERROR | GWM_MBUT_OK);
		};
	};
};

int procCommand(GWMCommandEvent *ev)
{
	switch (ev->symbol)
	{
	case SYM_TERM:
		doSignal(SIGTERM, "terminate");
		return GWM_EVSTATUS_OK;
	case SYM_KILL:
		doSignal(SIGKILL, "kill");
		return GWM_EVSTATUS_OK;
	default:
		return GWM_EVSTATUS_CONT;
	};
};

int procHandler(GWMEvent *ev, GWMWindow *win, void *context)
{
	switch (ev->type)
	{
	case GWM_EVENT_UPDATE:
		{
			ProcData *procs = procGetList();
			
			// first, delete all nodes which are no longer under /proc
			GWMDataNode *node;
			int i;
			for (i=0;;i++)
			{
				node = gwmGetDataNode(dcProc, NULL, i);
				if (node == NULL) break;
				
				pid_t pid = (pid_t) (uint64_t) gwmGetDataNodeDesc(dcProc, node);
				
				int found = 0;
				ProcData *scan;
				for (scan=procs; scan!=NULL; scan=scan->next)
				{
					if (scan->pid == pid)
					{
						found = 1;
						scan->found = 1;
						break;
					};
				};
				
				if (!found)
				{
					gwmDeleteDataNode(dcProc, node);
					i--;
				};
			};
			
			// if there are any processes under /proc which did not appear in
			// dcProc, then add them now
			ProcData *scan;
			for (scan=procs; scan!=NULL; scan=scan->next)
			{
				if (!scan->found)
				{
					GWMDataNode *node = gwmAddDataNode(dcProc, GWM_DATA_ADD_BOTTOM_CHILD, NULL);
					gwmSetDataNodeDesc(dcProc, node, (void*) (uint64_t) scan->pid);
					
					char pidstr[64];
					sprintf(pidstr, "%d", scan->pid);
					
					gwmSetDataString(dcProc, node, COL_EXE, scan->exe);
					gwmSetDataString(dcProc, node, COL_PID, pidstr);
					gwmSetDataString(dcProc, node, COL_USER, scan->user);
					gwmSetDataString(dcProc, node, COL_GROUP, scan->group);
				};
			};
			
			procDeleteList(procs);
		};
		return GWM_EVSTATUS_OK;
	case GWM_EVENT_COMMAND:
		return procCommand((GWMCommandEvent*) ev);
	default:
		return GWM_EVSTATUS_CONT;
	};
};

GWMWindow* newProcTab(GWMWindow *notebook)
{
	GWMWindow *tab = gwmNewTab(notebook);
	gwmSetWindowCaption(tab, "Processes");
	
	GWMLayout *mainBox = gwmCreateBoxLayout(GWM_BOX_VERTICAL);
	gwmSetWindowLayout(tab, mainBox);
	
	dcProc = gwmNewDataCtrl(tab);
	gwmBoxLayoutAddWindow(mainBox, dcProc, 1, 0, GWM_BOX_FILL);
	
	gwmSetDataFlags(dcProc, GWM_DATA_SHOW_HEADERS);
	GWMDataColumn *colApp = gwmAddDataColumn(dcProc, "Application", GWM_DATA_STRING, COL_EXE);
	gwmSetDataColumnWidth(dcProc, colApp, 300);
	
	gwmAddDataColumn(dcProc, "PID", GWM_DATA_STRING, COL_PID);
	gwmAddDataColumn(dcProc, "User", GWM_DATA_STRING, COL_USER);
	gwmAddDataColumn(dcProc, "Group", GWM_DATA_STRING, COL_GROUP);
	
	GWMLayout *btnBox = gwmCreateBoxLayout(GWM_BOX_HORIZONTAL);
	gwmBoxLayoutAddLayout(mainBox, btnBox, 0, 0, GWM_BOX_FILL);
	
	gwmBoxLayoutAddSpacer(btnBox, 1, 0, 0);
	
	gwmBoxLayoutAddWindow(btnBox, gwmCreateButtonWithLabel(tab, SYM_TERM, "Terminate"), 0, 0, 0);
	gwmBoxLayoutAddWindow(btnBox, gwmCreateButtonWithLabel(tab, SYM_KILL, "Kill"), 0, 0, 0);
	
	gwmCreateTimer(tab, 1000);
	gwmPushEventHandler(tab, procHandler, NULL);
	return tab;
};
