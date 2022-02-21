#include <sys/mman.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <unistd.h>

#include <stdio.h>
#include <stdlib.h>

#include "gr_utils.h"
#include "gr_mem.h"
#include "log.h"

#ifdef DEBUG

static void hex_dump_with_paddr(const void *data, const uint64_t paddr, size_t size) {
	char ascii[17];  
	size_t i, j;
	ascii[16] = '\0';
	for (i = 0; i < size; ++i) {
		if (i % 16 == 0) {
			print_to_tracebuffer("0x%lx | ", (paddr + i));
		}
		print_to_tracebuffer("%02X ", ((unsigned char*)data)[i]);
		if (((unsigned char*)data)[i] >= ' ' && ((unsigned char*)data)[i] <= '~') {
			ascii[i % 16] = ((unsigned char*)data)[i];
		} else {
			ascii[i % 16] = '.';
		}
		if ((i+1) % 8 == 0 || i+1 == size) {
			print_to_tracebuffer(" ");
			if ((i+1) % 16 == 0) {
				print_to_tracebuffer("|  %s \n", ascii);
			} else if (i+1 == size) {
				ascii[(i+1) % 16] = '\0';
				if ((i+1) % 16 <= 8) {
					print_to_tracebuffer(" ");
				}
				for (j = (i+1) % 16; j < 16; ++j) {
					print_to_tracebuffer("   ");
				}
				print_to_tracebuffer("|  %s \n", ascii);
			}
		}
	}
}

void hex_dump(const void *data, size_t size) {
	char ascii[17];
	size_t i, j;
	ascii[16] = '\0';
	for (i = 0; i < size; ++i) {
		print_to_tracebuffer("%02X ", ((unsigned char*)data)[i]);
		if (((unsigned char*)data)[i] >= ' ' && ((unsigned char*)data)[i] <= '~') {
			ascii[i % 16] = ((unsigned char*)data)[i];
		} else {
			ascii[i % 16] = '.';
		}
		if ((i+1) % 8 == 0 || i+1 == size) {
			print_to_tracebuffer(" ");
			if ((i+1) % 16 == 0) {
				print_to_tracebuffer("|  %s \n", ascii);
			} else if (i+1 == size) {
				ascii[(i+1) % 16] = '\0';
				if ((i+1) % 16 <= 8) {
					print_to_tracebuffer(" ");
				}
				for (j = (i+1) % 16; j < 16; ++j) {
					print_to_tracebuffer("   ");
				}
				print_to_tracebuffer("|  %s \n", ascii);
			}
		}
	}
}

void gr_dump_phys(const char *name, uint64_t paddr, size_t size) {
	int mem_fd;
	void *map_base, *vaddr;
	off_t offset_in_page = paddr & OFFSET_MASK;
	phys_addr_t page_start_paddr = paddr & ~OFFSET_MASK;

	if (size > PAGE_SIZE) {
		EE("size must be smaller than PAGE_SIZE");
		return;
	}

	mem_fd = open("/dev/mem", O_RDWR | O_DSYNC);
	if (mem_fd < 0) {
		perror("cnnot open /dev/mem");
		exit(1);
	}

	map_base = mmap(NULL,
				PAGE_SIZE,
				PROT_READ,
				MAP_SHARED,
				mem_fd,
				page_start_paddr);

	vaddr = (char *)map_base + offset_in_page;

	EE("=============== [%s] DUMP paddr 0x%lx ==============", name, paddr);
	hex_dump_with_paddr(vaddr, paddr, size);

	munmap(map_base, PAGE_SIZE);
	close(mem_fd);
}

