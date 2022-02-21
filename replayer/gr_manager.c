// implemented by Heejin Park
// 07/20/2020

#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <sys/types.h>
#include <fcntl.h>
#include <sys/ioctl.h>
#include <poll.h>
#include "gr_manager.h"
#include "log.h"

int gr_manager_poll(enum gr_irq_type type, int timeout) {
	struct pollfd event;
	int retval = 0;
	
	/** event.fd = gr_handle; */
	switch (type) {
		case IRQ_TYPE_JOB:
			event.fd = job_irq_handle;
			break;
		case IRQ_TYPE_MMU:
			event.fd = mmu_irq_handle;
			break;
		case IRQ_TYPE_GPU:
			event.fd = gpu_irq_handle;
			break;
	}

	event.events = POLLIN;
	event.revents = 0;
	
	while (1) {
		retval = poll(&event, 1, timeout);

		if (retval == -1) {
			perror("poll : ");
			exit(EXIT_FAILURE);
		} else if (retval > 0) {
			if (event.revents & POLLIN) {
				break;
			}
		}
	}
	return retval;
}

void gr_manager_mem_sync(struct gr_ioctl_mem_sync *param) {
	ioctl(gr_handle, GR_IOCTL_MEM_SYNC, param);
}

uint64_t gr_manager_alloc_get_single_page() {
	struct gr_ioctl_get_page param;

	ioctl(gr_handle, GR_MEMPOOL_GET_PAGE, &param);
	return param.pa;
}

void gr_manager_alloc_pages(uint64_t nr_pages) {
	struct gr_ioctl_alloc_pages param;
	param.nr_pages = nr_pages;
	
	ioctl(gr_handle, GR_MEMPOOL_GROW, &param);
}

// caller should provide array pointer
uint64_t gr_manager_get_pages(uint64_t *pa, uint64_t nr_pages) {
	struct gr_ioctl_get_pages param;
	param.nr_pages = nr_pages;

	param.pa = pa;

	ioctl(gr_handle, GR_MEMPOOL_GET_PAGES, &param);
	return 1;
}

static void gr_manager_irq_setup(void) {
	struct gr_ioctl_irqfds param;
	ioctl(gr_handle, GR_IOCTL_IRQ_SETUP, &param);

	job_irq_handle = param.job_irq;
	mmu_irq_handle = param.mmu_irq;
	gpu_irq_handle = param.gpu_irq;
}

void gr_manager_init() {
	WW("init gr manager");
	gr_handle = open("/dev/gr", O_RDWR);

	gr_manager_irq_setup();
	xzl_bug_on_msg(gr_handle < 0, "GR manager kenrel module open error");

	ioctl(gr_handle, GR_MEMPOOL_INIT);
}

void gr_manager_term() {
	WW("terminate gr manager");
	ioctl(gr_handle, GR_MEMPOOL_TERM);
	close(gr_handle);
}

