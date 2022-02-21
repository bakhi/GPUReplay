#ifndef _GR_VALIDATION_H_
#define _GR_VALIDATION_H_

#include "gr_defs.h"

int gr_is_same_regmap(struct replay_context *rctx, struct reg_io *cur_io, uint32_t io_offset); 
void gr_validation_init(struct replay_context *rctx, const char *path);
void gr_validation_term(struct replay_context *rctx);

#endif