void gr_tune_mc(const char* rpath, const char* wpath){
	FILE *rfp, *wfp;
	/** char buf[PAGE_SIZE]; */
	int64_t buf[PAGE_SIZE / sizeof(int64_t)];
	uint64_t gm_start, gm_end;
	uint64_t vaddr, paddr;
	uint32_t flags;
	uint8_t is_valid;
	size_t nents;

	uint32_t nr_gpu_pages = 0;	// binary pages + job page. simply count the valid pages
	int32_t as_cnt = 0;
	size_t page_cnt = 0;

	uint64_t hash = 0;

	rfp = fopen(rpath, "r");
	wfp = fopen(wpath, "w");

	xzl_bug_on_msg(!rfp, "File read error");

	while (!feof(rfp)) {
		fread(&gm_start, sizeof(uint64_t), 1, rfp); 
		fread(&gm_end, sizeof(uint64_t), 1, rfp);
		fread(&nents, sizeof(size_t), 1, rfp);
		fread(&flags, sizeof(uint32_t), 1, rfp);
		fread(&is_valid, sizeof(int8_t), 1, rfp);

		if (gm_start > 0xffffffffffff) {
			WW("[AS %d] gm_start: 0x%lx gm_end: 0x%lx nents: %ld flags: %x is_valid: %d", as_cnt, gm_start, gm_end, nents, flags, is_valid);
			continue;
		}

		if (feof(rfp)) break;

		fwrite(&gm_start, sizeof(uint64_t), 1, wfp); 
		fwrite(&gm_end, sizeof(uint64_t), 1, wfp);
		fwrite(&nents, sizeof(size_t), 1, wfp);
		fwrite(&flags, sizeof(uint32_t), 1, wfp);
		fwrite(&is_valid, sizeof(int8_t), 1, wfp);

		WW("[AS %d] gm_start: 0x%lx gm_end: 0x%lx nents: %ld flags: %x is_valid: %d", as_cnt, gm_start, gm_end, nents, flags, is_valid);
		hash = 0;
		if (is_valid) {
			nr_gpu_pages += nents;
		}
		for (vaddr = gm_start, page_cnt = 0; vaddr < gm_end && page_cnt < nents; vaddr += PAGE_SIZE, page_cnt++) {
			if (is_valid) {
				fread(&vaddr, sizeof(virt_addr_t), 1, rfp);
				fread(&paddr, sizeof(phys_addr_t), 1, rfp);

				fwrite(&vaddr, sizeof(virt_addr_t), 1, wfp);
				fwrite(&paddr, sizeof(phys_addr_t), 1, wfp);

				// sometimes garbage is in the contents
				if (vaddr > 0xffffffffffff) continue;

				fread(&buf, sizeof(char), PAGE_SIZE, rfp);

				if (as_cnt > 6 && page_cnt == 0 && nents == 1 && (flags == 0x606e /* G71 */ || flags == 0x604e /* G52 */)) {
					for (int offset = 0; offset < PAGE_SIZE / sizeof(int64_t); offset++){
						if (buf[offset] == 0x1 && buf[offset + 1] == 0x0) {
							buf[offset] = 0x0;
							EE("offset: %ld value is 0x1! %lx vaddr: %lx", offset * sizeof(int64_t), buf[offset], vaddr + sizeof(int64_t) * offset);
						#if 0
						} else if (buf[offset] == 0x20000000) {	// jin: odroid jobchain has 0x20 job task split
							EE("correct job task split from 0x20 (G52) to 0x1c (G71)");
							buf[offset] = 0x1c000000;	// jc in G71 has 0x1c job task split
						} else if (buf[offset] == 0x08002001) {
							EE("correct shader register allocation from (G52) to (G71)");
							buf[offset] = 0x08000001;
						} else if (buf[offset] == 0x029000) {
							EE("correct from (G52) to (G71)");
							buf[offset] = 0x829000;
						#endif
						} else if (buf[offset] == 0xffffa10cd000) {
							EE("find out, as_cnt: %d offset: %d vm: 0x%lx", as_cnt, offset, gm_start + sizeof(int64_t) * offset);
						}
					}
				}

				#if 0
				if (flags == 0x6062 /* G71 */ || flags == 0x6042 /* G62 */) {
					for (int offset = 0; offset < PAGE_SIZE / sizeof(int64_t); offset++){
						if (buf[offset] == 0xaf0060020c1765b9) {	// jin: GPU binary update from G52 to G71
							EE("correct GPU binary configuartion from (G52) to (G71)");
							buf[offset] = 0xa10040020c176591;
						} 
					}
				}
				#endif

				for (int k = 0 ; k < 512; k++)
					hash ^= buf[k];

				fwrite(buf, sizeof(char), PAGE_SIZE, wfp);
			}
		}
		II("hash : %lx", hash);
		as_cnt++;
	}
	II("tune is done. nr_gpu pages: %u", nr_gpu_pages);

	fclose(rfp);
	fclose(wfp);
}

