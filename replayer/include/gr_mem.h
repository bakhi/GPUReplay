#ifndef _GR_MEM_H_
#define _GR_MEM_H_

#include "gr_defs.h"

#define GPU_REG_PA_START	0xe82c0000
#define GPU_REG_PA_SIZE		0x4000
#define PAGE_SHIFT			12
#define PAGE_SIZE			(1UL << PAGE_SHIFT)
#define PAGE_MASK			(~(PAGE_SIZE - 1))
#define FLAG_MASK			(PAGE_SIZE - 1)
#define OFFSET_MASK			(PAGE_SIZE - 1)

#define PFN_DOWN(x)			((x) >> PAGE_SHIFT)
#define PFN_PHYS(x)			((x) << PAGE_SHIFT)

/* mmu */
#define MIDGARD_MMU_LEVEL(x) 	x
#define MIDGARD_MMU_TOPLEVEL	MIDGARD_MMU_LEVEL(0)
#define MIDGARD_MMU_BOTTOMLEVEL	MIDGARD_MMU_LEVEL(3)

#define KBASE_MMU_PAGE_ENTRIES 512


#define ENTRY_TYPE_MASK		3ULL

#define ENTRY_IS_ATE_L3		3ULL
#define ENTRY_IS_ATE_L02	1ULL
#define ENTRY_IS_INVAL		2ULL
#define ENTRY_IS_PTE		3ULL

#define ENTRY_ATTR_BITS 	(7ULL << 2) 	/* bits 4:2 */
#define ENTRY_ACCESS_RW 	(1ULL << 6)     /* bits 6:7 */
#define ENTRY_ACCESS_RO 	(3ULL << 6)
#define ENTRY_SHARE_BITS 	(3ULL << 8)    	/* bits 9:8 */
#define ENTRY_ACCESS_BIT 	(1ULL << 10)
#define ENTRY_NX_BIT 		(1ULL << 54)

#define gr_ATE_FLAGS		ENTRY_NX_BIT | ENTRY_ACCESS_BIT | ENTRY_SHARE_BITS | ENTRY_ACCESS_RW | ENTRY_IS_ATE_L3


//////////////////////////////////////////////////////////
/* jin: memory region flags information from the kbase driver */
enum kbase_share_attr_bits {
	/* (1ULL << 8) bit is reserved */
	SHARE_BOTH_BITS = (2ULL << 8),  /* inner and outer shareable coherency */
	SHARE_INNER_BITS = (3ULL << 8)  /* inner shareable coherency */
};

/* Free region */
#define KBASE_REG_FREE              (1ul << 0)
/* CPU write access */
#define KBASE_REG_CPU_WR            (1ul << 1)
/* GPU write access */
#define KBASE_REG_GPU_WR            (1ul << 2)
/* No eXecute flag */
#define KBASE_REG_GPU_NX            (1ul << 3)
/* Is CPU cached? */
#define KBASE_REG_CPU_CACHED        (1ul << 4)
/* Is GPU cached?
 * Some components within the GPU might only be able to access memory that is
 * GPU cacheable. Refer to the specific GPU implementation for more details.
 */
#define KBASE_REG_GPU_CACHED        (1ul << 5) 

#define KBASE_REG_GROWABLE          (1ul << 6)
/* Can grow on pf? */
#define KBASE_REG_PF_GROW           (1ul << 7)

/* Allocation doesn't straddle the 4GB boundary in GPU virtual space */
#define KBASE_REG_GPU_VA_SAME_4GB_PAGE (1ul << 8)

/* inner shareable coherency */
#define KBASE_REG_SHARE_IN          (1ul << 9)
/* inner & outer shareable coherency */
#define KBASE_REG_SHARE_BOTH        (1ul << 10)

/* Space for 4 different zones */
#define KBASE_REG_ZONE_MASK         (3ul << 11)
#define KBASE_REG_ZONE(x)           (((x) & 3) << 11)

/* GPU read access */
#define KBASE_REG_GPU_RD            (1ul<<13)
/* CPU read access */
#define KBASE_REG_CPU_RD            (1ul<<14)

/* Index of chosen MEMATTR for this region (0..7) */
#define KBASE_REG_MEMATTR_MASK      (7ul << 16)
#define KBASE_REG_MEMATTR_INDEX(x)  (((x) & 7) << 16)
#define KBASE_REG_MEMATTR_VALUE(x)  (((x) & KBASE_REG_MEMATTR_MASK) >> 16)

