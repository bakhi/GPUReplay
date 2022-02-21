#include "mali_kbase.h"
#include "gr_defs.h"
#include "measure.h"

#include <linux/kernel.h>
#include <linux/init.h>
#include <linux/syscalls.h>
#include <linux/fcntl.h>
#include <asm/uaccess.h>

#include <linux/delay.h>

#ifdef CONFIG_GR

/********************** file ops *************************/

// While inadvisable, just try to dump driver instrument using file IO in user space
// sys_open doesn't work
// https://sonseungha.tistory.com/137
// use filp_open
// https://stackoverrun.com/ko/q/214960
struct file *file_open(const char *path, int flags, int rights)
{
	struct file *filp = NULL;
	mm_segment_t oldfs;
	int err = 0;

	oldfs = get_fs();
	set_fs(get_ds());
	filp = filp_open(path, flags, rights);
	set_fs(oldfs);
	if (IS_ERR(filp)) {
		err = PTR_ERR(filp);
		return NULL;
	}
	return filp;
}

static int file_write(struct file *file, unsigned long long *offset, void *data, unsigned int size)
{
	mm_segment_t oldfs;
	int ret;
	oldfs = get_fs();
	set_fs(get_ds());

	ret = kernel_write(file, data, size, offset);

	set_fs(oldfs);
	return ret;
}

static void file_close(struct file *file)
{
	filp_close(file, NULL);
}

/******************** end of file ops *********************/

#ifdef CONFIG_GR_IO_HISTORY
int gr_io_history_init(struct kbase_io_history *h, u16 new_size)
{
	struct kbase_io_access *new_buf;
	unsigned long flags;

	h->enabled = true;
	spin_lock_init(&h->lock);

	if (!new_size)
		EE("size equals to 0");
	
	new_buf = vmalloc(new_size * sizeof(*h->buf));

	if (!new_buf)
		EE("history buffer is NULL");
	
	spin_lock_irqsave(&h->lock, flags);
	
	h->count = 0;
	h->size = new_size;	// nr of io access
	h->buf = new_buf;

	spin_unlock_irqrestore(&h->lock, flags);
	
	return 0;
}

void gr_io_history_add(struct kbase_io_history *h,
		u32 offset, u32 value, u8 write)
{
	struct kbase_io_access *io;
	unsigned long flags;
#ifdef CONFIG_GR_IO_HISTORY
	struct timespec ts;
#endif
	
	spin_lock_irqsave(&h->lock, flags);

	io = &h->buf[h->count % h->size];
	io->addr = (uintptr_t)offset | write;
	io->value = value;

#ifdef CONFIG_GR_IO_HISTORY
	getrawmonotonic(&ts);
	io->timestamp = (u64)ts.tv_sec * NSECS_IN_SEC + ts.tv_nsec;
	io->thread_id = task_pid_nr(current);
#endif

	++h->count;
	/* If count overflows, move the index by the buffer size so the entire
	 * buffer will still be dumped later */
	if (unlikely(!h->count))
		h->count = h->size;

	spin_unlock_irqrestore(&h->lock, flags);
}

void gr_io_history_dump(struct kbase_device *kbdev)
{
	struct kbase_io_history *h = &kbdev->gr_io_history;
	struct kbase_io_access *io;
	char access;
	u32 offset;
	/** uintptr_t offset; */
	char buffer[256];
	s32 written = 0;
	u64 timestamp;

	u64 prev_ts;
	u32 i;
	size_t iters;

	DD("kbase destory context");
	DD("count : %lu size: %u", h->count, h->size);
	iters = (h->size > h->count) ? h->count : h->size;

	if (iters > 0) {
		io = &h->buf[(h->count - iters) % h->size];
		prev_ts = io->timestamp;
	}

	/* format: elapsed_time, read or write, offset, value
	 * e.g 0,R,0x00000000,60a00002
	 */
	for (i = 0; i < iters; ++i) {
		io = &h->buf[(h->count - iters + i) % h->size];
		access = (io->addr & 1) ? 'W' : 'R';
		offset = (u32)io->addr & ~0x1;
		written = 0;

		timestamp = io->timestamp - prev_ts;
		written = snprintf(buffer, 256, "%llu,%c,%x,%x\n", timestamp, access, offset, io->value);
		file_write(kbdev->gr_io_file, &kbdev->gr_io_file_offset, buffer, written);

		prev_ts = io->timestamp;
	}
}