#endif	// ifndef DEBUG

void gr_clean_used_atom(struct replay_context *rctx) {
	struct gpu_mem_region *gm_region;
	int64_t buf[PAGE_SIZE / sizeof(int64_t)];
	uint32_t i, is_used;

	WW("gr_clean_used_atom run");
	for (i = 0; i < rctx->nr_regions; i++) {
		gm_region = &rctx->gm_region[i];
		if (i > 6 && gm_region->nr_pages == 1 && \
				(gm_region->flags == 0x606e /* G71 */ || gm_region->flags == 0x604e /* G52 */)) {
			WW("[Check atom memory region] reg_num: %d gm_start: 0x%lx gm_end: 0x%lx", i, gm_region->gm_start, gm_region->gm_end);
			is_used = 0;
			gr_read_phys(rctx->phys_mem_handle, gm_region->pages[0], (char *) buf, PAGE_SIZE);
			for (int offset = 0; offset < PAGE_SIZE / sizeof(int64_t); offset++){
				/** if ((buf[offset] == 0x1 && buf[offset + 1] == 0x0) ||  */
				if (buf[offset] == 0x010258 && buf[offset + 1] == 0x0) {	/* pattern of used atom */
					buf[offset] = 0x0;
					EE("offset: %ld value is 0x1! %lx vaddr: %lx", offset * sizeof(int64_t), buf[offset], gm_region-> gm_start + sizeof(int64_t) * offset);
					is_used = 1;
				}
			}
			if (is_used) {
				VV("[Clean atom memory region] reg_num: %d gm_start: 0x%lx gm_end: 0x%lx", i, gm_region->gm_start, gm_region->gm_end);
				gr_write_phys(rctx->phys_mem_handle, gm_region->pages[0], (char *) buf, PAGE_SIZE);
			}
		}
	}
}


