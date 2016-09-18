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
	printf("Putting something in the clipboard\n");
	gwmClipboardPutText("hello world", 11);
	return 0;
};
