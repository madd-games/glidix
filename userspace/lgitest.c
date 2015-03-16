#include <sys/ioctl.h>
#include <stdio.h>
#include <fcntl.h>
#include <unistd.h>
#include <stdlib.h>
#include <sys/mman.h>
#include <string.h>
#include <glidix/video.h>

int main(int argc, char *argv[])
{
	int fd = open("/dev/bga", O_RDWR);
	if (fd == -1)
	{
		fprintf(stderr, "failed to open /dev/bga\n");
		return 1;
	};

	LGIDisplayDeviceStat	dstat;
	LGIDisplayMode		mode;

	if (ioctl(fd, IOCTL_VIDEO_DEVSTAT, &dstat) != 0)
	{
		fprintf(stderr, "devstat failed\n");
		close(fd);
		return 1;
	};

	printf("this device supports %d display modes\n", dstat.numModes);
	printf("#\tRes\n");
	for (mode.index=0; mode.index<dstat.numModes; mode.index++)
	{
		ioctl(fd, IOCTL_VIDEO_MODSTAT, &mode);
		printf("%d\t%dx%d\n", (int)mode.index, (int)mode.width, (int)mode.height);
	};

	mode.index = 1;
	ioctl(fd, IOCTL_VIDEO_MODSTAT, &mode);

	printf("Switching to: %dx%d\n", mode.width, mode.height);
	ioctl(fd, IOCTL_VIDEO_SETMODE, &mode);

	size_t size = mode.width * mode.height * 4;

	printf("Trying to map video memory\n");
	char *videoram = (char*) mmap(NULL, size, PROT_WRITE, MAP_SHARED, fd, 0);
	if (videoram == ((char*)-1))
	{
		printf("failed\n");
		while (1);
	};

	printf("Loaded at: %p\n", videoram);
	char *data = (char*) malloc(size);
	memset(data, 0xFF, size);

	printf("Literally spamming.\n");
	memcpy(videoram, data, size);

	printf("done.\n");
	while (1);

	close(fd);
	return 0;
};