/** Reload the recorded content and compare with the newly replayed result */
void gr_mem_contents_check(struct replay_context *rctx, const char *path) {
	struct gpu_mem_region *gm_region;
	FILE *mc_fp;

	gpu_addr_t gm_start, gm_end, gaddr;
	phys_addr_t paddr;
	size_t nents;
	uint32_t flags;
	uint8_t is_valid;
	uint32_t region_idx = 0;

	uint32_t page_cnt;
	char buf[PAGE_SIZE];
	char replayed[PAGE_SIZE];

	uint8_t is_different = 0;

	mc_fp = fopen(path, "r");
	xzl_bug_on_msg(!mc_fp, "mem contents file read fail");

	while (!feof(mc_fp)) {
		fread(&gm_start, sizeof(uint64_t), 1, mc_fp);
		fread(&gm_end, sizeof(uint64_t), 1, mc_fp);
		fread(&nents, sizeof(size_t), 1, mc_fp);
		fread(&flags, sizeof(uint32_t), 1, mc_fp);
		fread(&is_valid, sizeof(int8_t), 1, mc_fp);

		gm_region = &rctx->gm_region[region_idx];

		if (is_valid) {
			is_different = 0;
			VV("region idx: %d gm_start: 0x%lx gm_end: 0x%lx", region_idx, gm_start, gm_end);
			for (gaddr = gm_region->gm_start, page_cnt = 0; gaddr < gm_region->gm_end && page_cnt < gm_region->nr_pages; gaddr += PAGE_SIZE, page_cnt++) {
				fread(&gaddr, sizeof(gpu_addr_t), 1, mc_fp);
				fread(&paddr, sizeof(phys_addr_t), 1, mc_fp);

				// read page contents and write them to a new page
				fread(&buf, PAGE_SIZE, sizeof(char), mc_fp);
				gr_read_phys(rctx->phys_mem_handle, gm_region->pages[page_cnt], replayed, PAGE_SIZE);

				if (!is_different)
					for (int i = 0; i < PAGE_SIZE; i++) {
						if (replayed[i] != buf[i]) {
							/** EE("difference between recorded and replayed [region idx: %d, nr_pages: %lu]", region_idx, gm_region->nr_pages);
							  * EE("gm_start : %lx, gm_end: %lx gaddr: %lx", gm_start, gm_end, gaddr); */

							/** hex_dump(&buf[i], 128);
							  * printf("------------------------\n");
							  * hex_dump(&replayed[i], 128);
							  * getchar(); */

							is_different = 1;
							break;
						}
					}

				// if any differences are detected, overwrite it with the recorded
				if (is_different) {
					gr_write_phys(rctx->phys_mem_handle, gm_region->pages[page_cnt], buf, PAGE_SIZE);
				}
			}
		}
		region_idx++;
	}
	EE("mem_contents_check done");
	fclose(mc_fp);
}

void gr_restore_pte(struct replay_context *rctx) {
	struct gpu_mem_region *gm_region;
	gpu_addr_t gaddr;
	uint64_t pfn;
	uint32_t i, page_cnt;

	for (i = 0; i < rctx->nr_regions; i++) {
		gm_region = &rctx->gm_region[i];

		WW("[Update pte] region: %d gm_start: 0x%lx gm_end: 0x%lx", i, gm_region->gm_start, gm_region->gm_end);
		for (gaddr = gm_region->gm_start, page_cnt = 0; gaddr < gm_region->gm_end && page_cnt < gm_region->nr_pages; gaddr += PAGE_SIZE, page_cnt++) {
			VV("[Update pte] gaddr : 0x%lx", gaddr);
			pfn = PFN_DOWN(gaddr);
			uint32_t pte_index = pfn & 0x1FF;
			struct user_pgtable *target_pgt = gr_get_user_pgd_at_level(rctx, gaddr, MIDGARD_MMU_BOTTOMLEVEL);
			virt_addr_t *pgd_page = target_pgt->mapped_page;

			uint64_t new_pte = 0;
			phys_addr_t target_page = gm_region->pages[page_cnt];

			// update pte
			new_pte = gr_entry_get_ate(target_page, gm_region->flags, MIDGARD_MMU_BOTTOMLEVEL);
			pgd_page[pte_index] = new_pte;
		}
	}
}

/** jin: if region was invalid at recording time, clean all the mem contents for subsequent replay */
void gr_clean_invalid_region(struct replay_context *rctx) { 
	struct gpu_mem_region *gm_region;
	char buf[PAGE_SIZE] = {0, };
	uint32_t i, j;
	
	for (i = 0; i < rctx->nr_regions; i++) {
		gm_region = &rctx->gm_region[i];
		if (!gm_region->is_valid) {
			// jin: zero out invalid pages when recorded
			WW("[Clean region %d] gm_start: 0x%lx, gm_end: 0x%lx", i, gm_region->gm_start, gm_region->gm_end);
			for (j = 0; j < gm_region->nr_pages; j++) {
				gr_copy_to_gpu(rctx, gm_region->gm_start + (j * PAGE_SIZE), buf, PAGE_SIZE);
			}
		}
	}
}
