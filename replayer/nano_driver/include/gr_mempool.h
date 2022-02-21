
#ifndef _GR_MEMPOOL_H_
#define _GR_MEMPOOL_H_

#include "gr.h"

int gr_mempool_grow(struct gr_mempool *pool, size_t nr_to_grow);
struct page *gr_mempool_alloc(struct gr_mempool *pool);
struct gr_mempool *gr_mempool_init(unsigned int order);
void gr_mempool_term(struct gr_mempool *pool);
void gr_mem_sync(struct gr_device *grdev, struct gr_ioctl_mem_sync *sync);

#endif
