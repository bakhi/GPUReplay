#include <linux/io.h>
#include "include/gr_hw.h"

extern void __iomem *reg;

void gr_reg_write(u32 offset, u32 value) {
	writel(value, reg + offset);
}

u32 gr_reg_read(u32 offset) {
	u32 val;
	val = readl(reg + offset);
	return val;
}
