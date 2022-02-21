#include <stdio.h>
#include <stdlib.h>
#include <stdbool.h>
#include <fcntl.h>
#include <string.h>

#include <sys/mman.h>
#include <unistd.h>

#include "gr_manager.h"
#include "gr_mem.h"
#include "gr_validation.h"
#include "gr_utils.h"
#include "measure.h"
#include "log.h"

char *reg_base;

__attribute__((unused))
static void gr_write64(uint64_t *base, uint32_t offset, uint64_t val) {
	void *virt_addr = base + offset;

	*(volatile uint64_t *)virt_addr = val;
}

static uint64_t gr_read64(uint64_t *base, uint32_t offset) {
	void *virt_addr = base + offset;
	
	return *(volatile uint64_t*)virt_addr;
}

void gr_reg_write(uint32_t offset, uint32_t value) {
	void *virt_addr = reg_base + offset;
	*(volatile uint32_t *)virt_addr = value;
	DD("[reg_write] reg_offset: 0x%08x value: 0x%08x", offset, value);
}

uint32_t gr_reg_read(uint32_t offset) {
	void *virt_addr = reg_base + offset;
	uint32_t val;

	val = *(volatile uint32_t*)virt_addr;
	DD("[reg_read] reg_offset: 0x%08x value: 0x%08x", offset, val);
	return val;
}

char *gr_map_page(const int mem_fd, phys_addr_t paddr) {
	char *vaddr;

	xzl_bug_on_msg(paddr % PAGE_SIZE != 0, " paddr is not aligned");

	vaddr = (char *) mmap(NULL,
			PAGE_SIZE,
			PROT_READ | PROT_WRITE,
			MAP_SHARED,
			mem_fd,
			paddr);	// should be aligned

	if (vaddr == MAP_FAILED) {
		perror("[gr_map_page] cannot map the memory to the user space");
		exit(-1);
	}
	return vaddr;
}

void gr_unmap_page(void *vaddr) {
	munmap(vaddr, PAGE_SIZE);
}


static phys_addr_t pte_to_phys_addr(uint64_t entry) {
   /**  if (!(entry & 1)) */
		/** return 0; */
	entry = entry & ENTRY_NX_BIT ? entry & ~ENTRY_NX_BIT : entry;	// jin: in case entry has NX_BIT 
	return entry & ~0xFFF;
}

/** Find and return pgd corresponding to given gpu addr */
struct user_pgtable *gr_get_user_pgd_at_level(struct replay_context *rctx, gpu_addr_t gaddr, int level) {
	struct user_pgtable *current_pgt = rctx->user_pgt;
	struct user_pgtable *target_pgt = rctx->user_pgt;
	uint64_t pgd_offset;
	uint64_t vpfn;
	int l;

	vpfn = PFN_DOWN(gaddr);
	for (l = MIDGARD_MMU_TOPLEVEL; l < level; l++) {
		pgd_offset = (vpfn >> (3 - l) * 9) & (KBASE_MMU_PAGE_ENTRIES - 1);
		target_pgt = current_pgt->pgd[pgd_offset];
		/** DD("vpfn: 0x%lx level: %d offset = %d target_pgt = %p", vpfn, l, pgd_offset, target_pgt); */
		xzl_bug_on_msg(!target_pgt, "cannot get target pgd");
		current_pgt = target_pgt;
	}
	return current_pgt;
}

