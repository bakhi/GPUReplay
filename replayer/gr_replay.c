#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include <fcntl.h>
#include <sys/mman.h>
#include <time.h>

#include "gr_manager.h"
#include "gr_defs.h"
#include "gr_mem.h"
#include "gr_utils.h"
#include "log.h"

#include "gr_replay.h"
#include "gr_checkpoint.h"
#include "gr_validation.h"
#include "measure.h"

#define GR_JS 		1		// job slot is always 1
#define GR_AS_NR	0		// always use 0th address space

uint64_t atom_status_unused = 0x0;
uint32_t latest_flush_id = 0;	// may not affect the correctness

__attribute__((unused))
static int reg_read_once(uint32_t offset, uint32_t value, uint8_t ignore) {
	uint32_t new_value;

	new_value = gr_reg_read(offset);
	if (!ignore) {
		return 0;
	} else {
		return new_value ^ value;
	}
}

static void reg_read_wait(uint32_t offset, uint32_t value) {
	uint32_t max_loops = KBASE_AS_INACTIVE_MAX_LOOPS;
	uint32_t new_value;

	new_value = gr_reg_read(offset);
	
	while (--max_loops && (new_value != value))
		new_value = gr_reg_read(offset);

	xzl_bug_on_msg(max_loops == 0, "AS_ACTIVE bit stuck");
}

__attribute__((unused))
static int gr_wait_gpu_irq(int timeout) {
	return gr_manager_poll(IRQ_TYPE_GPU, timeout);
}

static int gr_wait_job_irq(int timeout) {
	return gr_manager_poll(IRQ_TYPE_JOB, timeout);
}

static void gr_cache_invalidate() {
	const uint32_t mask = CLEAN_CACHES_COMPLETED | RESET_COMPLETED;
	uint32_t loops = 100000;    // KBASE_CLEAN_CACHE_MAX_LOOPS
	uint32_t val, irq_mask;

	/* Enable interrupt */
	irq_mask = gr_reg_read(GPU_CONTROL_REG(GPU_IRQ_MASK));
	gr_reg_write(GPU_CONTROL_REG(GPU_IRQ_MASK), irq_mask | CLEAN_CACHES_COMPLETED);

	gr_reg_write(GPU_CONTROL_REG(GPU_COMMAND), GPU_COMMAND_CLEAN_INV_CACHES);
	val = gr_reg_read(GPU_CONTROL_REG(GPU_IRQ_RAWSTAT));                     

	// XXX can use GPU interrupt as well
	while (((val & mask) == 0) && --loops){
		val = gr_reg_read(GPU_CONTROL_REG(GPU_IRQ_RAWSTAT));
	}

	/* Disable interrupt */
	irq_mask = gr_reg_read(GPU_CONTROL_REG(GPU_IRQ_MASK));
	gr_reg_write(GPU_CONTROL_REG(GPU_IRQ_MASK),
			irq_mask & ~CLEAN_CACHES_COMPLETED);

	gr_reg_write(GPU_CONTROL_REG(GPU_IRQ_CLEAR), val);
}

static void gr_as_flush(phys_addr_t lock_addr) {
	uint32_t val;
	uint32_t max_loops = 100000000; // KBASE_AS_INACTIVE_MAX_LOOPS
	/* Lock the region that needs to be updated */
	gr_reg_write(MMU_AS_REG(0 /* as number */, AS_LOCKADDR_LO),
			lock_addr & 0xFFFFFFFFUL);
	gr_reg_write(MMU_AS_REG(0, AS_LOCKADDR_HI),
			(lock_addr >> 32) & 0xFFFFFFFFUL);

	/* Lock the region that needs to be updated */
	gr_reg_write(MMU_AS_REG(0, AS_COMMAND), AS_COMMAND_LOCK);
	usleep(1);

	/* Run the MMU operation */
	gr_reg_write(MMU_AS_REG(0, AS_COMMAND), AS_COMMAND_FLUSH_MEM);

	/* Wait for the flush to complete */
	val = gr_reg_read(MMU_AS_REG(0, AS_STATUS)); /* read status */

	while (--max_loops && (val & AS_STATUS_AS_ACTIVE))
		val = gr_reg_read(MMU_AS_REG(0 /*as_nr*/, AS_STATUS));
}                   

/** Originally fixed by mem_content_tune.
  * Atom status will be changed once the atom is executed even during replay
  * The problem is sometimes atom is reused of which status should be usable (0x0 intead 0x1)
  * Here we always set the bit as 0x0 */
static void gr_js_tune(struct replay_context *rctx, uint32_t type, uint32_t val) {
	static uint32_t jc_lo = 0;
	gpu_addr_t jc_addr;

	if (type == JOB_SLOT_REG(GR_JS, JS_HEAD_NEXT_LO)) {
		jc_lo = val;
	} else {
		jc_addr = val;
		jc_addr <<= 32;
		jc_addr |= jc_lo;
		/** XXX should we tune all the js in a single page? */
		gr_copy_to_gpu(rctx, jc_addr, (void *) &atom_status_unused, sizeof(uint64_t));
	}
}

