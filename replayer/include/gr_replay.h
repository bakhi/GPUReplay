#ifndef _GR_REPLAY_H_
#define _GR_REPLAY_H_

#include <unistd.h>
#include "log.h"
#include "gr_defs.h"
#include "gr_mem.h"

#define KBASE_AS_INACTIVE_MAX_LOOPS	100000000

__attribute__((unused))
static void gpu_soft_reset() {
	gr_reg_write(GPU_CONTROL_REG(GPU_COMMAND), GPU_COMMAND_SOFT_RESET);
}

__attribute__((unused))
static void tiler_poweron() {
	gr_reg_write(GPU_CONTROL_REG(TILER_PWRON_LO), 1);
}

__attribute__((unused))
static void tiler_poweroff() {
	gr_reg_write(GPU_CONTROL_REG(TILER_PWROFF_LO), 1);
}

__attribute__((unused))
static void shader_poweron() {
	gr_reg_write(GPU_CONTROL_REG(SHADER_PWRON_LO), 0xff);
}

__attribute__((unused))
static void shader_poweroff() {
	gr_reg_write(GPU_CONTROL_REG(SHADER_PWROFF_LO), 0xff);
}

__attribute__((unused))
static void gr_verification() {
	uint32_t val = gr_reg_read(GPU_CONTROL_REG(GPU_ID));
	/* Check whether GPU ID does correspond to client's  */
	EE("GPU_ID: 0x%08x\n", val);
}

struct replay_context *gr_rctx_init(gr_config *config);
void gr_rctx_term(struct replay_context *rctx);

struct reg_list * gr_reg_init_list(const char *input);
void gr_reg_term_list(struct reg_list *list);
void gr_replay_run(struct replay_context *rctx);

#endif