/** Return the target pgd that contains the given gaddr */
static phys_addr_t gr_get_pgd_at_level(const int mem_fd, gpu_addr_t gaddr, int level, phys_addr_t pgd) {
	int l;
	uint64_t *pgd_page;
	phys_addr_t target_pgd;
	uint64_t pte_offset;
	uint64_t vpfn;

	/** start from top level (0) to bottom level (3)
	  * find a proper pgd first */
	vpfn = PFN_DOWN(gaddr);
	for (l = MIDGARD_MMU_TOPLEVEL; l < level; l++) {
		target_pgd = 0;
		pte_offset = (vpfn >> (3 - l) * 9) & 0x1FF;

		/** EE("pgd: 0x%lx, gaddr: 0x%lx, vpfn: 0x%lx, pte_offset: %lu", pgd, gaddr, vpfn, pte_offset); */
		xzl_bug_on_msg(pgd % PAGE_SIZE != 0, "pgd paddr is not aligned");
		pgd_page = (uint64_t *) gr_map_page(mem_fd, pgd);
		target_pgd = pte_to_phys_addr(gr_read64(pgd_page, pte_offset));

		if (!target_pgd) {
			EE("cannot get target pgd... gaddr: 0x%lx target_pgd = 0x%lx", gaddr, target_pgd);
			EE("[cur level %d] pgd: 0x%lx, pte_offset: %lu, pte: 0x%lx, target_pgd : 0x%lx", \
				l, pgd, pte_offset, pgd_page[pte_offset], target_pgd);
			for (int i = 0; i < KBASE_MMU_PAGE_ENTRIES; i++) {
				/** EE("[0x%lx][%d] 0x%lx", pgd + (i * 8), i, pgd_page[i]); */
				EE("[0x%lx][%d] 0x%lx", pgd + (i * 8), i, gr_read64(pgd_page, i));
			}
			exit(1);
		} else {
			DD("target pgd : 0x%lx", target_pgd);
			pgd = target_pgd;
		}
		gr_unmap_page(pgd_page);
	}

	return pgd;
}

void gr_phys_memset(const int mem_fd, phys_addr_t paddr, int value, size_t size) {
	off_t offset = paddr & OFFSET_MASK;
	char *page;

	xzl_bug_on_msg(offset + size > PAGE_SIZE, "memset bug: shouldn't exceed the end of PAGE");

	page = (char *) gr_map_page(mem_fd, paddr & ~OFFSET_MASK);
	memset(page + offset, value, size); 
	gr_unmap_page(page);
}

static off_t gr_find_region_enclosing_address(struct replay_context *rctx, gpu_addr_t gaddr) {
	int off;

	for (off = 0; off < rctx->nr_regions; off++) {
		if (gaddr >= rctx->gm_region[off].gm_start && gaddr < rctx->gm_region[off].gm_end) {
			return off;
		}
	}
	return -1;
}

void gr_read_phys(const int mem_fd, phys_addr_t paddr, char *buf, size_t size) {
	off_t offset = paddr & OFFSET_MASK;
	phys_addr_t page_start_paddr = paddr & ~OFFSET_MASK;

	DD("page_start_paddr: %lx offset: %ld size : %ld", page_start_paddr, offset, size);

	char *mem_page = gr_map_page(mem_fd, page_start_paddr);

	memcpy(buf, mem_page + offset, size);
	gr_unmap_page(mem_page);
}

void gr_write_phys(const int mem_fd, phys_addr_t paddr, char *buf, size_t size) {
	off_t offset = paddr & OFFSET_MASK;
	phys_addr_t page_start_paddr = paddr & ~OFFSET_MASK;

	DD("page_start_paddr: %lx offset: %ld size : %ld", page_start_paddr, offset, size);

	char *mem_page = gr_map_page(mem_fd, page_start_paddr);

	memcpy(mem_page + offset, buf, size);
	gr_unmap_page(mem_page);
}

