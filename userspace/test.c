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

void draw(GWMWindow *win)
{
	gwmClearWindow(win);
	DDISurface *canvas = gwmGetWindowCanvas(win);
	if (pen != NULL) ddiDeletePen(pen);
	
	const char *error;
	pen = ddiCreatePen(&canvas->format, gwmGetDefaultFont(), 2, 2, 196, 196, 0, 0, &error);
	if (pen == NULL)
	{
		fprintf(stderr, "Cannot create pen: %s\n", error);
		exit(1);
	};
	
	ddiSetPenCursor(pen, cursorPos);
	ddiWritePen(pen, "Basically this is an extremely nice meme :) AVE MARIA");
	ddiExecutePen(pen, canvas);
	
	gwmPostDirty(win);
};

int myEventHandler(GWMEvent *ev, GWMWindow *win)
{
	int pos;
	switch (ev->type)
	{
	case GWM_EVENT_CLOSE:
		return -1;
	case GWM_EVENT_DOWN:
		if (ev->scancode == GWM_SC_MOUSE_LEFT)
		{
			pos = ddiPenCoordsToPos(pen, ev->x, ev->y);
			printf("Mouse click: (%d, %d) -> %d\n", ev->x, ev->y, pos);
			if (pos != -1) cursorPos = pos;
			draw(win);
		};
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

	GWMWindow *win = gwmCreateWindow(NULL, "Hello world", 10, 10, 200, 200, GWM_WINDOW_MKFOCUSED);
	if (win == NULL)
	{
		fprintf(stderr, "Failed to create window!\n");
		return 1;
	};

	gwmSetWindowCursor(win, GWM_CURSOR_TEXT);
	draw(win);

	gwmSetEventHandler(win, myEventHandler);
	gwmMainLoop();
	gwmQuit();
	return 0;
};
