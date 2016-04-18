#include <stdio.h>
#include <libgwm.h>

int myEventHandler(GWMEvent *ev, GWMWindow *win)
{
	switch (ev->type)
	{
	case GWM_EVENT_CLOSE:
		return -1;
	default:
		return gwmDefaultHandler(ev, win);
	};
};

int myButtonCallback(void *ignore)
{
	int result = gwmMessageBox(NULL, "Caption", "This is a meme.", GWM_MBICON_ERROR | GWM_MBUT_YESNO);
	printf("Result: %d\n", result);
	return 0;
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

	GWMWindow *btn1 = gwmCreateButton(win, "Normal", 5, 5, 80, 0);
	gwmSetButtonCallback(btn1, myButtonCallback, NULL);
	GWMWindow *btn2 = gwmCreateButton(win, "Disabled", 5, 40, 80, GWM_BUTTON_DISABLED);
	
	gwmSetEventHandler(win, myEventHandler);
	gwmMainLoop();

	gwmQuit();
	return 0;
};