void gr_gpu_mem_io(struct replay_context *rctx, gpu_addr_t gaddr, char *buf, size_t size, uint8_t is_read) {
	struct gpu_mem_region *gm_region;
	off_t offset, region_offset, page_offset;
	phys_addr_t paddr;
	int i;

	uint32_t page_count;
	
	void (*gpu_mem_io)(const int, phys_addr_t, char *, size_t);

	if (is_read)
		gpu_mem_io = gr_read_phys;
	else
		gpu_mem_io = gr_write_phys;

	region_offset = gr_find_region_enclosing_address(rctx, gaddr);
	gm_region = &rctx->gm_region[region_offset];

	VV("gaddr: %lx region_offset: %ld gm_start: %lx gm_end: %lx, size: %ld", \
		gaddr, region_offset, gm_region->gm_start, gm_region->gm_end, size);

	xzl_bug_on_msg(gm_region->gm_end < gaddr + size, "cannot read/write exceeding the end of vm");

	offset = gaddr & OFFSET_MASK;
	page_offset = (gaddr - gm_region->gm_start) >> PAGE_SHIFT; 
	page_count = (offset + size + (PAGE_SIZE - 1)) >> PAGE_SHIFT;

	paddr = gm_region->pages[page_offset] + offset;

	VV("gaddr: %lx offset: %x page_offset: %ld  page_cont: %u region_offset: %ld vpfn: %lx pddr: %lx", \
		gaddr, offset, page_offset, page_count, region_offset, PFN_DOWN(gaddr), paddr);

	// first page
	size_t sz = min(((off_t) PAGE_SIZE - offset), size);
	gpu_mem_io(rctx->phys_mem_handle, paddr, buf, sz);
	buf += sz;
	size -= sz;
	DD("[First page 0] paddr: 0x%lx, written: %ld, remained: %ld", paddr, sz, size);

	// middle pages
	for (i = 1; page_count > 2 && i < page_count - 1; i++) {
		paddr = gm_region->pages[page_offset + i];
		gpu_mem_io(rctx->phys_mem_handle, paddr, buf, PAGE_SIZE);
		buf += PAGE_SIZE;
		size -= (size_t) PAGE_SIZE;
		DD("[Middle pages %d] paddr: 0x%lx, written: %ld, remained: %ld", page_offset + i, paddr, PAGE_SIZE, size);
	}

	// last page
	if (page_count > 2 && size) {
		xzl_bug_on_msg(size > PAGE_SIZE, "[last page] read/write larger than a single page");
		paddr = gm_region->pages[page_offset + page_count- 1];
		gpu_mem_io(rctx->phys_mem_handle, paddr, buf, size);
		sz = size;
		DD("[Last pages %d] paddr: 0x%lx, written: %ld, remained: %ld", page_offset + page_count - 1, paddr, PAGE_SIZE, size);
	}
}

/** Copy data from userspace to GPU memory */
void gr_copy_from_gpu(struct replay_context *rctx, gpu_addr_t gaddr, char *buf, size_t size) {
	gr_gpu_mem_io(rctx, gaddr, buf, size, 1);
}

/** Copy data from GPU memory to userspace */
void gr_copy_to_gpu(struct replay_context *rctx, gpu_addr_t gaddr, char *buf, size_t size) {
	gr_gpu_mem_io(rctx, gaddr, buf, size, 0);
}

/** This function first find phys page for the pgt corresponding to the GPU memory region.
  * Then, allocates new pages for the region and copy into them the original contents dumped from the driver.
  * It also handles PTE falgs as dumped pgt may have incorrect flags.
  * (e.g. GPU driver and MMU continuously update pgt during GPU execution.
  * the recorded PTE might be invalidated depending on recording time.)
  * Later, the input data can be overwritten.
  * The initial input data could be the one that recorder had used.
  * XXX both input and output can be in the same memory region */
