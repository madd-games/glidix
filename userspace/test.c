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

int main()
{
	gwmInit();
	char *text = gwmGetInput("Example caption", "Enter text:", "Hello, world!");
	if (text == NULL)
	{
		printf("You clicked cancel!\n");
	}
	else
	{
		printf("You clicked OK and typed: %s\n", text);
		free(text);
	};
	
	gwmQuit(); 
	return 0;
};
