#include <stdio.h>
#include <string.h>
#include <stdlib.h>

#include "gr_checkpoint.h"
#include "gr_mem.h"
#include "log.h"
#include "measure.h"

// XXX copy gm_region to reserved space
// start from first vm region in the list
int gr_write_checkpoint(struct replay_context *rctx) {
	gr_config *config = &rctx->config;
	uint32_t i, j;
	struct gpu_mem_region *gm_region;
	virt_addr_t *mapped_page;
	static uint32_t count = 0;

	if (count++ >= 2) return 0;

	xzl_bug_on_msg(!config->test_checkpoint, "no checkpoint is configured");
	xzl_bug_on_msg(!rctx->provision, "no mem allocated for provisioning");

	rctx->nr_written_pages = 0;

	/** k2_measure("checkpoint start"); */
	for (i = 0; i < rctx->nr_regions; i++) {
		gm_region = &rctx->gm_region[i];
		DD("gm_region[%u], nr_pages: %u written_pages: %u tot_pages: %u", \
			i, gm_region->nr_pages, rctx->nr_written_pages, rctx->tot_nr_pages);
		for (j = 0; j < gm_region->nr_pages; j++) {
			char *buf = rctx->provision + rctx->nr_written_pages * PAGE_SIZE;
			xzl_bug_on_msg(rctx->nr_written_pages >= rctx->tot_nr_pages, "no available mem for checkpoint");
			mapped_page = (virt_addr_t *) gr_map_page(rctx->phys_mem_handle, gm_region->pages[j]);
			memcpy(buf, (char *) mapped_page, PAGE_SIZE);
			rctx->nr_written_pages++;
			gr_unmap_page(mapped_page);
			DD("page[%u] written_pages: %u tot_pages: %u", \
				j, rctx->nr_written_pages, rctx->tot_nr_pages);
		}
	}
	/** k2_measure("checkpoint end"); */
	return 1;
}
