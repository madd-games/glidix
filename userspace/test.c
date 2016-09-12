#include <stdio.h>
#include <stdlib.h>
#include <fcntl.h>
#include <unistd.h>
#include <sys/glidix.h>
#include <sys/socket.h>
#include <arpa/inet.h>
#include <string.h>
#include <poll.h>
#include <sys/stat.h>
#include <libddi.h>
#include <time.h>
#include <libgwm.h>

DDIPen *pen = NULL;
int cursorPos = 0;
GWMWindow *win;

void draw(GWMWindow *win)
{
	gwmClearWindow(win);
	DDISurface *canvas = gwmGetWindowCanvas(win);
	
	int fd = open("/dev/random", O_RDONLY);
	read(fd, canvas->data, canvas->width*canvas->height*canvas->format.bpp);
	close(fd);
	
	gwmPostDirty(win);
};

int myEventHandler(GWMEvent *ev, GWMWindow *win)
{
	int pos;
	switch (ev->type)
	{
	case GWM_EVENT_CLOSE:
		return -1;
	case GWM_EVENT_RESIZE_REQUEST:
		gwmMoveWindow(win, ev->x, ev->y);
		gwmResizeWindow(win, ev->width, ev->height);
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

	win = gwmCreateWindow(NULL, "Hello world", 100, 100, 200, 200, GWM_WINDOW_MKFOCUSED | GWM_WINDOW_RESIZEABLE);
	if (win == NULL)
	{
		fprintf(stderr, "Failed to create window!\n");
		return 1;
	};

	draw(win);

	gwmSetEventHandler(win, myEventHandler);
	gwmMainLoop();
	gwmQuit();
	return 0;
};