void gr_replay_run(struct replay_context *rctx) {
	gr_config *config = &rctx->config;
	struct reg_io *cur_io= rctx->list->head;
	uint32_t cmd, value, new_value;
	
	uint32_t job_submit_cnt = 1;

	uint32_t ts_replay_start, ts_job_start, ts_job_end;
	uint32_t tot_comp_time = 0;

	/** struct gr_ioctl_irqfds param; */

	EE("Start replay...");
	ts_replay_start = k2_measure("start replaying");
	/** gpu_soft_reset(); */

	for (int i = 0; i < rctx->list->cnt; i++) {
		cmd = cur_io->offset;
		value = cur_io->value;
		VV("[%d] %ld, %c, 0x%08x, 0x%08x", i, cur_io->ts, cur_io->is_read, cur_io->offset, cur_io->value);

		/********************************* Read *****************************/
		if (cur_io->is_read == 'R') {
			new_value = gr_reg_read(cmd);
			switch(cmd) {
				case GPU_CONTROL_REG(SHADER_READY_LO):
				case GPU_CONTROL_REG(TILER_READY_LO):
					/** if (new_value != cur_io->value) { 
					 *     EE("read value unmatched with the old one");
					 *     exit(1);
					 * } */
					break;
				case MMU_AS_REG(GR_AS_NR, AS_STATUS):
					if (value == AS_STATUS_AS_ACTIVE)
						reg_read_wait(cmd, AS_STATUS_AS_INACTIVE);
				case GPU_CONTROL_REG(LATEST_FLUSH):
					/** Keep this state for next reg write */
					latest_flush_id = new_value;
					break;
				default:
					/* ignore */
					break;
			}

		/********************************* Write ****************************/
		} else {
			switch (cmd) {
			case JOB_SLOT_REG(GR_JS, JS_HEAD_NEXT_LO):
			case JOB_SLOT_REG(GR_JS, JS_HEAD_NEXT_HI):
				gr_js_tune(rctx, cmd, value);
				break;
			case JOB_SLOT_REG(GR_JS, JS_COMMAND_NEXT):
				/** if (!rctx->is_first_run && job_submit_cnt <= config->nr_prep_jobs) {
				  *     job_submit_cnt++;
				  *     continue;
				  * } */
				EE("[%d] ------------------------- submit job %u  --------------------------", i, job_submit_cnt);
				if (config->test_checkpoint) {
					k2_measure("job_cache_invalidate");
					gr_cache_invalidate();
					gr_as_flush(rctx->new_transtab);
					k2_measure("job_cache_invalidate done");
					gr_write_checkpoint(rctx);
				}
				break;
			case JOB_SLOT_REG(GR_JS, JS_FLUSH_ID_NEXT):
				VV("original: %x next_flush_id: %x", value, latest_flush_id);
				value = latest_flush_id;
				break;
			case JOB_SLOT_REG(GR_JS, JS_CONFIG_NEXT):
				/** Turn off auto cache invalidate, flush. We manually do it for checkpointing */
				if (config->test_checkpoint)
					value = JS_CONFIG_START_FLUSH_NO_ACTION;
				VV("config_next_job: %x", value);
				break;
			case MMU_AS_REG(GR_AS_NR, AS_TRANSTAB_LO):
				WW("SetGPUPgt(lo) | from 0x%x -> 0x%lx", value, rctx->new_transtab & 0xFFFFFFFFUL);
				value = rctx->new_transtab & 0xFFFFFFFFUL;
				break;
			case MMU_AS_REG(GR_AS_NR, AS_TRANSTAB_HI):
				WW("SetGPUPgt(hi)| from 0x%x -> 0x%lx", value, (rctx->new_transtab >> 32) & 0xFFFFFFFFUL);
				value = (rctx->new_transtab >> 32) & 0xFFFFFFFFUL;
				break;
			default:
				break;
			}

			/* add delay */
			for (volatile int k = 0; k < 1000; k++);
			gr_reg_write(cmd, value);

			switch (cmd) {
			case JOB_SLOT_REG(GR_JS, JS_COMMAND_NEXT) :
				/****************************************************************************/
				/* Synchronize job: after write the next command, we wait for the interrupt */
				/****************************************************************************/
				ts_job_start = k2_measure("job_start");
				
				if (job_submit_cnt == 1)
					rctx->ts_prep_start = k2_measure("prep_start");
				if (job_submit_cnt == config->nr_prep_jobs + 1)
					rctx->ts_comp_start = k2_measure("comp_start");

				/******** Wait for job irq ********/
				gr_wait_job_irq(0.05 /* timeout */);
				/**********************************/

				ts_job_end = k2_measure("job_end");

				if (job_submit_cnt > config->nr_prep_jobs && job_submit_cnt <= config->nr_tot_jobs) {
					tot_comp_time += (ts_job_end - ts_job_start);
				}

				if (job_submit_cnt == config->nr_prep_jobs) {
					rctx->ts_prep_end = k2_measure("prep_end");
				} else if (job_submit_cnt == config->nr_tot_jobs) {
					rctx->ts_comp_end = k2_measure("comp_end");
					k2_measure("done");
					break;
				}

				if (job_submit_cnt > config->nr_prep_jobs && (job_submit_cnt - config->nr_prep_jobs) % 32 == 0){
					k2_measure("resume point");
				}

				job_submit_cnt++;
				break;
			case MMU_AS_REG(GR_AS_NR, AS_COMMAND):
				if (value ==  AS_COMMAND_FLUSH_MEM || value == AS_COMMAND_FLUSH_PT)
					/** XXX may need to flush page table */
					VV("AS_COMMAND_FLUSH_MEM/PT");
				break;
			case GPU_CONTROL_REG(GPU_COMMAND):
				if (value == GPU_COMMAND_SOFT_RESET) {
					VV("GPU_COMMAND_SOFT_RESET");
					gr_reg_write(GPU_CONTROL_REG(GPU_IRQ_MASK), 0x100);
				}
			case GPU_CONTROL_REG(TILER_PWRON_LO):
			case GPU_CONTROL_REG(SHADER_PWRON_LO):
				/** usleep(1); */
				/** gr_wait_gpu_irq(0.05); */
			default:
				break;
			}
		}

		if(config->is_validate && !gr_is_same_regmap(rctx, cur_io, i)) {
			perror("validation failed, something wrong");
		}

		cur_io = cur_io->link;
	}	// end of loop

	rctx->tot_prep_time += rctx->ts_prep_end - ts_replay_start;
	rctx->tot_comp_time += rctx->ts_comp_end - rctx->ts_comp_start;
	EE("tot_only_comp_time = %u", tot_comp_time);
	k2_measure_clean();
}

