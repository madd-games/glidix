#include <pthread.h>
#include <stdio.h>
#include <stdlib.h>
#include <sys/glidix.h>
#include <time.h>
#include <signal.h>
#include <libgwm.h>
#include <libddi.h>

int myEventHandler(GWMEvent *ev, GWMWindow *win)
{
	printf("received event type %d\n", ev->type);
	switch (ev->type)
	{
	case GWM_EVENT_CLOSE:
		return -1;
	case GWM_EVENT_DESKTOP_UPDATE:
		printf("DESKTOP UPDATE\n");
		return 0;
	default:
		return gwmDefaultHandler(ev, win);
	};
};

int main()
{
	if (gwmInit() != 0)
	{
		fprintf(stderr, "Failed to initialize GWM!\n");
		return 1;
	};

	GWMWindow *win = gwmCreateWindow(NULL, "Hello world", 10, 10, 200, 200, GWM_WINDOW_HIDDEN);
	if (win == NULL)
	{
		fprintf(stderr, "Failed to create window!\n");
		return 1;
	};

	gwmSetListenWindow(win);
	
	gwmSetEventHandler(win, myEventHandler);
	gwmMainLoop();
	gwmQuit();
	return 0;
};