#endif	// CONFIG_GR_IO_HISTORY

int gr_context_init(struct kbase_context *kctx)
{
	struct gr_context *gr_ctx;
	gr_ctx = vzalloc(sizeof(*gr_ctx));
	if (WARN_ON(!gr_ctx))
		return -1;

	if (gr_ctx != NULL) {
		VV("gpuReplay context init success");
		VV("kctx : %p, gr_ctx : %p", (void *)kctx, (void *)gr_ctx);
		INIT_LIST_HEAD(&gr_ctx->as_list);
		INIT_LIST_HEAD(&gr_ctx->sync_as_list);
	}

	atomic_set(&gr_ctx->tmmu.pgt_nr_pages, 0);

	gr_ctx->tmmu.buf_size 		= 0;
	gr_ctx->kctx 				= kctx;
	kctx->gr_ctx 				= gr_ctx;
	gr_ctx->as_cnt				= 0;

	// jin: memory dump file open
	gr_ctx->mem_dump_file 			= file_open("/home/linaro/gr_mem_contents", O_WRONLY | O_CREAT, 0644);
	gr_ctx->mem_dump_file_offset	= 0;

	// jin: format = vm_start (u64), vm_end (u64), size (size_t)
	gr_ctx->sync_as_file 			= file_open("/home/linaro/gr_sync_as", O_WRONLY | O_CREAT, 0644);
	gr_ctx->sync_as_file_offset 	= 0;
	gr_ctx->sync_as_cnt 			= 0;

	/** EE("mem_dump_file: %p, sysn_as_file: %p", gr_ctx->mem_dump_file, gr_ctx->sync_as_file); */
	return 0;
}

void gr_context_term(struct kbase_context *kctx)
{
	struct gr_context *gr_ctx = kctx->gr_ctx;
	struct gr_as *tas;
	struct gr_sync_as *sync_as;

	JJ("free GR context");
	file_close(gr_ctx->mem_dump_file);
	file_close(gr_ctx->sync_as_file);

	list_for_each_entry(tas, &gr_ctx->as_list, link) {
		kfree(tas);
	}

	list_for_each_entry(sync_as, &gr_ctx->sync_as_list, link) {
		kfree(sync_as);
	}

	vfree(gr_ctx);
}

// jin: assume that only single thread from userspace can access it.
// otherwise we should lock before add as
int gr_as_add_sync(struct gr_context *gr_ctx, struct basep_syncset const *sset, size_t offset)
{
	size_t sync_size = (size_t) sset->size;
	u64 sync_vm_start = sset->user_addr;
	u64 sync_vm_end = sync_vm_start + sync_size;
	struct gr_sync_as *sync_as;
	u8 is_exist = 0;

	list_for_each_entry(sync_as, &gr_ctx->sync_as_list, link) {
		if (sync_as->vm_start == sync_vm_start && sync_as->vm_end == sync_vm_end) {
			is_exist = 1;
			WW("[exist] vm_start: 0x%llx, vm_end: 0x%llx, size: %lu", sync_vm_start, sync_vm_end, sync_size);
			break;
		}
	}
	
	if (!is_exist) {
		WW("[Not exist] vm_start: 0x%llx, vm_end: 0x%llx, size: %lu", sync_vm_start, sync_vm_end, sync_size);
		sync_as = kzalloc(sizeof(*sync_as), GFP_KERNEL);
		if (WARN_ON(!sync_as))
			return GR_FAIL;

		sync_as->vm_start 	= sync_vm_start;
		sync_as->vm_end 	= sync_vm_end;
		sync_as->size 		= sync_size;

		gr_ctx->sync_as_cnt++;

		INIT_LIST_HEAD(&sync_as->link);

		list_add_tail(&sync_as->link, &gr_ctx->sync_as_list);
	}

	switch (sset->type) {
	case BASE_SYNCSET_OP_MSYNC:
		sync_as->cpu_to_gpu++;
		break;
	case BASE_SYNCSET_OP_CSYNC:
		sync_as->gpu_to_cpu++;
		break;
	default:
		EE("[ERROR] Unkown msync op %d", sset->type);
		return GR_FAIL;
	}

	return GR_SUCCESS;
}

