#include <stdio.h>
#include <stdlib.h>

#include "gr_mem.h"
#include "gr_validation.h"
#include "log.h"

int gr_is_same_regmap(struct replay_context *rctx, struct reg_io *cur_io, uint32_t io_offset) { 
	uint32_t offset, value;
	int ret = 1;

	fread(rctx->regmap, GPU_REG_PA_SIZE, 1, rctx->reg_fp);

	/** omit job after job submission and job irq related part */
	if (cur_io->offset == JOB_CONTROL_REG(JOB_IRQ_STATUS) ||
		cur_io->offset == JOB_SLOT_REG(1, JS_COMMAND_NEXT))
		return 1;
	else if (cur_io->offset == MMU_AS_REG(0, AS_COMMAND) && \
		((cur_io->value == AS_COMMAND_LOCK) || cur_io->value == AS_COMMAND_FLUSH_PT || cur_io->value == AS_COMMAND_FLUSH_MEM))
		return 1;

	for (offset = 0; offset < GPU_REG_PA_SIZE; offset += sizeof(uint32_t)) {
		value = gr_reg_read(offset);
		/** if (offset == 0xf00 || offset == 0x2418) */
   /**      if (offset == 0x34) */
			/** WW("[offset:%x] replay: %x, recorded: %x", offset, value, rctx->regmap[offset/4]); */
		if (offset == MMU_AS_REG(0, AS_TRANSTAB_LO) || \
			offset == MMU_AS_REG(0, AS_TRANSTAB_HI))
			continue;
		if (rctx->regmap[offset/4] != value) {
			EE("not matched(%x). reg val: %x val from recording: %x", offset, value, rctx->regmap[offset/4]); 
			ret = 0;
		}
	}
	if (ret == 0) {
		EE("---------- [%d] given cmd: 0x%08x, value: 0x%08x", io_offset, cur_io->offset, cur_io->value); 
		getchar();
	}

	return 1;
}

void gr_validation_init(struct replay_context *rctx, const char *path) {
	rctx->reg_fp = fopen(path, "r");
	xzl_bug_on_msg(!rctx->reg_fp, "snapshot file read fails");
	rctx->regmap = (uint32_t *) malloc (GPU_REG_PA_SIZE);
}

void gr_validation_term(struct replay_context *rctx) {
	free(rctx->regmap);
	fclose(rctx->reg_fp);
}