void gr_mem_contents_init(struct replay_context *rctx, const char *path, phys_addr_t pgd) {
	FILE *mc_fp;
	uint64_t vaddr;
	phys_addr_t paddr;
	char buf[PAGE_SIZE];

	virt_addr_t gm_start, gm_end;
	size_t nents;
	uint32_t flags;
	uint8_t is_valid;
	size_t page_cnt = 0;

	int as_cnt = 0;

	int mem_fd = rctx->phys_mem_handle;
	struct gpu_mem_region *gm_region;

	mc_fp = fopen(path, "r");
	xzl_bug_on_msg(!mc_fp, "mem contents file read fail");

	/** format: GPU vm region metadata + mem content (unit: page) */
	while (!feof(mc_fp)) {
		fread(&gm_start, sizeof(uint64_t), 1, mc_fp);
		fread(&gm_end, sizeof(uint64_t), 1, mc_fp);
		fread(&nents, sizeof(size_t), 1, mc_fp);
		fread(&flags, sizeof(uint32_t), 1, mc_fp);
		fread(&is_valid, sizeof(int8_t), 1, mc_fp);

		gm_region = &rctx->gm_region[rctx->nr_regions++];
		gm_region->gm_start = gm_start;
		gm_region->gm_end = gm_end;
		gm_region->nr_pages = nents;
		gm_region->flags = flags;
		gm_region->is_valid = is_valid;

		// region size is different from the size of allocated memory
		II("[AS = %d] gm_start: %lx gm_end: %lx nents: %lu is_valid: %u", as_cnt, gm_start, gm_end, nents, is_valid);
		phys_addr_t *pages = (phys_addr_t *) malloc (sizeof(phys_addr_t) * nents);
		rctx->tot_nr_pages += nents;

		// allocate physical memory for the correponding address space
		gr_manager_alloc_pages(nents);
		gr_manager_get_pages(pages, nents);

		for (vaddr = gm_start, page_cnt = 0; vaddr < gm_end && page_cnt < nents; vaddr += PAGE_SIZE, page_cnt++) {
			uint32_t vindex = PFN_DOWN(vaddr) /* VPFN */  & 0x1FF;

			// Use user pagetable to find the target pgd
			// We do not keep the last level entry in the user pgd
			struct user_pgtable *target_pgt = gr_get_user_pgd_at_level(rctx, vaddr, MIDGARD_MMU_BOTTOMLEVEL);
			virt_addr_t *pgd_page = target_pgt->mapped_page;
#if CONFIG_KAGE_GLOBAL_DEBUG_LEVEL <= 40
			uint64_t orig_pte = pgd_page[vindex];
			phys_addr_t target_pgd = target_pgt->phys_page;
#endif
			phys_addr_t new_page = pages[page_cnt];
			uint64_t new_pte = 0;

			if (is_valid) {
				fread(&vaddr, sizeof(uint64_t), 1, mc_fp);
				fread(&paddr, sizeof(phys_addr_t), 1, mc_fp);
				DD("[%s] vaddr: 0x%lx paddr: 0x%lx", __func__, vaddr, paddr);

				// read page contents and write them to a new page
				fread(&buf, PAGE_SIZE, 1, mc_fp);

				/** XXX if you want to keep last level page mapped in the userspace,
				  * put the mapped page into the user_pgt struct */

				// copy content to the corresponding page
				gr_write_phys(mem_fd, new_page, (char *) buf, PAGE_SIZE);
			} else {
				/** NOTHING we should handle */
				/** may need to zero out the page? */

				/** memset(&buf, 0x0, PAGE_SIZE); */
				/** gr_write_phys(mem_fd, new_page, (char *) buf, PAGE_SIZE); */
			}

			// XXX mmu flags must be updated properly depending on recording paging policy
			// e.g. compute job is 0x443 + NX bit
			// For corss RnR (e.g. record from odroid C4 and replay from Hikey)
			// keep in mind that C4 uses LPAE and format of PTE optional status is different.
			new_pte = gr_entry_get_ate(new_page, gm_region->flags, MIDGARD_MMU_BOTTOMLEVEL);
			pgd_page[vindex] = new_pte;

			if (page_cnt == 0) {
				WW("target_pgd: %lx new_page: %lx, vindex: %u, orig_pte: %lx, new_pte: %lx", target_pgd, new_page, vindex, orig_pte & 0xFFF, new_pte & 0xFFF);
			}
		}
		gm_region->pages = pages;
		as_cnt++;
	}
	WW("end of mem_contents_init");
}

static int pte_is_valid(uint64_t pte, int const level)
{
	if (level == MIDGARD_MMU_BOTTOMLEVEL)
		return false;
	return ((pte & ENTRY_TYPE_MASK) == ENTRY_IS_PTE);
}

/** Allocate a new physical page and update pgt
  * No more action is needed for pgd page. Just update entry with new ppa | flag
  * The lat level page table entry will be updated when copying dumped memory contents.
  * XXX pfn can be used for debugging which is calculated by each pgd's offset (kinda reversing) */
