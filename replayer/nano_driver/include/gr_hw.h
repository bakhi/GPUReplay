#ifndef _GR_HW_H_
#define _GR_HW_H_

#include <linux/kernel.h>
#include "mali_kbase_gpu_regmap.h"

void gr_reg_write(u32 offset, u32 value);
u32 gr_reg_read(u32 offset);

#endif