struct replay_context *gr_rctx_init(gr_config *config) {
	struct replay_context *rctx;
	rctx = (struct replay_context *) malloc (sizeof(struct replay_context));
	memset(rctx, 0, sizeof(struct replay_context));

	rctx->phys_mem_handle = open("/dev/mem", O_RDWR | O_DSYNC);
	if (rctx->phys_mem_handle < 0) {
		perror("cannot open /dev/mem");
		gr_rctx_term(rctx);
		exit(1);
	}

	memcpy(&rctx->config, config, sizeof(gr_config));

	rctx->list = gr_reg_init_list(config->io_path);

	gr_mem_init(rctx);
	return rctx;
}

static void free_user_pgtable(struct user_pgtable *user_pgt)
{
	if (user_pgt) {
		for (int i = 0; i < 512; i++)
			if (user_pgt->pgd[i])
				free_user_pgtable(user_pgt->pgd[i]);
		gr_unmap_page(user_pgt->mapped_page);
		free(user_pgt);
	}
}

void gr_rctx_term(struct replay_context *rctx) {
	WW("========================= Replay context term ======================== ");

	if (!rctx) {
		EE("given rctx is null");
		return;
	}

	gr_mem_term(rctx);

	if (rctx->list)
		gr_reg_term_list(rctx->list);
	
	// free userland page table
	free_user_pgtable(rctx->user_pgt);

	if (rctx->phys_mem_handle >= 0)
		close(rctx->phys_mem_handle);

	free(rctx);
}

struct reg_list *gr_reg_init_list(const char *input)
{
	struct reg_list *list;
	struct reg_io *io;
	FILE *reg_fp;

	list = (struct reg_list *) malloc (sizeof(struct reg_list));
	list->cnt = 0;

	reg_fp = fopen(input, "r");
	xzl_bug_on_msg(!reg_fp, "File read fail");

	II("===================== Load Register I/O  ===================");

	while (!feof(reg_fp)) {
		io = (struct reg_io *) malloc (sizeof(*io));
		fscanf(reg_fp, "%lu,%c,%x,%x\n", &io->ts, &io->is_read, &io->offset, &io->value);
		DD("%lld, %c, 0x%08x, 0x%08x", io->ts, io->is_read, io->offset, io->value);

		if (list->cnt == 0) {
			list->head = list->pos = list->tail = io;
		} else {
			list->tail->link = io;
			list->tail = io;
		}
		list->cnt++;
	}

	II("reg list construction done. Total cnt = %d", list->cnt);

	return list;
}

void gr_reg_term_list(struct reg_list *list) {
	struct reg_io *io;
	int i;

	for (i = 0; i < list->cnt; i++) {
		io = list->pos;
		list->pos = list->pos->link;
		free(io);
	}
	free(list);
}

__attribute__((constructor))
void gr_init() {
	gr_manager_init();
}

__attribute__((destructor))	
void gr_fini() {
	gr_manager_term();
}