phys_addr_t gr_construct_pgt(const int mem_fd, uint64_t *pgt_value, uint32_t level, uint32_t *offset, uint64_t pfn, struct user_pgtable **user_pgd) {
	phys_addr_t new_pgd, next_pgd;
	uint64_t pte_buffer[PAGE_SIZE];
	uint64_t *old_pte_value;
	uint32_t i;

	struct user_pgtable *user_new_pgd;
	struct user_pgtable *user_next_pgd;

#if CONFIG_KAGE_GLOBAL_DEBUG_LEVEL <= 10
	phys_addr_t old_pgd = pgt_value[*offset];
#endif

	new_pgd = gr_manager_alloc_get_single_page();		// alloc a new physical page
	user_new_pgd = (struct user_pgtable *) malloc (sizeof(struct user_pgtable));

	/** keep mapping for the future access */
	user_new_pgd->phys_page = new_pgd;
	user_new_pgd->mapped_page = (virt_addr_t *) gr_map_page(mem_fd, new_pgd);


	old_pte_value = &pgt_value[*offset + 1];

	/** copy original entries first */
	memcpy(pte_buffer, old_pte_value, sizeof(uint64_t) * KBASE_MMU_PAGE_ENTRIES);	

	if (level < MIDGARD_MMU_BOTTOMLEVEL) {
		for (i = 0; i < KBASE_MMU_PAGE_ENTRIES; i++) {
			if (pte_is_valid(old_pte_value[i], level)) {
				DD("[pgt level:%u offset:%u] pte: 0x%lx", level, i, old_pte_value[i]);
				*offset += KBASE_MMU_PAGE_ENTRIES + 1;
			
				next_pgd = gr_construct_pgt(mem_fd, pgt_value, level + 1, offset, pfn << 9 | i, &user_next_pgd);

				// Keep all the pgd in the user page table to speed up lookup
				user_new_pgd->pgd[i] = user_next_pgd;

				pte_buffer[i] = (old_pte_value[i] & FLAG_MASK) | next_pgd;
				DD("[pgt level : %u old pgd(0x%lx)] valid entry at offset %u(0x%lx) --> newly allocated page(0x%lx) --> updated_pte(0x%lx)",
					level, old_pgd, i, old_pte_value[i], next_pgd, pte_buffer[i]);
			} else if ((old_pte_value[i] & ENTRY_TYPE_MASK) == ENTRY_IS_INVAL) {
				DD("[pgt level : %u old pgd(0x%lx)] invalid entry at offset %u(0x%lx)", \
					level, old_pgd, i, old_pte_value[i]);
			}
		}
	}

	DD("[level %u] old pgd: 0x%lx, new pgd: 0x%lx", level, old_pgd, new_pgd);
	gr_write_phys(mem_fd, new_pgd, (char *) pte_buffer, PAGE_SIZE);

	*user_pgd = user_new_pgd;
	return new_pgd;
}

void gr_pgt_init(struct replay_context *rctx, const char *path, phys_addr_t *old_pgd, phys_addr_t *new_pgd) {
	FILE *pgt_fp;
	size_t size;
	uint64_t transtab, memattr, transcfg;
	uint64_t *pgt_value;

	uint32_t cnt = 0;
	uint32_t remained;
	uint32_t offset = 0;

	pgt_fp = fopen(path, "rb");
	if (!pgt_fp)
		perror("pgt file read fail");
	
	fread(&size, sizeof(size_t), 1, pgt_fp);
	fread(&transtab, sizeof(uint64_t), 1, pgt_fp);
	fread(&memattr, sizeof(uint64_t), 1, pgt_fp);
	fread(&transcfg, sizeof(uint64_t), 1, pgt_fp);

	*old_pgd = transtab;

	DD("size: %lu", size);
	DD("transtab: 0x%lx", transtab);
	DD("memattr: 0x%lx", memattr);
	DD("transcfg: 0x%lx", transcfg);

	remained = (size - sizeof(size_t) - (sizeof(uint64_t) * 3)) / sizeof(uint64_t);
	VV("remained : %u", remained);

	pgt_value = (uint64_t *) malloc (sizeof(uint64_t) * remained);

	/** Read dumped page tabe */
	while (!feof(pgt_fp)) {
		fread(&pgt_value[cnt], sizeof(uint64_t), 1, pgt_fp);

		if (pgt_value[cnt] != 0x2)
			DD("[%d] 0x%lx -->  0x%lx\n", cnt, pgt_value[cnt], pgt_value[cnt] & FLAG_MASK);

		if (pgt_value[cnt] == 0xFFFFFFFFFFULL)	// meet end marker
			break;
		cnt++;
	}

	VV("count : %u, first pgt value: 0x%016lx", cnt, pgt_value[0]);
	*new_pgd = gr_construct_pgt(rctx->phys_mem_handle, pgt_value, \
		MIDGARD_MMU_TOPLEVEL, &offset, 0, &rctx->user_pgt); 

	WW("orig pgd (0x%lx) == > new pgd (0x%lx)", transtab, *new_pgd);

	fclose(pgt_fp);
	free(pgt_value);
}

