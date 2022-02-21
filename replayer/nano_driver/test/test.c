#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <unistd.h>
#include <sys/types.h>
#include <fcntl.h>
#include <sys/ioctl.h>

struct gr_ioctl_get_page {
	uint64_t pa;
};

#define GR_IOCTL_TYPE	0x80
#define GR_GET_PAGE	_IOR(GR_IOCTL_TYPE, 1, struct gr_ioctl_get_page) 

int main(int argc, char** argv) {

	int fd;
	struct gr_ioctl_get_page param;

	fd = open("/dev/gr", O_RDWR);
	if (fd == -1) {
		printf("Kernel Module Open Error\n");
	} else { 
		printf("Kernel Module Open Success\n"); 
	}

	param.pa = 0;
	ioctl(fd, 0);
	ioctl(fd, GR_GET_PAGE, &param);
	printf("pa : 0x%llx\n", param.pa);

	close(fd); 
	return 0;
}
