#ifndef _GR_IRQ_H_
#define _GR_IRQ_H_

/* GPU IRQ Tags */
#define JOB_IRQ_TAG 0
#define MMU_IRQ_TAG 1
#define GPU_IRQ_TAG 2

#include "gr.h"

enum gr_irq_type {
	IRQ_TYPE_JOB,
	IRQ_TYPE_MMU,
	IRQ_TYPE_GPU,
};

struct gr_irq {
	int irq;
	int flags;
	atomic_t irq_count;
	wait_queue_head_t irq_queue;
};

int gr_install_interrupts(struct gr_device *grdev);
void gr_release_interrupts(struct gr_device *grdev);

u64 gr_wait_irq(struct gr_device *grdev, enum gr_irq_type irq);
u64 gr_wait_gpu_irq(struct gr_device *grdev);
u64 gr_wait_job_irq(struct gr_device *grdev);

unsigned int job_irq_init(struct gr_device *grdev);
unsigned int gpu_irq_init(struct gr_device *grdev);

#endif
