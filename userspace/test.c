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

void _heap_dump();

int main(int argc, char *argv[])
{
	int fd = open("/dev/ptr0", O_RDWR);
	if (fd == -1)
	{
		perror("open");
		return 1;
	};
	
	_glidix_ptrstate state;
	state.width = 640;
	state.height = 480;
	state.posX = 320;
	state.posY = 240;
	
	write(fd, &state, sizeof(_glidix_ptrstate));
	
	while (1)
	{
		ssize_t sz = read(fd, &state, sizeof(_glidix_ptrstate));
		if (sz == -1)
		{
			perror("read");
		}
		else
		{
			printf("State: %dx%d, mouse at (%d, %d)\n", state.width, state.height, state.posX, state.posY);
		};
	};
	
	close(fd);
	return 0;
};
