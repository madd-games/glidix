/*
	Glidix GUI

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
DDISurface *imgIconbar;
DDISurface *imgBackground;
GWMWindow *win;
int currentMouseX=0, currentMouseY=0;
int lastSelectRow=-1;
int lastSelectColumn=-1;
int pressed = 0;
GWMGlobWinRef windows[128];
int numWindows = 0;
GWMMenu *menuSys;
GWMMenu *menuCat[CAT_NUM];
DDISurface *imgFolder;
DDISurface *imgApp;

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
	appArgs[0] = "/usr/bin/env";
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
		else if (strcmp(option, "Terminal") == 0)
		{
			if (strcmp(value, "1") == 0)
			{
				appArgs[0] = "/usr/bin/terminal";
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
	
	GWMGlobWinRef focwin;
	GWMGlobWinRef winlist[128];
	int count = gwmGetDesktopWindows(&focwin, winlist);
	int displayIndex = 0;
	
	int selectedColumn = (currentMouseX-40)/150;
	int selectedRow = currentMouseY/20;
	if (currentMouseX < 40) selectedColumn = -1;
	
	int i;
	for (i=0; i<count; i++)
	{
		GWMWindowParams params;
		if (gwmGetGlobWinParams(&winlist[i], &params) == 0)
		{
			if ((params.flags & GWM_WINDOW_NOTASKBAR) == 0)
			{
				int spriteIndex = 0;

				int row = displayIndex & 1;
				int column = displayIndex >> 1;
				memcpy(&windows[displayIndex], &winlist[i], sizeof(GWMGlobWinRef));
				displayIndex++;
				
				if ((row == selectedRow) && (column == selectedColumn))
				{
					spriteIndex = 1;
					
					if (pressed)
					{
						spriteIndex = 2;
					};
				};
				
				if ((winlist[i].pid == focwin.pid) && (winlist[i].fd == focwin.fd)
					&& (winlist[i].id == focwin.id))
				{
					spriteIndex += 3;
				};
		
				ddiBlit(imgIconbar, 0, 20*spriteIndex, canvas, 40+150*column, 20*row, 150, 20);

				DDIPen *pen = ddiCreatePen(&canvas->format, gwmGetDefaultFont(), 45+150*column, 20*row+2,
								143, 18, 0, 0, NULL);
				if (pen != NULL)
				{
					char name[258];
					if (params.flags & GWM_WINDOW_HIDDEN)
					{
						sprintf(name, "[%s]", params.iconName);
					}
					else
					{
						strcpy(name, params.iconName);
					};
					
					ddiSetPenWrap(pen, 0);
					ddiWritePen(pen, name);
					ddiExecutePen(pen, canvas);
					ddiDeletePen(pen);
				};
			};
		};
	};
	
	numWindows = displayIndex;
	ddiBlit(menuButton, 0, 0, canvas, 2, 2, 32, 32);
	gwmPostDirty(win);
};

void triggerWindow(int index)
{
	if ((index < 0) || (index >= numWindows))
	{
		return;
	};
	
	gwmToggleWindow(&windows[index]);
};

int sysbarEventHandler(GWMEvent *ev, GWMWindow *win)
{
	int newSelectRow, newSelectColumn;
	int winIndex;
	
	switch (ev->type)
	{
	case GWM_EVENT_DOWN:
		if (ev->scancode == GWM_SC_MOUSE_LEFT)
		{
			pressed = 1;
			sysbarRedraw();
		};

		return 0;
	case GWM_EVENT_DESKTOP_UPDATE:
		sysbarRedraw();
		return 0;
	case GWM_EVENT_ENTER:
	case GWM_EVENT_MOTION:
		currentMouseX = ev->x;
		currentMouseY = ev->y;
		newSelectColumn = (ev->x-40)/150;
		newSelectRow = ev->y/20;
		if ((newSelectColumn != lastSelectColumn) || (newSelectRow != lastSelectRow))
		{
			lastSelectColumn = newSelectColumn;
			lastSelectRow = newSelectRow;
			sysbarRedraw();
		};
		return 0;
	case GWM_EVENT_LEAVE:
		currentMouseX = 0;
		currentMouseY = 0;
		lastSelectColumn = -1;
		lastSelectRow = -1;
		sysbarRedraw();
		return 0;
	case GWM_EVENT_UP:
		if (ev->scancode == GWM_SC_MOUSE_LEFT)
		{
			if (ev->x < 40)
			{
				gwmOpenMenu(menuSys, win, 0, 0);
			}
			else
			{
				pressed = 0;
				winIndex = 2 * lastSelectColumn + lastSelectRow;
				triggerWindow(winIndex);
				sysbarRedraw();
			};
		};
		return 0;
	default:
		return gwmDefaultHandler(ev, win);
	};
};

int sysbarShutdown(void *context)
{
	if (gwmMessageBox(NULL, "Shut down", "Are you sure you want to shut down?", GWM_MBICON_WARN | GWM_MBUT_YESNO) == 0)
	{
		if (fork() == 0)
		{
			execl("/usr/bin/halt", "poweroff", NULL);
			exit(1);
		};
		
		return -1;
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
	imgSysBar = ddiLoadAndConvertPNG(&canvas->format, "/usr/share/images/sysbar.png", &error);
	if (imgSysBar == NULL)
	{
		fprintf(stderr, "Failed to load sysbar pattern: %s\n", error);
		return 1;
	};
	
	menuButton = ddiLoadAndConvertPNG(&canvas->format, "/usr/share/images/sysmenubutton.png", &error);
	if (menuButton == NULL)
	{
		fprintf(stderr, "Failed to load menu button image: %s\n", error);
		return 1;
	};
	
	imgIconbar = ddiLoadAndConvertPNG(&canvas->format, "/usr/share/images/iconbar.png", &error);
	if (imgIconbar == NULL)
	{
		fprintf(stderr, "Failed to load icon bar image: %s\n", error);
		return 1;
	};

	imgBackground = ddiCreateSurface(&canvas->format, width, 40, NULL, 0);
	for (i=0; i<canvas->width; i++)
	{
		ddiOverlay(imgSysBar, 0, 0, imgBackground, i, 0, 1, 40);
	};

	sysbarRedraw();
	
	gwmSetListenWindow(win); 
	gwmSetEventHandler(win, sysbarEventHandler);
	gwmMainLoop();
	gwmQuit();
	return 0;
};