phys_addr_t gr_trans_va_to_pa(const int phys_mem_handle, phys_addr_t pgd, virt_addr_t vaddr) {
	phys_addr_t target_pgd, paddr;
	uint64_t *pgd_page;
	uint64_t vpfn;
	uint64_t pte;
	uint32_t offset = vaddr & OFFSET_MASK;

	vpfn = PFN_DOWN(vaddr);
	target_pgd = gr_get_pgd_at_level(phys_mem_handle, vaddr, MIDGARD_MMU_BOTTOMLEVEL, pgd);
	pgd_page = (uint64_t *) gr_map_page(phys_mem_handle, target_pgd);
	pte = pgd_page[vpfn & 0x1FF];

	gr_unmap_page(pgd_page);

	paddr = pte_to_phys_addr(pte) + offset;
	WW("vaddr: %lx, pte: %lx, paddr: %lx", vaddr, pte, paddr);

	return paddr;
}

/** Return synced address space according to the given buffer contents */
/** Given buffer size should be equal to or larger than PAGE_SIZE */
struct sync_as *gr_find_sync_as(struct replay_context *rctx, const char *buf, size_t size) {
	struct sync_as **sas = rctx->sas;
	char sas_buf[PAGE_SIZE];

	for (uint32_t offset = 0; offset < rctx->sas_cnt; offset++) {
		gr_read_phys(rctx->phys_mem_handle, sas[offset]->pm_start, sas_buf, PAGE_SIZE);
		if (!memcmp(buf, sas_buf, size))
			return sas[offset];
	}

	EE("cannot find AS for the given input");
	return NULL;
}

/** NOTE: should be called after building pgt */
void gr_get_sync_as(struct replay_context *rctx, const char *path) {
	FILE *sas_fp;
	uint32_t cnt = 0;

	uint64_t gm_start, gm_end;
	size_t size;

	struct sync_as **sas;
	off_t region_offset;

	sas_fp = fopen(path, "r");
	if (!sas_fp)
		perror("sync as file read fail");

	fread(&cnt, sizeof(uint32_t), 1, sas_fp);
	WW("total sync_as cnt : %u", cnt);

	sas = (struct sync_as **) malloc (sizeof(struct sync_as *) * cnt);

	for (uint32_t offset = 0; offset < cnt; offset++) {
		fread(&gm_start, sizeof(uint64_t), 1, sas_fp);
		fread(&gm_end, sizeof(uint64_t), 1, sas_fp);
		fread(&size, sizeof(size_t), 1, sas_fp);

		sas[offset] = (struct sync_as *) malloc (sizeof(struct sync_as));

		sas[offset]->gm_start 	= gm_start;
		sas[offset]->gm_end 	= gm_end;
		sas[offset]->size 		= size;

		// XXX: phys pages are not contiguous so currently we only have start phys_addr
		sas[offset]->pm_start 	= gr_trans_va_to_pa(rctx->phys_mem_handle, rctx->new_transtab, gm_start);
		sas[offset]->pm_end 	= 0;
		/** sas[offset]->pm_end 	= gr_trans_va_to_pa(rctx, gm_end); */
		sas[offset]->is_input 	= offset == cnt - 1? 1: 0; 

		region_offset = gr_find_region_enclosing_address(rctx, gm_start);
		struct gpu_mem_region *gm_region = &rctx->gm_region[region_offset];
		WW("gm_region = %ld", region_offset);
		gm_region->is_synced = 1;

		II("gm_start: %lx, gm_end: %lx, pm_start: %lx, size: %lu", \
			gm_start, gm_end, sas[offset]->pm_start, size);
	}
	rctx->sas = sas;
	rctx->sas_cnt = cnt;
}

