/* vtswitch N: chvt, for images without the kbd package. Not shipped. */
#include <fcntl.h>
#include <linux/vt.h>
#include <stdio.h>
#include <stdlib.h>
#include <sys/ioctl.h>
int main(int argc, char **argv)
{
	int v = atoi(argv[1]), fd = open("/dev/tty0", O_RDWR);

	if (fd < 0 || ioctl(fd, VT_ACTIVATE, v) || ioctl(fd, VT_WAITACTIVE, v)) {
		perror("vtswitch");
		return 1;
	}
	return 0;
}
