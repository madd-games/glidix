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
#include <stdio.h>
#include <libgwm.h>
#include <time.h>
#include <sys/time.h>
#include <stdlib.h>
#include <string.h>
#include <signal.h>
#include <unistd.h>
#include <dirent.h>

/**
 * The system bar at the bottom of the screen.
 */

enum
{
	CAT_ACC,
	CAT_GAM,
	CAT_SYS,
	
	CAT_NUM
};

const char *catLabels[CAT_NUM] = {
	"Accessories",
	"Games",
	"System"
};

const char *catNames[CAT_NUM] = {
	"ACC",
	"GAM",
	"SYS"
};

DDISurface *imgSysBar;
DDISurface *menuButton;
DDISurface *imgTaskBtn;
DDISurface *imgBackground;
DDISurface *imgDefIcon;

GWMWindow *win;
int currentMouseX=0, currentMouseY=0;
int lastSelect=-1;
int pressed = 0;

GWMMenu *menuSys;
GWMMenu *menuCat[CAT_NUM];
DDISurface *imgFolder;
DDISurface *imgApp;
GWMGlobWinRef focwin;

typedef struct WinInfo_
{
	struct WinInfo_ *prev;
	struct WinInfo_ *next;
	
	/**
	 * Window parameters.
	 */
	GWMWindowParams params;
	
	/**
	 * The window icon.
	 */
	DDISurface *icon;
	
	/**
	 * The reference.
	 */
	GWMGlobWinRef ref;
} WinInfo;
WinInfo *windows;

int catByName(const char *name)
{
	int i;
	for (i=0; i<CAT_NUM; i++)
	{
		if (strcmp(catNames[i], name) == 0)
		{
			return i;
		};
	};
	
	return CAT_SYS;
};

int runApp(void *context)
{
	char **argv = (char**) context;
	
	if (fork() == 0)
	{
		execv(argv[0], argv);
		exit(1);
	};
	
	return 0;
};

void loadAppFile(const char *filename)
{
	FILE *fp = fopen(filename, "r");
	if (fp == NULL)
	{
		printf("Failed to open: %s\n", filename);
		return;
	};
	
	char line[1024];
	char *appName = NULL;
	int appCat = CAT_SYS;
	char **appArgs = (char**) malloc(sizeof(char*));
	appArgs[0] = "/usr/bin/grexec";
	int argc = 1;
	DDISurface *appIcon = NULL;
	
	int ok = 1;
	
	while (fgets(line, 1024, fp) != NULL)
	{
		char *newline = strchr(line, '\n');
		if (newline == NULL)
		{
			printf("File %s contains a line that is too long\n", filename);
			ok = 0;
			break;
		};
		
		*newline = 0;
		
		if (line[0] == '#')
		{
			continue;
		};
		
		char *equals = strchr(line, '=');
		if (equals == NULL)
		{
			printf("Invalid line in file %s: %s\n", filename, line);
			ok = 0;
			break;
		};
		
		char *option = line;
		*equals = 0;
		char *value = equals+1;
		
		if (strcmp(option, "Name") == 0)
		{
			appName = strdup(value);
		}
		else if (strcmp(option, "Exec") == 0)
		{
			char *token;
			
			for (token=strtok(value, " "); token!=NULL; token=strtok(NULL, " "))
			{
				int index = argc++;
				appArgs = realloc(appArgs, sizeof(char*)*argc);
				appArgs[index] = strdup(token);
			};
		}
		else if (strcmp(option, "Icon") == 0)
		{
			if (appIcon == NULL)
			{
				appIcon = ddiLoadAndConvertPNG(&imgFolder->format, value, NULL);
			};
		}
		else if (strcmp(option, "Category") == 0)
		{
			appCat = catByName(value);
		}
		else
		{
			printf("Invalid option in %s: %s\n", filename, option);
			ok = 0;
			break;
		};
	};

	fclose(fp);
	
	if (appIcon == NULL)
	{
		appIcon = imgApp;
	};
	
	if (ok)
	{
		appArgs = realloc(appArgs, sizeof(char*) * (argc+1));
		appArgs[argc] = NULL;
	
		if (appName == NULL)
		{
			gwmMenuAddEntry(menuCat[appCat], "Nameless App", runApp, appArgs);
		}
		else
		{
			gwmMenuAddEntry(menuCat[appCat], appName, runApp, appArgs);
			free(appName);
		};
		
		gwmMenuSetIcon(menuCat[appCat], appIcon);
	}
	else
	{
		free(appArgs);
		free(appName);
	};
};

void loadApps()
{
	DIR *dirp = opendir("/usr/share/apps");
	if (dirp == NULL)
	{
		printf("Failed to open /usr/share/apps");
		return;
	};
	
	struct dirent *ent;
	while ((ent = readdir(dirp)) != NULL)
	{
		if (ent->d_name[0] != '.')
		{
			char fullpath[256];
			sprintf(fullpath, "/usr/share/apps/%s", ent->d_name);
		
			loadAppFile(fullpath);
		};
	};
	
	closedir(dirp);
};

