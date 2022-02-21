#ifndef _GR_DEFS_H
#define _GR_DEFS_H

#define GR_FAIL	0
#define GR_SUCCESS 1

#ifdef CONFIG_GR_IO_HISTORY
#define GR_DEFAULT_REGISTER_HISTORY_SIZE ((u16)(1 << 15))

/* The number of nanoseconds in a second. */
#define NSECS_IN_SEC       1000000000ull /* ns */
#endif

enum gr_as_type {
	GR_AS_TYPE_UNKNOWN,
	GR_AS_TYPE_GPU_BINARY,
	GR_AS_TYPE_INOUT,
	GR_AS_TYPE_ALL_WO_COMPUTE_JOB,
	GR_AS_TYPE_COMPUTE_JOB,
	GR_AS_TYPE_ALL,
};

struct gr_mmu {
	phys_addr_t pgd;
	u64 transtab;
	u64 memattr;
	u64 transcfg;
	atomic_t pgt_nr_pages; 	// # of valid pages used for page table
//	size_t pgt_nr_pages;	// # of valid pages used for page table
	size_t buf_size;
	char *buf;	// dump buffer
};

struct gr_as {
	u64 vm_start;
	u64 vm_end;
	u64 start_pfn;			/* The PFN in GPU space */
	size_t nr_pages;		/* # of pages */
	size_t nents;
	unsigned int flags;		/* region flgas */
	uint8_t is_valid;

	uint32_t as_num;		/* as number */

	/* jin: gr_flgas used to
	 * 		1) check the the region is valid or not
	 *		2) the region is used for either input or output
	 */
	uint8_t gr_flags;		/* gpuReplay specific flags used for special purpose */
	uint8_t is_dumped;		/* flag indicates if AS is dumped */

	enum kbase_memory_type cpu_type;
	enum kbase_memory_type gpu_type;

	struct kbase_va_region *reg;
	enum gr_as_type as_type;

	struct list_head link;
};

struct gr_sync_as {
	uint64_t vm_start;
	uint64_t vm_end;
	size_t size;
	u8 cpu_to_gpu;
	u8 gpu_to_cpu;

	struct list_head link;
};

struct gr_context {
	struct gr_as tas_tmp;
	struct list_head as_list;
	u32 as_cnt;
	struct gr_mmu tmmu;
	struct kbase_context *kctx;

	// about dump sync_as
	struct list_head sync_as_list;
	u32 sync_as_cnt;
	struct file *sync_as_file;
	u64 sync_as_file_offset;

	// about dump va region memory
	struct file *mem_dump_file;
	u64 mem_dump_file_offset;

#ifdef CONFIG_GR_REG_SNAPSHOT
	// about dump register map
	struct file *reg_dump_file;
	u64 reg_dump_file_offset;
#endif

#ifdef CONFIG_GR_IO_HISTORY
	struct kbase_io_history io_history;
#endif

	struct mutex mutex;
};

#ifdef CONFIG_GR

int gr_context_init(struct kbase_context *kctx);
void gr_context_term(struct kbase_context *kctx);

/* called by kbase_do_syncset
 * store synced AS into the list */
int gr_as_add_sync(struct gr_context *gr_ctx, struct basep_syncset const *sset, size_t offet);

/* called by kbase_cpu_vm_fault.
 * mark region is valid after page fault. */
int gr_as_add_valid(struct gr_context *gr_ctx, struct vm_area_struct *vma, size_t nents);

/* called by kbase_context_mmap.
 * store used AS into the list */
int gr_as_add(struct gr_context *gr_ctx, struct vm_area_struct *vma, struct kbase_va_region *region);
int gr_print_as(struct gr_context *gr_cnt, int is_forced);

void gr_mmu_dump(struct gr_context *gr_ctx, struct kbase_context *kctx);

/* Wrapper to invoke kbasep_mmu_dump_level() which dumps current GPU page table */
size_t gr_mmu_dump_level(struct kbase_context *kctx, phys_addr_t pgd,
		int level, char ** const buffer, size_t *size_left);

/* Dump physical pages
 * As memory is not flushed yet due to both cpu/gpu cache,
 * we should map physical pages to kernel space first using kbase_vmap which flush the cache
 * and synchronize the memory contents
 */
int gr_dump_phys_pages(struct kbase_context *kctx, struct file *file, u64 *offset, struct gr_as *tas, int is_mapped); 

void gr_dump_mc_valid(struct gr_context *tctx, enum gr_as_type as_type);
void gr_dump_mc_invalid(struct gr_context *tctx, enum gr_as_type as_type);

void gr_dump_sync_as(struct gr_context *tctx);

void gr_print_reg_flags(unsigned int flags);

struct file *file_open(const char *path, int flags, int rights);

#ifdef CONFIG_GR_REG_SNAPSHOT
void gr_reg_dump(struct kbase_device *kbdev, u32 is_read, unsigned long flags);
void gr_reg_flush(struct kbase_device *kbdev);
#endif

#ifdef CONFIG_GR_IO_HISTORY
int gr_io_history_init(struct kbase_io_history *h, u16 new_size);
void gr_io_history_add(struct kbase_io_history *h, u32 offset, u32 value, u8 write);
void gr_io_history_dump(struct kbase_device *kbdev);
#endif

#endif	// CONFIG_GR

#endif // _GR_DEFS_
