#ifndef _GR_MANAGER_H_
#define _GR_MANAGER_H_

#include <stdint.h>

#define GR_IOCTL_TYPE           0x80
#define GR_MEMPOOL_INIT         _IO(GR_IOCTL_TYPE, 0)
#define GR_MEMPOOL_GET_PAGE     _IOR(GR_IOCTL_TYPE, 1, struct gr_ioctl_get_page)
#define GR_MEMPOOL_TERM         _IO(GR_IOCTL_TYPE, 2)

#define GR_MEMPOOL_GET_PAGES        _IOWR(GR_IOCTL_TYPE, 6, struct gr_ioctl_get_pages)
#define GR_MEMPOOL_GROW         _IOR(GR_IOCTL_TYPE, 7, struct gr_ioctl_alloc_pages)

#define GR_IOCTL_MEM_SYNC       _IOW(GR_IOCTL_TYPE, 15, struct gr_ioctl_mem_sync)
#define GR_IOCTL_IRQ_SETUP      _IOR(GR_IOCTL_TYPE, 16, struct gr_ioctl_irqfds)

int gr_handle;

int job_irq_handle;
int mmu_irq_handle;
int gpu_irq_handle;

enum gr_irq_type {
	IRQ_TYPE_JOB,
	IRQ_TYPE_MMU,
	IRQ_TYPE_GPU,
};

struct gr_ioctl_irqfds {
	int job_irq;
	int mmu_irq;
	int gpu_irq;
};

struct gr_ioctl_alloc_pages {
	uint64_t nr_pages;
};

struct gr_ioctl_get_page {
	uint64_t pa;
};

struct gr_ioctl_get_pages {
	uint64_t nr_pages;
	uint64_t *pa;
};

struct gr_ioctl_mem_sync {
	uint64_t handle;
	uint64_t user_addr;
	uint64_t size;
	uint8_t type;
	uint8_t padding[7];
};

int gr_manager_poll(enum gr_irq_type type, int timeout);
void gr_manager_init(void);
void gr_manager_term(void);
void gr_manager_mem_sync(struct gr_ioctl_mem_sync *param);
void gr_manager_alloc_pages(uint64_t nr_pages);

uint64_t gr_manager_alloc_get_single_page(void);
uint64_t gr_manager_get_pages(uint64_t *pa, uint64_t nr_pages);

#endif