int gr_as_add_valid(struct gr_context *gr_ctx, struct vm_area_struct *vma, size_t nents)
{
	struct gr_as *tas;

	list_for_each_entry(tas, &gr_ctx->as_list, link) {
		if (tas->vm_start == vma->vm_start && tas->vm_end == vma->vm_end) {
			tas->is_valid = 1;
			tas->nents = nents;
			break;
		}
	}
	return GR_SUCCESS;
}

int gr_as_add(struct gr_context *gr_ctx, struct vm_area_struct *vma, struct kbase_va_region *region)
{
	struct gr_as *tas;

	tas = kzalloc(sizeof(*tas), GFP_KERNEL);
	if (WARN_ON(!tas))
		return GR_FAIL;

   /**  tas->cpu_va = cpu_va; */
	/** tas->gpu_va = gpu_va; */
	tas->vm_start = vma->vm_start;
	tas->vm_end = vma->vm_end;
	tas->start_pfn = region->start_pfn;
	tas->nr_pages = region->nr_pages;
	tas->flags = region->flags;
	tas->is_valid = 0;
	tas->cpu_type = region->cpu_alloc->type;
	tas->gpu_type = region->gpu_alloc->type;

	tas->reg = region;

	/** NOTE:
	  * reg->initial_commit: initial commit when kbase_mem_alloc is invoked
	  * va_pages: size of vm region in pages
	  * alloc->nents: actual allocated pages which can be updated when ioctl_commit runs -> vm_page_fault() */
	tas->nents = region->cpu_alloc->nents;
	tas->is_dumped = 0;

	/** EE("region->start_pfn : %llx", region->start_pfn); */

#if 0
	struct kbase_mem_phy_alloc *cpu_alloc = tas->reg->cpu_alloc;
	struct kbase_mem_phy_alloc *gpu_alloc = tas->reg->gpu_alloc;

	phys_addr_t cpu_pa = as_phys_addr_t(cpu_alloc->pages[0]);
	phys_addr_t gpu_pa = as_phys_addr_t(gpu_alloc->pages[0]);

	EE("addr: %llx cpu_pa : %llx gpu_pa : %llx", tas->vm_start, cpu_pa, gpu_pa);
	EE("[gpuReplay] vm_start : %llx, vm_end : %llx", tas->vm_start, tas->vm_end);
#endif
	/** EE("[gpuReplay] as_cnt: %d, vm_start : %llx, vm_end : %llx", gr_ctx->as_cnt, tas->vm_start, tas->vm_end); */

	INIT_LIST_HEAD(&tas->link);

	if (!(tas->flags & KBASE_REG_GPU_NX)) {
		// jin: GPU_BINARY region does not have GPU_NX flag
		tas->as_type = GR_AS_TYPE_GPU_BINARY;
	} else if (gr_ctx->as_cnt > 5 && tas->nr_pages == 1 && !(tas->flags & KBASE_REG_CPU_CACHED)) {
		// jin: compute job region has no CPU_CACHE flag and its nr_pages is 1
		tas->as_type = GR_AS_TYPE_COMPUTE_JOB;
	} else if (tas->flags & KBASE_REG_CPU_CACHED && tas->flags & KBASE_REG_GPU_CACHED) {
		// jin: in/output region has both CPU and GPU CACHED flags
		tas->as_type = GR_AS_TYPE_INOUT;
	} else {
		tas->as_type = GR_AS_TYPE_UNKNOWN;
	}

	if (gr_ctx) {
		/** EE("[gpuReplay] list add tail"); */
		tas->as_num = gr_ctx->as_cnt++;
		list_add_tail(&tas->link, &gr_ctx->as_list);
	}

	return GR_SUCCESS;
}