// Ref to busybox devmem
// https://github.com/mirror/busybox/blob/1_27_2/miscutils/devmem.c#L85
// Good description
// https://stackoverflow.com/questions/12040303/how-to-access-physical-addresses-from-user-space-in-linux
void gr_mem_init(struct replay_context *rctx) {
	phys_addr_t old_pgd, new_pgd;
	const int mem_fd = rctx->phys_mem_handle;
	gr_config *config = &rctx->config;
	
	WW("--------------------------- gr_mem_init start ---------------------------");

	/** Instead of ioremap() */
    gr_reg_base_init(mem_fd);
	
	/*******************************************************/
	/** Constrcut a new page table for user space replayer */
	/*******************************************************/
	WW("========================= Reconstruct page table ======================== ");
	gr_pgt_init(rctx, config->pgt_path, &old_pgd, &new_pgd);

	/*************************************************************************************/
	/** Copy the original memory contents to the space newly allocated in the page table */
	/*************************************************************************************/
	WW("========================== Memory contents dump ========================= ");
	gr_mem_contents_init(rctx, config->mc_path, new_pgd);
	rctx->old_transtab = old_pgd;
	rctx->new_transtab = new_pgd;

	/**********************************************************/
	/** Get the synced GPU address space during GPU execution */
	/**********************************************************/
	WW("========================= Get synced GPU address ======================== ");
	gr_get_sync_as(rctx, config->sas_path);

	/** provisioning space for checkpoint */
	if (config->test_checkpoint) {
		WW("provisioning - tot_nr_pages: %u", rctx->tot_nr_pages);
		rctx->provision = (char *) malloc (PAGE_SIZE * rctx->tot_nr_pages);
		xzl_bug_on_msg(!rctx->provision, "No mem allocated for checkpoint provisioning space");
	}

	/** load reg snapshots for validation */
	if (config->is_validate)
		gr_validation_init(rctx, config->snapshot_path);

	WW("----------------------- gr_mem_init done ----------------------");
}

void gr_mem_term(struct replay_context *rctx) {
	gr_config *config = &rctx->config;
	int region;

	gr_reg_base_term();

	for (region = 0; region < rctx->nr_regions; region++) {
		struct gpu_mem_region *gm_region = &rctx->gm_region[region];
		if (gm_region->nr_pages > 0) {
			free(gm_region->pages);
		}
	}

	// free sas regions
	if (rctx->sas_cnt > 0) {
		for (int offset = 0; offset < rctx->sas_cnt; offset++)
			free(rctx->sas[offset]);
		free(rctx->sas);
	}


	if (config->test_checkpoint && rctx->provision) {
		WW("free provisioning space for checkpoint");
		free(rctx->provision);
	}

	if (config->is_validate) {
		gr_validation_term(rctx);
	}
}

/** jin: copied from kdriver (map KBASE_REG flags to MMU flags) */
static uint64_t get_mmu_flags(unsigned long flags) {
	uint64_t mmu_flags;

	/* store mem_attr index as 4:2 (macro called ensures 3 bits already) */
	mmu_flags = KBASE_REG_MEMATTR_VALUE(flags) << 2;

	/* Set access flags - note that AArch64 stage 1 does not support
	 * write-only access, so we use read/write instead
	 */
	if (flags & KBASE_REG_GPU_WR)
		mmu_flags |= ENTRY_ACCESS_RW;
	else if (flags & KBASE_REG_GPU_RD)
		mmu_flags |= ENTRY_ACCESS_RO;

	/* nx if requested */
	mmu_flags |= (flags & KBASE_REG_GPU_NX) ? ENTRY_NX_BIT : 0;

	if (flags & KBASE_REG_SHARE_BOTH) {
		/* inner and outer shareable */
		mmu_flags |= SHARE_BOTH_BITS;
	} else if (flags & KBASE_REG_SHARE_IN) {
		/* inner shareable coherency */
		mmu_flags |= SHARE_INNER_BITS;
	}

	return mmu_flags;
}

uint64_t gr_entry_get_ate(phys_addr_t paddr, unsigned long flags, int const level) {
	if (level == MIDGARD_MMU_BOTTOMLEVEL)
		return (paddr | get_mmu_flags(flags) | ENTRY_ACCESS_BIT | ENTRY_IS_ATE_L3);
	else
		return (paddr | get_mmu_flags(flags) | ENTRY_ACCESS_BIT | ENTRY_IS_ATE_L02);
}

uint64_t gr_entry_get_pte(phys_addr_t paddr) {
	return (paddr | ENTRY_ACCESS_BIT | ENTRY_IS_PTE);
}

void gr_reg_base_init(const int mem_fd) {
	reg_base = (char *) mmap(NULL,
			GPU_REG_PA_SIZE,	// 16K
			PROT_READ | PROT_WRITE,
			MAP_SHARED,
			mem_fd,
			GPU_REG_PA_START);	// 0xe82c0000
	if (reg_base == MAP_FAILED) {
		perror("[gr_reg_base_init] cannot map the memory to the user space");
		exit(-1);
	}
	DD("mmap is done... reg_base : 0x%08x", reg_base);
}

void gr_reg_base_term(void) {
	munmap(reg_base, GPU_REG_PA_SIZE);
}