void on_signal(int sig, siginfo_t *si, void *ignore)
{
	if (sig == SIGCHLD)
	{
		waitpid(si->si_pid, NULL, 0);
	};
};

void sysbarRedraw()
{
	DDISurface *canvas = gwmGetWindowCanvas(win);
	ddiOverlay(imgBackground, 0, 0, canvas, 0, 0, canvas->width, canvas->height);

	WinInfo *info;
	int index = 0;
	for (info=windows; info!=NULL; info=info->next)
	{
		int spriteIndex = 4;
		if (index == lastSelect)
		{
			spriteIndex = 2;
			if (pressed) spriteIndex = 3;
		}
		else if (info->ref.id == focwin.id && info->ref.fd == focwin.fd)
		{
			spriteIndex = 0;
		};
		
		ddiBlit(imgTaskBtn, 32*spriteIndex, 0, canvas, 40+34*index, 3, 32, 32);
		if (info->icon != NULL)
		{
			if (info->icon->width < (16+24))
			{
				ddiBlit(info->icon, 0, 0, canvas, 40+34*index+8, 11, 16, 16);
			}
			else
			{
				ddiBlit(info->icon, 16, 0, canvas, 40+34*index+4, 7, 24, 24);
			};
		}
		else
		{
			ddiBlit(imgDefIcon, 0, 0, canvas, 40+34*index+8, 11, 16, 16);
		};
		index++;
	};
	
	ddiBlit(menuButton, 0, 0, canvas, 2, 2, 32, 32);
	gwmPostDirty(win);
};

void triggerWindow(int index)
{
	WinInfo *info = windows;
	while (index--)
	{
		if (info == NULL) return;
		info = info->next;
	};
	
	if (info == NULL) return;
	gwmToggleWindow(&info->ref);
};

void desktopUpdate()
{
	GWMGlobWinRef winlist[128];
	int count = gwmGetDesktopWindows(&focwin, winlist);
	
	// first remove from the list any windows that are no longer existent
	WinInfo *info = windows;
	while (info != NULL)
	{
		int i;
		int found = 0;
		for (i=0; i<count; i++)
		{
			if (info->ref.id == winlist[i].id && info->ref.fd == winlist[i].fd)
			{
				found = 1;
				break;
			};
		};

		GWMWindowParams params;
		if (gwmGetGlobWinParams(&info->ref, &params) != 0)
		{
			found = 0;
		};
		
		if (params.flags & GWM_WINDOW_NOTASKBAR)
		{
			found = 0;
		};

		if (!found)
		{
			WinInfo *next = info->next;
			if (info->prev != NULL) info->prev->next = info->next;
			if (info->next != NULL) info->next->prev = info->prev;
			if (windows == info) windows = info->next;
			
			if (info->icon != NULL) ddiDeleteSurface(info->icon);
			free(info);
			info = next;
		}
		else
		{
			info = info->next;
		};
	};
	
	// now add to the list the windows that aren't on it yet
	int i;
	for (i=0; i<count; i++)
	{
		GWMWindowParams params;
		if (gwmGetGlobWinParams(&winlist[i], &params) != 0)
		{
			continue;
		};
		
		if (params.flags & GWM_WINDOW_NOTASKBAR)
		{
			continue;
		};
		
		int found = 0;
		for (info=windows; info!=NULL; info=info->next)
		{
			if (info->ref.id == winlist[i].id && info->ref.fd == winlist[i].fd)
			{
				found = 1;
				uint32_t iconID = gwmGetGlobIcon(&info->ref);
				if (info->icon == NULL)
				{
					if (iconID != 0)
					{
						info->icon = ddiOpenSurface(iconID);
					};
				}
				else
				{
					if (iconID == 0)
					{
						ddiDeleteSurface(info->icon);
						info->icon = NULL;
					}
					else if (iconID != info->icon->id)
					{
						ddiDeleteSurface(info->icon);
						info->icon = ddiOpenSurface(iconID);
					};
				};
				break;
			};
		};
		
		if (!found)
		{
			info = (WinInfo*) malloc(sizeof(WinInfo));
			info->next = NULL;
			memcpy(&info->ref, &winlist[i], sizeof(GWMGlobWinRef));
			uint32_t iconID = gwmGetGlobIcon(&winlist[i]);
			if (iconID == 0) info->icon = NULL;
			else info->icon = ddiOpenSurface(iconID);
			memcpy(&info->params, &params, sizeof(GWMWindowParams));
			
			if (windows == NULL)
			{
				info->prev = NULL;
				windows = info;
			}
			else
			{
				WinInfo *last = windows;
				while (last->next != NULL) last = last->next;
				info->prev = last;
				last->next = info;
			};
		};
	};
};