void gr_print_reg_flags(unsigned int flags)
{
	switch (flags & KBASE_REG_ZONE_MASK) {
		case KBASE_REG_ZONE_CUSTOM_VA:
			printk(KERN_CONT "ZONE_CUSTOME_VA | ");
			break;
		case KBASE_REG_ZONE_SAME_VA:
			printk(KERN_CONT "ZONE_SAME_VA | ");
			break;
		case KBASE_REG_ZONE_EXEC_VA:
			printk(KERN_CONT "ZONE_EXEC_VA | ");
			break;
		default:
			break;
	}

	if (flags & KBASE_REG_FREE)
		printk(KERN_CONT "FREE | ");
	if (flags & KBASE_REG_CPU_WR)
		printk(KERN_CONT "CPU_WR | ");
	if (flags & KBASE_REG_CPU_RD)
		printk(KERN_CONT "CPU_RD | ");
	if (flags & KBASE_REG_GPU_WR)
		printk(KERN_CONT "GPU_WR | ");
	if (flags & KBASE_REG_GPU_RD)
		printk(KERN_CONT "GPU_RD | ");
	if (flags & KBASE_REG_GPU_NX)
		printk(KERN_CONT "GPU_NX | ");
	if (flags & KBASE_REG_NO_USER_FREE)
		printk(KERN_CONT "NO_USER_FREE | ");
	if (flags & KBASE_REG_CPU_CACHED)
		printk(KERN_CONT "CPU_CACHED | ");
	if (flags & KBASE_REG_GPU_CACHED)
		printk(KERN_CONT "GPU_CACHED | ");
}

int gr_print_as(struct gr_context *gr_ctx, int is_forced) {
	struct gr_as *tas;
	int cnt = 0;
	static int is_done = 0;
	
	// jin: note that reg->cpu_alloc would be freed so that it no longer contains paddrs used for that region
	// This is why vmap failed...
	if (is_done < 1 || is_forced) {	// for va, conv
		is_done++;

		if (!gr_ctx)
			return GR_FAIL;

		list_for_each_entry(tas, &gr_ctx->as_list, link) {
			EE("[AS:%d type: %d] start: %llx, end: %llx, flags: %x, valid: %u, nr_pages: %lu, nents: %lu", \
					cnt, tas->as_type, tas->vm_start, tas->vm_end, tas->flags, tas->is_valid, tas->nr_pages, tas->nents);
			gr_print_reg_flags(tas->flags);
			/** EE("cpu_type : %d gpu_type : %d", (int)tas->cpu_type, (int)tas->gpu_type); */
			cnt++;
			}
		} else {

		}
	return GR_SUCCESS;

}