#define KBASE_REG_PROTECTED         (1ul << 19)

#define KBASE_REG_DONT_NEED         (1ul << 20)

/* Imported buffer is padded? */
#define KBASE_REG_IMPORT_PAD        (1ul << 21)

/* Bit 22 is reserved.
 *
 * Do not remove, use the next unreserved bit for new flags */
#define KBASE_REG_RESERVED_BIT_22   (1ul << 22)

/* The top of the initial commit is aligned to extent pages.
 * Extent must be a power of 2 */
#define KBASE_REG_TILER_ALIGN_TOP   (1ul << 23)

/* Whilst this flag is set the GPU allocation is not supposed to be freed by
 * user space. The flag will remain set for the lifetime of JIT allocations.
 */

#define KBASE_REG_NO_USER_FREE      (1ul << 24)

/* Memory has permanent kernel side mapping */
#define KBASE_REG_PERMANENT_KERNEL_MAPPING (1ul << 25)

/* GPU VA region has been freed by the userspace, but still remains allocated
 * due to the reference held by CPU mappings created on the GPU VA region.
 *
 * A region with this flag set has had kbase_gpu_munmap() called on it, but can
 * still be looked-up in the region tracker as a non-free region. Hence must
 * not create or update any more GPU mappings on such regions because they will
 * not be unmapped when the region is finally destroyed.
 *                                                                                                                                                                                                         
 * Since such regions are still present in the region tracker, new allocations
 * attempted with BASE_MEM_SAME_VA might fail if their address intersects with
 * a region with this flag set.
 *
 * In addition, this flag indicates the gpu_alloc member might no longer valid
 * e.g. in infinite cache simulation.
 */
#define KBASE_REG_VA_FREED (1ul << 26)

#define KBASE_REG_ZONE_SAME_VA      KBASE_REG_ZONE(0)

//////////////////////////////////////////////////////////

struct sync_as {
	uint64_t gm_start;
	uint64_t gm_end;
	uint64_t size;

	uint8_t is_input;
	
	phys_addr_t pm_start;
	phys_addr_t pm_end;
};

void gr_mem_init(struct replay_context *rctx);
void gr_mem_term(struct replay_context *rctx);

/* map physical page an return mapped virtual address */
char *gr_map_page(const int mem_fd, phys_addr_t paddr);
/* unmap page */
void gr_unmap_page(void *vaddr);

void gr_reg_base_init(const int mem_fd);
void gr_reg_base_term(void);

/********** page table **********/
void gr_pgt_init(struct replay_context *rctx, const char *path, phys_addr_t *old_pgd, phys_addr_t *new_pgd);

phys_addr_t gr_construct_pgt(const int mem_fd, uint64_t *pgt_value, uint32_t level, uint32_t *offset, uint64_t pfn, struct user_pgtable **user_pgd);
phys_addr_t gr_trans_va_to_pa(const int phys_mem_handle, phys_addr_t pgd, virt_addr_t vaddr);
struct user_pgtable *gr_get_user_pgd_at_level(struct replay_context *rctx, gpu_addr_t gaddr, int level);
uint64_t gr_entry_get_ate(phys_addr_t paddr, unsigned long flags, int const level);
uint64_t gr_entry_get_pte(phys_addr_t paddr);

/********** mem dump **********/
void gr_mem_contents_init(struct replay_context *rctx, const char *path, phys_addr_t pgd);

/********** synced mem regions **********/
void gr_get_sync_as(struct replay_context *rctx, const char *path);
struct sync_as *gr_find_sync_as(struct replay_context *rctx, const char *buffer, size_t size);

/********** GPU register access **********/
void gr_reg_write(uint32_t offset, uint32_t value);
uint32_t gr_reg_read(uint32_t offset);

/********** copy to/from GPU  **********/
void gr_phys_memset(const int mem_fd, phys_addr_t paddr, int value, size_t size);
void gr_read_phys(const int mem_fd, phys_addr_t paddr, char *buf, size_t size);
void gr_write_phys(const int mem_fd, phys_addr_t paddr, char *buf, size_t size);

void gr_gpu_mem_io(struct replay_context *rctx, gpu_addr_t gaddr, char *buf, size_t size, uint8_t is_read);
void gr_copy_from_gpu(struct replay_context *rctx, gpu_addr_t gaddr, char *buf, size_t size);
void gr_copy_to_gpu(struct replay_context *rctx, gpu_addr_t gaddr, char *buf, size_t size);

#endif