int sysbarEventHandler(GWMEvent *ev, GWMWindow *win, void *context)
{
	int newSelect;
	int winIndex;
	
	switch (ev->type)
	{
	case GWM_EVENT_DOWN:
		if (ev->keycode == GWM_KC_MOUSE_LEFT)
		{
			pressed = 1;
			sysbarRedraw();
		};

		return GWM_EVSTATUS_OK;
	case GWM_EVENT_DESKTOP_UPDATE:
		desktopUpdate();
		sysbarRedraw();
		return GWM_EVSTATUS_OK;
	case GWM_EVENT_ENTER:
	case GWM_EVENT_MOTION:
		currentMouseX = ev->x;
		currentMouseY = ev->y;
		newSelect = (ev->x-40)/34;
		if (newSelect != lastSelect)
		{
			lastSelect = newSelect;
			sysbarRedraw();
		};
		return GWM_EVSTATUS_OK;
	case GWM_EVENT_LEAVE:
		currentMouseX = 0;
		currentMouseY = 0;
		lastSelect = -1;
		sysbarRedraw();
		return GWM_EVSTATUS_OK;
	case GWM_EVENT_UP:
		if (ev->keycode == GWM_KC_MOUSE_LEFT)
		{
			pressed = 0;
			if (ev->x < 40)
			{
				gwmOpenMenu(menuSys, win, 0, 0);
			}
			else
			{
				winIndex = lastSelect;
				triggerWindow(winIndex);
				sysbarRedraw();
			};
		};
		return GWM_EVSTATUS_OK;
	default:
		return GWM_EVSTATUS_CONT;
	};
};

int sysbarShutdown(void *context)
{
	if (fork() == 0)
	{
		execl("/usr/bin/gui-shutdown", "gui-shutdown", NULL);
		_exit(1);
	};
	
	return 0;
};

int main()
{
	struct sigaction sa;
	sa.sa_sigaction = on_signal;
	sigemptyset(&sa.sa_mask);
	sa.sa_flags = SA_SIGINFO;
	if (sigaction(SIGCHLD, &sa, NULL) != 0)
	{
		perror("sigaction SIGCHLD");
		return 1;
	};

	if (gwmInit() != 0)
	{
		fprintf(stderr, "Failed to initialize GWM!\n");
		return 1;
	};

	int width, height;
	gwmScreenSize(&width, &height);
	
	win = gwmCreateWindow(NULL, "sysbar", 0, height-40, width, 40,
				GWM_WINDOW_MKFOCUSED | GWM_WINDOW_NODECORATE | GWM_WINDOW_NOTASKBAR);
	
	if (win == NULL)
	{
		fprintf(stderr, "Failed to create window!\n");
		return 1;
	};

	DDISurface *canvas = gwmGetWindowCanvas(win);
	imgFolder = ddiLoadAndConvertPNG(&canvas->format, "/usr/share/images/fileman.png", NULL);
	imgApp = ddiLoadAndConvertPNG(&canvas->format, "/usr/share/images/appicon.png", NULL);
	
	int i;
	menuSys = gwmCreateMenu();
	for (i=0; i<CAT_NUM; i++)
	{
		menuCat[i] = gwmMenuAddSub(menuSys, catLabels[i]);
		gwmMenuSetIcon(menuSys, imgFolder);
	};
	
	gwmMenuAddSeparator(menuSys);
	gwmMenuAddEntry(menuSys, "Shut down...", sysbarShutdown, NULL);
	
	loadApps();
	
	const char *error;
	imgSysBar = (DDISurface*) gwmGetThemeProp("gwm.sysbar.sysbar", GWM_TYPE_SURFACE, NULL);
	if (imgSysBar == NULL)
	{
		fprintf(stderr, "Failed to load sysbar pattern: %s\n", error);
		return 1;
	};
	
	menuButton = (DDISurface*) gwmGetThemeProp("gwm.sysbar.menu", GWM_TYPE_SURFACE, NULL);
	if (menuButton == NULL)
	{
		fprintf(stderr, "Failed to load menu button image: %s\n", error);
		return 1;
	};
	
	imgTaskBtn = (DDISurface*) gwmGetThemeProp("gwm.sysbar.taskbtn", GWM_TYPE_SURFACE, NULL);
	if (imgTaskBtn == NULL)
	{
		fprintf(stderr, "Failed to load task bar image: %s\n", error);
		return 1;
	};

	imgDefIcon = ddiLoadAndConvertPNG(&canvas->format, "/usr/share/images/defwinicon.png", &error);
	if (imgDefIcon == NULL)
	{
		fprintf(stderr, "Failed to load default icon image: %s\n", error);
		return 1;
	};

	imgBackground = ddiCreateSurface(&canvas->format, width, 40, NULL, 0);
	for (i=0; i<canvas->width; i++)
	{
		ddiOverlay(imgSysBar, 0, 0, imgBackground, i, 0, 1, 40);
	};

	sysbarRedraw();
	
	gwmSetListenWindow(win); 
	gwmPushEventHandler(win, sysbarEventHandler, NULL);
	gwmMainLoop();
	gwmQuit();
	return 0;
};