void gr_mmu_dump(struct gr_context *gr_ctx, struct kbase_context *kctx) {
	struct gr_mmu *tmmu = &gr_ctx->tmmu;
	char *buffer;
	char *mmu_dump_buffer;
	u64 *tmp_buffer;
	struct kbase_mmu_setup as_setup;

	struct file* file;
	size_t size_left = 0, dump_size = 0;
	u64 end_marker = 0xFFFFFFFFFFULL;
	u64 offset = 0;

	size_t pgt_nr_pages = 0;

	static int is_done = 0;

	if (!is_done) {

		kctx->kbdev->mmu_mode->get_as_setup(&kctx->mmu, &as_setup);

		pgt_nr_pages = atomic_read(&tmmu->pgt_nr_pages);
		//	pgt_nr_pages += 1; 	// add one for level 0 pgd
		//	tmmu->pgt_nr_pages += 1;	// add one for level 0 pgd

		/** calculate buffer size*/
		size_left += sizeof(u64);					// size
		size_left += sizeof(u64) * 3;				// for config
		size_left += sizeof(u64) * pgt_nr_pages;	// for pgd, pgd, pmd
		size_left += pgt_nr_pages * PAGE_SIZE;		// for entries
		size_left += sizeof(u64);					// for end_marker

		DD("size_left : %lu", size_left);
		DD("[%s] pgd : 0x%0llx", __func__, tmmu->pgd);
		DD("[%s] nr_pages for pgt : %lu", __func__, pgt_nr_pages);

		tmmu->transtab = as_setup.transtab;
		tmmu->memattr = as_setup.memattr;
		tmmu->transcfg = as_setup.transcfg;

		buffer = (char *) vzalloc(size_left);
		mmu_dump_buffer = buffer;
		tmp_buffer = (u64 *) buffer;

		memcpy(mmu_dump_buffer, &size_left, sizeof(u64));
		mmu_dump_buffer += sizeof(size_t);
		memcpy(mmu_dump_buffer, &as_setup.transtab, sizeof(u64));
		mmu_dump_buffer += sizeof(u64);
		memcpy(mmu_dump_buffer, &as_setup.memattr, sizeof(u64));
		mmu_dump_buffer += sizeof(u64);
		memcpy(mmu_dump_buffer, &as_setup.transcfg, sizeof(u64));
		mmu_dump_buffer += sizeof(u64);

		size_left -= sizeof(size_t) + sizeof(u64) * 3;
		dump_size += sizeof(size_t) + sizeof(u64) * 3;		// confg

		dump_size += gr_mmu_dump_level(kctx,
				kctx->mmu.pgd,
				MIDGARD_MMU_TOPLEVEL,
				&mmu_dump_buffer,
				&size_left);

		if (size_left != sizeof(u64)) {
			EE("size left (%lu) is not euqal to config and endmarker", size_left);
		}

		memcpy(mmu_dump_buffer, &end_marker, sizeof(u64));
		mmu_dump_buffer += sizeof(u64);
		size_left -= sizeof(u64);
		dump_size += sizeof(u64);		// end marker

		JJ("dump_size : %lu", dump_size);

		// open file to dump page table
		file = file_open("/home/linaro/gr_pgt", O_WRONLY | O_CREAT, 0644);

		file_write(file, &offset, buffer, dump_size);

		file_close(file);
		vfree(buffer);

		is_done++;
	}
}

