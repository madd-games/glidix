#include <sys/ioctl.h>
#include <stdio.h>
#include <fcntl.h>
#include <unistd.h>
#include <stdlib.h>
#include <sys/mman.h>
#include <string.h>
#include <glidix/video.h>
#include <libddi.h>

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

	DDIPixelFormat format;
	format.bpp = 4;
	format.redMask = 0xFF0000;
	format.greenMask = 0x00FF00;
	format.blueMask = 0x0000FF;
	format.alphaMask = 0;
	format.pixelSpacing = 0;
	format.scanlineSpacing = 0;
	DDISurface *screen = ddiCreateSurface(&format, mode.width, mode.height, videoram, DDI_STATIC_FRAMEBUFFER);

	DDISurface *back = ddiCreateSurface(&format, mode.width, mode.height, NULL, 0);
	DDIColor color = {0x33, 0x33, 0xAA, 0xFF};
	printf("Drawing rectangle in back buffer..\n");
	ddiFillRect(back, 0, 0, mode.width, mode.height, &color);
	
	printf("Blitting rectangle to screen...\n");
	ddiBlit(back, 0, 0, screen, 0, 0, mode.width, mode.height);
	
	printf("done.\n");
	while (1);

	close(fd);
	return 0;
};
