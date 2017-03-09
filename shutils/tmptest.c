#include <stdio.h>

int main()
{
	uint16_t vendor=0, device=0;
	char devname[1024];
	
	int count = sscanf("1234:1111 bga", "%hx:%hx %s", &vendor, &device, devname);
	printf("vendor=%hX, device=%hX, devname=%s, count=%d\n", vendor, device, devname, count);
	return 0;
};