/* 
 * Format:
 * vaddr(u64)
 * paddr(u64)
 * contents(PAGE_SIZE) 512 entries
 *
 * Thought: Caller dump memory contents right before the job submission
 * 			It would be fine without sync (cache flush) as we access cpu vaddr.
 *			Even in case contents is cached, CPU knows the correct contents.
*/
int gr_dump_phys_pages(struct kbase_context *kctx, struct file *file, u64 *offset, struct gr_as *tas, int is_mapped)
{
	struct kbase_mem_phy_alloc *cpu_alloc = tas->reg->cpu_alloc;
	struct kbase_mem_phy_alloc *gpu_alloc = tas->reg->gpu_alloc;
	/** phys_addr_t cpu_pa = as_phys_addr_t(cpu_alloc->pages[0]); */
	size_t i;
	u64 *buffer;
	u64 vaddr, paddr;
	
	buffer = (u64 *) tas->vm_start;

	if (!file || !offset) {
		EE("no file pointer or offset");
		return GR_FAIL;
	}

	JJ("vm_start: 0x%llx vm_end: 0x%llx nents: %lu", tas->vm_start, tas->vm_end, tas->nents);
	for (i = 0; i < tas->nents; i++) {
		//jin: must map paddr. Guess: at some point, cpu mapping is freed? cannot directly access cpu vaddr
		struct page *p;
		u64 *mapped_page;

		vaddr = tas->vm_start + (PAGE_SIZE * i);
		paddr = as_phys_addr_t(cpu_alloc->pages[i]);

		buffer = (u64 *) vaddr;
		/** EE("vaddr: %llx, paddr: %llx, nents: %llu buffer: %llx", vaddr, paddr, tas->nents, buffer); */

		p = as_page(cpu_alloc->pages[i]);

		// sync cpu to mem
		if (likely(cpu_alloc == gpu_alloc)) {
			dma_addr_t dma_addr;

			dma_addr = kbase_dma_addr(pfn_to_page(PFN_DOWN(paddr)));
			dma_sync_single_for_device(kctx->kbdev->dev, dma_addr, PAGE_SIZE, DMA_BIDIRECTIONAL);
			/** dma_sync_single_for_cpu(kctx->kbdev->dev, dma_addr, PAGE_SIZE, DMA_BIDIRECTIONAL); */
		} else {
			/** TODO: handle cpu_pa != gpu_pa */
		}

		mapped_page = kmap(p);
		// XXX should sync page to cpu?
		// if needed, use kbase_sync_single()

		file_write(file, offset, &vaddr, sizeof(u64));
		file_write(file, offset, &paddr, sizeof(u64));
		file_write(file, offset, mapped_page, PAGE_SIZE);

		/** if (tas->nents != 513 && tas->nents != 64) { */
		if (tas->nents == 1 || tas->as_num == 0 || i == 0) {
			if(is_mapped == 1) {
				JJ("----------- buffer (%lln) ------------ ", buffer);
				/** hex_dump((void *) buffer, 128); */
			} else if(is_mapped == 0) {
				JJ("----------- mapped_page (%lln)------------ ", mapped_page);
				/** hex_dump((void *) mapped_page, 128); */
			} else {
				JJ("----------- buffer (%lln) ------------ ", buffer);
				/** hex_dump((void *) mapped_page, 4096); */
			}
		}
		kunmap(p);
	}
	return GR_SUCCESS;
}

void gr_dump_sync_as(struct gr_context *gr_ctx) {
	struct file *file = gr_ctx->sync_as_file;
	struct gr_sync_as *sync_as;
	u64 *offset = &gr_ctx->sync_as_file_offset;

	file_write(file, offset, &gr_ctx->sync_as_cnt, sizeof(u32));
	JJ("[gr_dump_sync_as] total count of sync_as: %u", gr_ctx->sync_as_cnt);

	list_for_each_entry(sync_as, &gr_ctx->sync_as_list, link) {
		WW("vm_start: 0x%016llx, vm_end: 0x%016llx, size: %lu", \
			sync_as->vm_start, sync_as->vm_end, sync_as->size);
		file_write(file, offset, &sync_as->vm_start, sizeof(u64));
		file_write(file, offset, &sync_as->vm_end, sizeof(u64));
		file_write(file, offset, &sync_as->size, sizeof(size_t));
	}
}

static void _gr_dump_mc(struct gr_context *gr_ctx, struct gr_as *tas) {
	struct file *file = gr_ctx->mem_dump_file;
	u64 *offset = &gr_ctx->mem_dump_file_offset;

	file_write(file, offset, &tas->vm_start, sizeof(u64));
	file_write(file, offset, &tas->vm_end, sizeof(u64));
	file_write(file, offset, &tas->nents, sizeof(size_t));
	file_write(file, offset, &tas->flags, sizeof(uint32_t));
	file_write(file, offset, &tas->is_valid, sizeof(uint8_t));

	if (tas->is_valid) {
		gr_dump_phys_pages(gr_ctx->kctx, file, offset, tas, 1);
		JJ("[TOUCHED][AS:%d SUCCESS] offset = %llu", tas->as_num, *offset);
	} else {
		JJ("[GPU INTERNAL][AS:%d SUCCESS] offset = %llu", tas->as_num, *offset);
	}

	tas->is_dumped = 1;
}

