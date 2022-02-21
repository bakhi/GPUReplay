#include <linux/mm.h>
#include <linux/slab.h>
#include <linux/dma-mapping.h>
#include <linux/gfp.h>	// __get_dma_pages
#include <linux/version.h>
#include "include/gr_mempool.h"

#define GR_DMA

static inline dma_addr_t gr_dma_addr(struct page *p)
{
	if (sizeof(dma_addr_t) > sizeof(p->private))
		return ((dma_addr_t)page_private(p)) << PAGE_SHIFT;
	return (dma_addr_t)page_private(p);
}

static inline size_t gr_mempool_size(struct gr_mempool *pool)
{
	return pool->cur_size;
}

static bool gr_mempool_is_empty(struct gr_mempool *pool)
{
	return gr_mempool_size(pool) == 0;
}

static void gr_mempool_add(struct gr_mempool *pool, struct page *p)
{
	list_add(&p->lru, &pool->page_list);
	pool->cur_size++;
	pool->tot_size++;
}

static struct page *gr_mem_alloc_page(struct gr_mempool *pool)
{
	gfp_t gfp = 0;

#if defined(config_ARM) && !defined(CONFIG_HAVE_DMA_ATTRS) && \
	LINUX_VERSION_CODE < KERNEL_VERSION(3, 5, 0)
	gfp = GFP_USER | __GFP_ZERO;
#else
	gfp = GFP_HIGHUSER | __GFP_ZERO;
#endif

	/* don't warn on higher order failures */
	if (pool->order)
		gfp |= __GFP_NOWARN;

	return alloc_pages(gfp, pool->order);
}

static void gr_mempool_free_page(struct gr_mempool *pool, struct page *p)
{
#ifdef GR_DMA
	dma_addr_t dma_addr;
	struct gr_device *const grdev = pool->grdev;
	struct device *const dev = grdev->dev;

	dma_addr = gr_dma_addr(p);

	dma_unmap_page(dev, dma_addr, (PAGE_SIZE << pool->order), DMA_BIDIRECTIONAL);
	/** printk(KERN_INFO "[Free] pa: 0x%lx da: 0x%lx", page_to_phys(p), dma_addr); */
	ClearPagePrivate(p);
#endif
	__free_pages(p, pool->order);
}


static struct page *gr_mempool_remove(struct gr_mempool *pool)
{
	struct page *p;

	if (gr_mempool_is_empty(pool))
		return NULL;

	p = list_first_entry(&pool->page_list, struct page, lru);
	list_del_init(&p->lru);
	pool->cur_size--;

	list_add(&p->lru, &pool->used_list);

	return p;
}

struct page *gr_mempool_alloc(struct gr_mempool *pool)
{
	struct page *p;
	p = gr_mempool_remove(pool);

	/** printk(KERN_INFO "alloc page (pa : 0x%llx)", page_to_phys(p)); */
	if (p) return p;
	return NULL;
}

// XXX add lock
int gr_mempool_grow(struct gr_mempool *pool, size_t nr_to_grow)
{
	struct page *p;
	size_t i;
	dma_addr_t dma_addr;

	struct device *const dev = pool->grdev->dev;

	for (i = 0; i < nr_to_grow; i++) {
		p = gr_mem_alloc_page(pool);
#ifdef GR_DMA
		dma_addr = dma_map_page(dev, p, 0, (PAGE_SIZE << pool->order), DMA_BIDIRECTIONAL);
		/** printk(KERN_INFO "[Alloc] pa: 0x%lx da: 0x%lx", page_to_phys(p), dma_addr); */

		if (dma_mapping_error(dev, dma_addr)) {
			gr_mempool_free_page(pool, p);
			printk(KERN_INFO "gr: dma mapping error");
			return -1;
		} else {
			SetPagePrivate(p);
			if (sizeof(dma_addr_t) > sizeof(p->private))
				set_page_private(p, dma_addr >> PAGE_SHIFT);
			else {
				set_page_private(p, dma_addr);
			}
		}
#endif

		if (!p) {
			return -ENOMEM;
		}

		gr_mempool_add(pool, p);
		/** printk(KERN_INFO "add page to the pool (pa : 0x%llx)", page_to_phys(p)); */
	}
	return 0;
}

struct gr_mempool *gr_mempool_init(unsigned int order)
{
	struct gr_mempool *pool;

	pool = (struct gr_mempool *) kmalloc(sizeof(struct gr_mempool), GFP_KERNEL);

#ifdef DEBUG
	printk("[%s]", __func__);
#endif
	pool->order = order;
	pool->cur_size = 0;
	pool->tot_size = 0;
	INIT_LIST_HEAD(&pool->page_list);
	INIT_LIST_HEAD(&pool->used_list);

	return pool;
}

void gr_mempool_term(struct gr_mempool *pool)
{
	struct page *p, *tmp;
	LIST_HEAD(free_list);

#ifdef DEBUG
	printk(KERN_INFO "[%s] mempool tot size: %lu cur_size: %lu\n", __func__, pool->tot_size, pool->cur_size);
#endif

	while (!gr_mempool_is_empty(pool)) {
		/* Free remaining pages to kernel */
		p = gr_mempool_remove(pool);
		list_add(&p->lru, &free_list);
	}

	list_for_each_entry_safe(p, tmp, &free_list, lru) {
		/** printk(KERN_INFO "[TERM_FREE_LIST] page (pa : 0x%llx)", page_to_phys(p)); */
		list_del_init(&p->lru);
		gr_mempool_free_page(pool, p);
	}

	list_for_each_entry_safe(p, tmp, &pool->used_list, lru) {
		/** printk(KERN_INFO "[TERM_USED_LIST] page (pa : 0x%llx)", page_to_phys(p)); */
		list_del_init(&p->lru);
		gr_mempool_free_page(pool, p);
	}

	kfree(pool);
}

void gr_mem_sync(struct gr_device *grdev, struct gr_ioctl_mem_sync *sync) {
	phys_addr_t phys_addr = sync->handle;
	struct page *p = pfn_to_page(phys_addr >> PAGE_SHIFT);
	dma_addr_t dma_addr = gr_dma_addr(p);
	size_t size = sync->size;

	dma_sync_single_for_cpu(grdev->dev, dma_addr, size, DMA_BIDIRECTIONAL);
	printk(KERN_INFO "[sync done] paddr: 0x%llx", phys_addr);

}
