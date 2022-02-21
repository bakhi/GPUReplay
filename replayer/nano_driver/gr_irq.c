#include <linux/interrupt.h>
#include <linux/delay.h>
#include <linux/poll.h>
#include <linux/anon_inodes.h>

#include "include/gr_irq.h"
#include "include/gr_hw.h"
#include "include/gr.h"

atomic_t is_job_irq;
atomic_t is_gpu_irq;

struct gr_irq irqs[3];
u32 job_irq_cnt = 0;
u32 gpu_irq_cnt = 0;

static void *tgx_tag(void *ptr, u32 tag) {
	return (void *)(((uintptr_t) ptr) | tag);
}

static void *tgx_untag(void *ptr) {
    return (void *)(((uintptr_t) ptr) & ~3);
}

static unsigned int gpu_irq_poll(struct file *filp, poll_table *wait) {
	struct gr_device *grdev;

	grdev = filp->private_data; 
	poll_wait(filp, &grdev->gpu_irq_queue, wait);

	if (atomic_read(&is_gpu_irq)) {
		atomic_dec(&is_gpu_irq);
		return POLLIN;
	}
	return 0;
}

static const struct file_operations gpu_irq_fops = {
	.poll = gpu_irq_poll,
};

unsigned int gpu_irq_init(struct gr_device *grdev) {
	atomic_set(&is_gpu_irq, 0);
	return anon_inode_getfd(
		"[gr_gpu_irq_desc]",
		&gpu_irq_fops,
		&grdev,
		O_RDONLY | O_CLOEXEC);
}

static unsigned int job_irq_poll(struct file *filp, poll_table *wait) {
	struct gr_device *grdev;

	grdev = filp->private_data; 
	poll_wait(filp, &grdev->job_irq_queue, wait);

	if (atomic_read(&is_job_irq)) {
		atomic_dec(&is_job_irq);
		return POLLIN | POLLRDNORM;
	}
	return 0;
}

static const struct file_operations job_irq_fops = {
	.poll = job_irq_poll,
};

unsigned int job_irq_init(struct gr_device *grdev) {
	atomic_set(&is_job_irq, 0);
	return anon_inode_getfd(
		"[gr_job_irq_desc]",
		&job_irq_fops,
		&grdev,
		O_RDONLY | O_CLOEXEC);
}

static irqreturn_t gr_job_irq_handler(int irq, void *data) {
	struct gr_device *grdev = tgx_untag(data);
	u32 done;
	int i;

	++job_irq_cnt;
	wake_up_interruptible(&grdev->job_irq_queue);

#ifdef DEBUG
	printk(KERN_INFO "----[%d JOB IRQ %u]----", JOB_IRQ_TAG, job_irq_cnt);
#endif

	done = gr_reg_read(JOB_CONTROL_REG(JOB_IRQ_STATUS));

	while(done) {
		u32 failed = done >> 16;
		u32 finished = (done & 0xFFFF) | failed;

		i = ffs(finished) - 1;

		gr_reg_write(JOB_CONTROL_REG(JOB_IRQ_CLEAR), done & ((1 << i) | (1 << (i + 16))));
		done = gr_reg_read(JOB_CONTROL_REG(JOB_IRQ_RAWSTAT));
	}
	
	atomic_inc(&is_job_irq);
	return IRQ_HANDLED;
}

static irqreturn_t gr_mmu_irq_handler(int irq, void *data) {
#ifdef DEBUG
	printk(KERN_INFO "----[%d MMU IRQ]----", MMU_IRQ_TAG);
#endif

	// disable IRQ
	gr_reg_write(MMU_REG(MMU_IRQ_MASK), 0);
	gr_reg_write(MMU_REG(MMU_IRQ_CLEAR), 0xFFFFFFFF);

	// enable IRQ
	gr_reg_write(MMU_REG(MMU_IRQ_CLEAR), 0xFFFFFFFF);
	gr_reg_write(MMU_REG(MMU_IRQ_MASK), 0xFFFFFFFF);

	return IRQ_HANDLED;
}

static irqreturn_t gr_gpu_irq_handler(int irq, void *data) {
	/** struct gr_device *grdev = tgx_untag(data); */
	u32 val;

	++gpu_irq_cnt;

	/** wake_up_interruptible(&grdev->gpu_irq_queue); */

#ifdef DEBUG
	printk(KERN_INFO "----[%d GPU IRQ %u]----", GPU_IRQ_TAG, gpu_irq_cnt);
#endif

	val = gr_reg_read(GPU_CONTROL_REG(GPU_IRQ_STATUS));
	gr_reg_write(GPU_CONTROL_REG(GPU_IRQ_CLEAR), val);

	atomic_inc(&is_gpu_irq);
	return IRQ_HANDLED;
}

static irq_handler_t gr_handler_table[] = {
	[JOB_IRQ_TAG] = gr_job_irq_handler,
	[MMU_IRQ_TAG] = gr_mmu_irq_handler,
	[GPU_IRQ_TAG] = gr_gpu_irq_handler,
};


static void gr_get_irqs(void) {
	u32 nr = ARRAY_SIZE(gr_handler_table);
	u32 i;

	irqs[0].irq = 64;	// job
	irqs[1].irq = 65;	// mmu
	irqs[2].irq = 66;	// gpu

	for (i = 0 ; i <nr; i++) {
		irqs[i].flags = 0x4;
	}
}

int gr_install_interrupts(struct gr_device *grdev) {
	u32 nr = ARRAY_SIZE(gr_handler_table);
	int err;
	u32 i;

	gr_get_irqs();

	for (i = 0; i < nr; i++) {
		err = request_irq(irqs[i].irq, gr_handler_table[i],
				irqs[i].flags | IRQF_SHARED,
				"e82c0000.mali", tgx_tag(grdev, i));
		if (err) {
			printk(KERN_INFO "irq[%d] install fail: %x", irqs[i].irq, err);
		}
	}
	return err;
}

void gr_release_interrupts(struct gr_device *grdev) {
	u32 nr = ARRAY_SIZE(gr_handler_table);
	u32 i;

	for (i = 0; i < nr; i++) {
		free_irq(irqs[i].irq, tgx_tag(grdev, i));
	}
}