static void gr_dump_mc_type(struct gr_context *gr_ctx, struct gr_as *tas, enum gr_as_type as_type) {
	switch (as_type) {
		case GR_AS_TYPE_COMPUTE_JOB:
		case GR_AS_TYPE_GPU_BINARY:
			if (tas->as_type == GR_AS_TYPE_COMPUTE_JOB || tas->as_type == GR_AS_TYPE_GPU_BINARY) {
				_gr_dump_mc(gr_ctx, tas);
			}
			break;
		case GR_AS_TYPE_ALL_WO_COMPUTE_JOB:
		case GR_AS_TYPE_UNKNOWN:
		case GR_AS_TYPE_INOUT:
			if (tas->as_type != GR_AS_TYPE_COMPUTE_JOB && tas->as_type != GR_AS_TYPE_GPU_BINARY) {
				_gr_dump_mc(gr_ctx, tas);
			}
			break;
		default:
			_gr_dump_mc(gr_ctx, tas);
			break;
	}
}

void gr_dump_mc_invalid(struct gr_context *gr_ctx, enum gr_as_type as_type) {
	struct gr_as *tas;

	list_for_each_entry(tas, &gr_ctx->as_list, link) {
		if (!tas->is_dumped && !tas->is_valid) {
			gr_dump_mc_type(gr_ctx, tas, as_type);
		}
	}
}
void gr_dump_mc_valid(struct gr_context *gr_ctx, enum gr_as_type as_type) {
	struct gr_as *tas;

	/** kbase_gpu_vm_lock(gr_ctx->kctx); */
	/** mutex_lock(&gr_ctx->mutex); */
	list_for_each_entry(tas, &gr_ctx->as_list, link) {
		if (!tas->is_dumped && tas->is_valid) {
			gr_dump_mc_type(gr_ctx, tas, as_type);
		}
	}
	/** mutex_unlock(&gr_ctx->mutex); */
	/** kbase_gpu_vm_unlock(gr_ctx->kctx); */
}

#ifdef CONFIG_GR_REG_SNAPSHOT
void gr_reg_flush(struct kbase_device *kbdev) {
	struct file *file = kbdev->reg_dump_file;
	u64 *offset = &kbdev->reg_dump_file_offset;
	u32 *reg_snapshot = kbdev->reg_snapshot;

	/** jin: cannot dump files in atomic operation
	  * Fundamentally, cannot invoke potentially sleep operation (e.g. I/O) during atomic operation */
	if (!irqs_disabled()) {
		file_write(file, offset, reg_snapshot, kbdev->reg_size * kbdev->nr_reg_snapshots);
		kbdev->nr_reg_snapshots = 0;
		JJ("[Flush snapshots: %d ] reg dump offset: %llu", kbdev->reg_dump_cnt, *offset);

	} else {
		JJ("in IRQ, cannot dump reg map");
		/** kbdev->nr_reg_snapshots++; */
		if (kbdev->nr_reg_snapshots > kbdev->max_reg_snapshots)
		EE("GR BUG: not enough space to keep register snapshots");
	}
}

void gr_reg_dump(struct kbase_device *kbdev, u32 is_read, unsigned long flags) {
	u32 *reg_snapshot = kbdev->reg_snapshot;
	u32 i, value = 0;
	u32 cnt = kbdev->nr_reg_snapshots * (kbdev->reg_size / sizeof(u32));

	for (i = 0; i < kbdev->reg_size; i += sizeof(u32)) {
		value = 0;
		value = readl(kbdev->reg + i);
		reg_snapshot[cnt++] = value;
	}

	kbdev->nr_reg_snapshots++;
	kbdev->reg_dump_cnt++;
	/** JJ("[dump_cnt: %d ] is_read:%d reg dump offset: %llu", kbdev->reg_dump_cnt, is_read, *offset); */

	spin_unlock_irqrestore(&kbdev->reg_dump_lock, flags);

	gr_reg_flush(kbdev);
}
#endif	// CONFIG_GR_REG_SNAPSHOT

#endif	// CONFIG_GR
