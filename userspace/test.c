#include <stdio.h>
#include <libgwm.h>

GWMWindow *txt1;
GWMWindow *txt2;
GWMWindow *txt3;

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
	size_t sz = gwmGetTextFieldSize(txt2);
	char buffer[sz+1];
	gwmReadTextField(txt2, buffer, 0, sz);
	int result = gwmMessageBox(NULL, "Caption", buffer, GWM_MBICON_ERROR | GWM_MBUT_YESNO);
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

	int width, height;
	gwmScreenSize(&width, &height);
	printf("Screen size: %dx%d\n", width, height);
	
	GWMWindow *win = gwmCreateWindow(NULL, "Hello world", 10, 10, 200, 200, GWM_WINDOW_MKFOCUSED);
	if (win == NULL)
	{
		fprintf(stderr, "Failed to create window!\n");
		return 1;
	};

	GWMWindow *btn1 = gwmCreateButton(win, "Normal", 5, 5, 80, 0);
	gwmSetButtonCallback(btn1, myButtonCallback, NULL);
	GWMWindow *btn2 = gwmCreateButton(win, "Disabled", 5, 40, 80, GWM_BUTTON_DISABLED);
	
	txt1 = gwmCreateTextField(win, "Memes", 5, 90, 180, 0);
	txt2 = gwmCreateTextField(win, "", 5, 120, 180, GWM_TXT_MASKED);
	txt3 = gwmCreateTextField(win, "Some shit", 5, 150, 180, GWM_TXT_DISABLED);
	
	gwmSetEventHandler(win, myEventHandler);
	gwmMainLoop();

	gwmQuit();
	return 0;
};
