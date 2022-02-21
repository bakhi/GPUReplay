#ifndef _GR_H_
#define _GR_H_

#include <linux/platform_device.h>
#include <linux/miscdevice.h>
#include <linux/list.h>
#include <linux/module.h>
#include <linux/atomic.h>

struct gr_ioctl_irqfds {
	unsigned int job_irq;
	unsigned int mmu_irq;
	unsigned int gpu_irq;
};

struct gr_ioctl_get_page {
	__u64 pa;
};

struct gr_ioctl_mem_sync {
	__u64 handle;
	__u64 user_addr;
	__u64 size;
	__u8 type;
	__u8 padding[7];
};


struct gr_ioctl_mem_alloc {
	__u64 nr_pages;	// number of pages to be allocated:w
};

struct gr_ioctl_get_pages {
	__u64 nr_pages;
	__u64 *pa;
};

struct gr_ioctl_wait_irq {
	__u64 count;
};

struct gr_device {
	struct device *dev;
	struct miscdevice mdev;

	wait_queue_head_t job_irq_queue;
	wait_queue_head_t mmu_irq_queue;
	wait_queue_head_t gpu_irq_queue;

	u64 reg_start;
	size_t reg_size;
};

struct gr_mempool {
	struct gr_device *grdev;
	size_t				cur_size;
	size_t				tot_size;
	size_t				max_size;
	u8					order;
	u8					group_id;
	spinlock_t			pool_lock;
	struct list_head	page_list;
	struct list_head	used_list;
};

#define GR_IOCTL_TYPE	0x80

#define GR_MEMPOOL_INIT		_IO(GR_IOCTL_TYPE, 0)
#define GR_MEMPOOL_GET_PAGE	_IOR(GR_IOCTL_TYPE, 1, struct gr_ioctl_get_page)
#define GR_MEMPOOL_TERM		_IO(GR_IOCTL_TYPE, 2)

#define GR_MEMPOOL_GET_PAGES	_IOWR(GR_IOCTL_TYPE, 6, struct gr_ioctl_get_pages)
#define GR_MEMPOOL_GROW		_IOR(GR_IOCTL_TYPE, 7, struct gr_ioctl_mem_alloc)

#define GR_IOCTL_MEM_SYNC	_IOW(GR_IOCTL_TYPE, 15, struct gr_ioctl_mem_sync)
#define GR_IOCTL_IRQ_SETUP	_IOR(GR_IOCTL_TYPE, 16, struct gr_ioctl_irqfds)

#endif
