#ifndef _GR_DEF_H
#define _GR_DEF_H

#include <stdint.h>
#include "midgard/mali_kbase_gpu_regmap.h"

typedef uint64_t virt_addr_t;
typedef uint64_t phys_addr_t;
typedef uint64_t gpu_addr_t;

#define min(a, b) (((a) < (b)) ? (a) : (b))
#define max(a, b) (((a) > (b)) ? (a) : (b))

#define NR_VA_REGION 128

struct reg_list {
	int cnt;
	struct reg_io *pos;
	struct reg_io *head;
	struct reg_io *tail;
};

struct reg_io {
	uint64_t ts;
	char is_read;
	uint32_t offset;
	uint32_t value;
	struct reg_io *link;
};

// Keep gpu region structure to ease management
struct gpu_mem_region {
	gpu_addr_t gm_start;
	gpu_addr_t gm_end;

	phys_addr_t paddr_start;
	phys_addr_t paddr_end;
	phys_addr_t *pages;
	// char *provision;	// for checkpoint

	size_t nr_pages;
	unsigned long flags;
	uint8_t is_valid;

	uint8_t is_synced;
};

struct user_pgtable {
	// Mapped address of current pgd
	// We can directly access phys memory once mapping it.
	// No need to remap it as long as we keep user pgt
	virt_addr_t *mapped_page;
	phys_addr_t phys_page;

	// keep child pgd virt addresses if valid
	// mapped_addr include not child pgt address but phys addr
	struct user_pgtable *pgd[512];
};

typedef struct _gr_config {
	char *io_path;
	char *pgt_path;
	char *mc_path;
	char *sas_path;
	uint32_t nr_prep_jobs;	// # of compute jobs for prep
	uint32_t nr_comp_jobs;	// # of compute jobs for computation
	uint32_t nr_tot_jobs;

	int8_t test_checkpoint;	// toggle when applying checkpoint
	
	int8_t is_validate;		// toggle when applying validation
	char *snapshot_path;
} gr_config;

struct replay_context {
	gr_config config;
	struct reg_list *list;			// io history record

	struct sync_as **sas;
	uint32_t sas_cnt;

	int phys_mem_handle;

	phys_addr_t old_transtab;	// orignal pgt addr
	phys_addr_t new_transtab;	// newly allocated pgd for replaying

	uint64_t memattr;
	uint64_t transcfg;

	// TODO maange gpu_region with structure
	struct gpu_mem_region gm_region[NR_VA_REGION];
	size_t nr_regions;

	uint32_t tot_nr_pages;
	char *provision;
	uint32_t nr_written_pages;

	uint32_t ts_prep_start;
	uint32_t ts_prep_end;
	uint32_t ts_comp_start;
	uint32_t ts_comp_end;

	uint32_t tot_prep_time;
	uint32_t tot_comp_time;

	// user space page table to look up
	// keep the mapping between physical to user address space for better look up speed
	struct user_pgtable *user_pgt;

	uint32_t *regmap;
	FILE *reg_fp;

	uint32_t is_first_run;
};

#endif
