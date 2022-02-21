/*
 *
 * (C) COPYRIGHT 2010-2019 ARM Limited. All rights reserved.
 *
 * This program is free software and is provided to you under the terms of the
 * GNU General Public License version 2 as published by the Free Software
 * Foundation, and any use by you of this program is subject to the terms
 * of such GNU licence.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, you can access it online at
 * http://www.gnu.org/licenses/gpl-2.0.html.
 *
 * SPDX-License-Identifier: GPL-2.0
 *
 */



/*
 * Base kernel device APIs
 */

#include <linux/debugfs.h>
#include <linux/dma-mapping.h>
#include <linux/seq_file.h>
#include <linux/kernel.h>
#include <linux/module.h>
#include <linux/of_platform.h>
#include <linux/types.h>

#include <mali_kbase.h>
#include <mali_kbase_defs.h>
#include <mali_kbase_hwaccess_instr.h>
#include <mali_kbase_hw.h>
#include <mali_kbase_config_defaults.h>

#include <tl/mali_kbase_timeline.h>
#include "mali_kbase_vinstr.h"
#include "mali_kbase_hwcnt_context.h"
#include "mali_kbase_hwcnt_virtualizer.h"

#include "mali_kbase_device.h"
#include "mali_kbase_device_internal.h"
#include "backend/gpu/mali_kbase_pm_internal.h"
#include "backend/gpu/mali_kbase_irq_internal.h"

#ifdef CONFIG_GR_DEBUG_FS_VERBOSE
/* The number of nanoseconds in a second. */
#define NSECS_IN_SEC       1000000000ull /* ns */
#endif

/* NOTE: Magic - 0x45435254 (TRCE in ASCII).
 * Supports tracing feature provided in the base module.
 * Please keep it in sync with the value of base module.
 */
#define TRACE_BUFFER_HEADER_SPECIAL 0x45435254

#if KBASE_TRACE_ENABLE
static const char *kbasep_trace_code_string[] = {
	/* IMPORTANT: USE OF SPECIAL #INCLUDE OF NON-STANDARD HEADER FILE
	 * THIS MUST BE USED AT THE START OF THE ARRAY */
#define KBASE_TRACE_CODE_MAKE_CODE(X) # X
#include "tl/mali_kbase_trace_defs.h"
#undef  KBASE_TRACE_CODE_MAKE_CODE
};
#endif

#define DEBUG_MESSAGE_SIZE 256

/* Number of register accesses for the buffer that we allocate during
 * initialization time. The buffer size can be changed later via debugfs.
 */
#ifdef CONFIG_GR_DEBUG_FS_VERBOSE
#define KBASEP_DEFAULT_REGISTER_HISTORY_SIZE ((u16)(1 << 15))	// jin
#else
#define KBASEP_DEFAULT_REGISTER_HISTORY_SIZE ((u16)512)
#endif

static DEFINE_MUTEX(kbase_dev_list_lock);
static LIST_HEAD(kbase_dev_list);
static int kbase_dev_nr;

static int kbasep_trace_init(struct kbase_device *kbdev);
static void kbasep_trace_term(struct kbase_device *kbdev);
static void kbasep_trace_hook_wrapper(void *param);

struct kbase_device *kbase_device_alloc(void)
{
	return kzalloc(sizeof(struct kbase_device), GFP_KERNEL);
}

// jin: kbase->as is array of objects representing address spaces of GPU
static int kbase_device_as_init(struct kbase_device *kbdev, int i)
{
	kbdev->as[i].number = i;
	kbdev->as[i].bf_data.addr = 0ULL;
	kbdev->as[i].pf_data.addr = 0ULL;

	kbdev->as[i].pf_wq = alloc_workqueue("mali_mmu%d", 0, 1, i);
	if (!kbdev->as[i].pf_wq)
		return -EINVAL;

	INIT_WORK(&kbdev->as[i].work_pagefault, page_fault_worker);
	INIT_WORK(&kbdev->as[i].work_busfault, bus_fault_worker);

	return 0;
}

static void kbase_device_as_term(struct kbase_device *kbdev, int i)
{
	destroy_workqueue(kbdev->as[i].pf_wq);
}

static int kbase_device_all_as_init(struct kbase_device *kbdev)
{
	int i, err;

	for (i = 0; i < kbdev->nr_hw_address_spaces; i++) {
		err = kbase_device_as_init(kbdev, i);
		if (err)
			goto free_workqs;
	}

	return 0;

free_workqs:
	for (; i > 0; i--)
		kbase_device_as_term(kbdev, i);

	return err;
}

static void kbase_device_all_as_term(struct kbase_device *kbdev)
{
	int i;

	for (i = 0; i < kbdev->nr_hw_address_spaces; i++)
		kbase_device_as_term(kbdev, i);
}

int kbase_device_misc_init(struct kbase_device * const kbdev)
{
	int err;
#ifdef CONFIG_ARM64
	struct device_node *np = NULL;
#endif /* CONFIG_ARM64 */

	spin_lock_init(&kbdev->mmu_mask_change);
	mutex_init(&kbdev->mmu_hw_mutex);
#ifdef CONFIG_ARM64
	kbdev->cci_snoop_enabled = false;
	np = kbdev->dev->of_node;
	if (np != NULL) {
		if (of_property_read_u32(np, "snoop_enable_smc",
					&kbdev->snoop_enable_smc))
			kbdev->snoop_enable_smc = 0;
		if (of_property_read_u32(np, "snoop_disable_smc",
					&kbdev->snoop_disable_smc))
			kbdev->snoop_disable_smc = 0;
		/* Either both or none of the calls should be provided. */
		if (!((kbdev->snoop_disable_smc == 0
			&& kbdev->snoop_enable_smc == 0)
			|| (kbdev->snoop_disable_smc != 0
			&& kbdev->snoop_enable_smc != 0))) {
			WARN_ON(1);
			err = -EINVAL;
			goto fail;
		}
	}
#endif /* CONFIG_ARM64 */
	/* Get the list of workarounds for issues on the current HW
	 * (identified by the GPU_ID register)
	 */
	err = kbase_hw_set_issues_mask(kbdev);
	if (err)
		goto fail;

	/* Set the list of features available on the current HW
	 * (identified by the GPU_ID register)
	 */
	kbase_hw_set_features_mask(kbdev);

	kbase_gpuprops_set_features(kbdev);

	/* On Linux 4.0+, dma coherency is determined from device tree */
#if defined(CONFIG_ARM64) && LINUX_VERSION_CODE < KERNEL_VERSION(4, 0, 0)
	set_dma_ops(kbdev->dev, &noncoherent_swiotlb_dma_ops);
#endif

	/* Workaround a pre-3.13 Linux issue, where dma_mask is NULL when our
	 * device structure was created by device-tree
	 */
	if (!kbdev->dev->dma_mask)
		kbdev->dev->dma_mask = &kbdev->dev->coherent_dma_mask;

	err = dma_set_mask(kbdev->dev,
			DMA_BIT_MASK(kbdev->gpu_props.mmu.pa_bits));
	if (err)
		goto dma_set_mask_failed;

	err = dma_set_coherent_mask(kbdev->dev,
			DMA_BIT_MASK(kbdev->gpu_props.mmu.pa_bits));
	if (err)
		goto dma_set_mask_failed;

	kbdev->nr_hw_address_spaces = kbdev->gpu_props.num_address_spaces;

	err = kbase_device_all_as_init(kbdev);
	if (err)
		goto as_init_failed;

	spin_lock_init(&kbdev->hwcnt.lock);

	err = kbasep_trace_init(kbdev);
	if (err)
		goto term_as;

	init_waitqueue_head(&kbdev->cache_clean_wait);

	kbase_debug_assert_register_hook(&kbasep_trace_hook_wrapper, kbdev);

	atomic_set(&kbdev->ctx_num, 0);

	err = kbase_instr_backend_init(kbdev);
	if (err)
		goto term_trace;

	kbdev->pm.dvfs_period = DEFAULT_PM_DVFS_PERIOD;

	kbdev->reset_timeout_ms = DEFAULT_RESET_TIMEOUT_MS;

	if (kbase_hw_has_feature(kbdev, BASE_HW_FEATURE_AARCH64_MMU))
		kbdev->mmu_mode = kbase_mmu_mode_get_aarch64();
	else
		kbdev->mmu_mode = kbase_mmu_mode_get_lpae();

	mutex_init(&kbdev->kctx_list_lock);
	INIT_LIST_HEAD(&kbdev->kctx_list);

	return 0;
term_trace:
	kbasep_trace_term(kbdev);
term_as:
	kbase_device_all_as_term(kbdev);
as_init_failed:
dma_set_mask_failed:
fail:
	return err;
}

void kbase_device_misc_term(struct kbase_device *kbdev)
{
	KBASE_DEBUG_ASSERT(kbdev);

	WARN_ON(!list_empty(&kbdev->kctx_list));

#if KBASE_TRACE_ENABLE
	kbase_debug_assert_register_hook(NULL, NULL);
#endif

	kbase_instr_backend_term(kbdev);

	kbasep_trace_term(kbdev);

	kbase_device_all_as_term(kbdev);
}

void kbase_device_free(struct kbase_device *kbdev)
{
	kfree(kbdev);
}

void kbase_device_id_init(struct kbase_device *kbdev)
{
	scnprintf(kbdev->devname, DEVNAME_SIZE, "%s%d", kbase_drv_name,
			kbase_dev_nr);
	kbdev->id = kbase_dev_nr++;
}

int kbase_device_hwcnt_backend_gpu_init(struct kbase_device *kbdev)
{
	return kbase_hwcnt_backend_gpu_create(kbdev, &kbdev->hwcnt_gpu_iface);
}

void kbase_device_hwcnt_backend_gpu_term(struct kbase_device *kbdev)
{
	kbase_hwcnt_backend_gpu_destroy(&kbdev->hwcnt_gpu_iface);
}

int kbase_device_hwcnt_context_init(struct kbase_device *kbdev)
{
	return kbase_hwcnt_context_init(&kbdev->hwcnt_gpu_iface,
			&kbdev->hwcnt_gpu_ctx);
}

void kbase_device_hwcnt_context_term(struct kbase_device *kbdev)
{
	kbase_hwcnt_context_term(kbdev->hwcnt_gpu_ctx);
}

int kbase_device_hwcnt_virtualizer_init(struct kbase_device *kbdev)
{
	return kbase_hwcnt_virtualizer_init(kbdev->hwcnt_gpu_ctx,
			KBASE_HWCNT_GPU_VIRTUALIZER_DUMP_THRESHOLD_NS,
			&kbdev->hwcnt_gpu_virt);
}

void kbase_device_hwcnt_virtualizer_term(struct kbase_device *kbdev)
{
	kbase_hwcnt_virtualizer_term(kbdev->hwcnt_gpu_virt);
}

int kbase_device_timeline_init(struct kbase_device *kbdev)
{
	u32 tlstream_enabled = (1 << 31) | 1; // jin :TLSTREAM_ENALBED = (1 << 31)
	atomic_set(&kbdev->timeline_is_enabled, tlstream_enabled);	// jin: turn on timeline
	/** atomic_set(&kbdev->timeline_is_enabled, 0);	// jin: turn on timeline */
	return kbase_timeline_init(&kbdev->timeline,
			&kbdev->timeline_is_enabled);
}

void kbase_device_timeline_term(struct kbase_device *kbdev)
{
	kbase_timeline_term(kbdev->timeline);
}

int kbase_device_vinstr_init(struct kbase_device *kbdev)
{
	return kbase_vinstr_init(kbdev->hwcnt_gpu_virt, &kbdev->vinstr_ctx);
}

void kbase_device_vinstr_term(struct kbase_device *kbdev)
{
	kbase_vinstr_term(kbdev->vinstr_ctx);
}

int kbase_device_io_history_init(struct kbase_device *kbdev)
{
	return kbase_io_history_init(&kbdev->io_history,
			KBASEP_DEFAULT_REGISTER_HISTORY_SIZE);
}

void kbase_device_io_history_term(struct kbase_device *kbdev)
{
	kbase_io_history_term(&kbdev->io_history);
}

int kbase_device_misc_register(struct kbase_device *kbdev)
{
	return misc_register(&kbdev->mdev);
}

void kbase_device_misc_deregister(struct kbase_device *kbdev)
{
	misc_deregister(&kbdev->mdev);
}

int kbase_device_list_init(struct kbase_device *kbdev)
{
	const struct list_head *dev_list;

	dev_list = kbase_device_get_list();
	list_add(&kbdev->entry, &kbase_dev_list);
	kbase_device_put_list(dev_list);

	return 0;
}

void kbase_device_list_term(struct kbase_device *kbdev)
{
	const struct list_head *dev_list;

	dev_list = kbase_device_get_list();
	list_del(&kbdev->entry);
	kbase_device_put_list(dev_list);
}

const struct list_head *kbase_device_get_list(void)
{
	mutex_lock(&kbase_dev_list_lock);
	return &kbase_dev_list;
}
KBASE_EXPORT_TEST_API(kbase_device_get_list);

void kbase_device_put_list(const struct list_head *dev_list)
{
	mutex_unlock(&kbase_dev_list_lock);
}
KBASE_EXPORT_TEST_API(kbase_device_put_list);

/*
 * Device trace functions
 */
#if KBASE_TRACE_ENABLE

static int kbasep_trace_init(struct kbase_device *kbdev)
{
	struct kbase_trace *rbuf;

	rbuf = kmalloc_array(KBASE_TRACE_SIZE, sizeof(*rbuf), GFP_KERNEL);

	if (!rbuf)
		return -EINVAL;

	kbdev->trace_rbuf = rbuf;
	spin_lock_init(&kbdev->trace_lock);
	return 0;
}

static void kbasep_trace_term(struct kbase_device *kbdev)
{
	kfree(kbdev->trace_rbuf);
}

static void kbasep_trace_format_msg(struct kbase_trace *trace_msg, char *buffer, int len)
{
	s32 written = 0;

#ifdef CONFIG_GR_DEBUG_FS_VERBOSE
	written += MAX(snprintf(buffer + written, MAX(len - written, 0), "[%5llu] ", trace_msg->ts), 0);

	/* Initial part of message */
	written += MAX(snprintf(buffer + written, MAX(len - written, 0), "%s,%p,", kbasep_trace_code_string[trace_msg->code], trace_msg->ctx), 0);
#else

	/* Initial part of message */
	written += MAX(snprintf(buffer + written, MAX(len - written, 0), "%d.%.6d,%d,%d,%s,%p,", (int)trace_msg->timestamp.tv_sec, (int)(trace_msg->timestamp.tv_nsec / 1000), trace_msg->thread_id, trace_msg->cpu, kbasep_trace_code_string[trace_msg->code], trace_msg->ctx), 0);
#endif

	if (trace_msg->katom)
		written += MAX(snprintf(buffer + written, MAX(len - written, 0), "atom %d (ud: 0x%llx 0x%llx)", trace_msg->atom_number, trace_msg->atom_udata[0], trace_msg->atom_udata[1]), 0);

	written += MAX(snprintf(buffer + written, MAX(len - written, 0), ",%.8llx,", trace_msg->gpu_addr), 0);

	/* NOTE: Could add function callbacks to handle different message types */
	/* Jobslot present */
	if (trace_msg->flags & KBASE_TRACE_FLAG_JOBSLOT)
		written += MAX(snprintf(buffer + written, MAX(len - written, 0), "%d", trace_msg->jobslot), 0);

	written += MAX(snprintf(buffer + written, MAX(len - written, 0), ","), 0);

	/* Refcount present */
	if (trace_msg->flags & KBASE_TRACE_FLAG_REFCOUNT)
		written += MAX(snprintf(buffer + written, MAX(len - written, 0), "%d", trace_msg->refcount), 0);

	written += MAX(snprintf(buffer + written, MAX(len - written, 0), ","), 0);

	/* Rest of message */
	written += MAX(snprintf(buffer + written, MAX(len - written, 0), "0x%.8lx", trace_msg->info_val), 0);
}

static void kbasep_trace_dump_msg(struct kbase_device *kbdev, struct kbase_trace *trace_msg)
{
	char buffer[DEBUG_MESSAGE_SIZE];

	kbasep_trace_format_msg(trace_msg, buffer, DEBUG_MESSAGE_SIZE);
	dev_dbg(kbdev->dev, "%s", buffer);
#ifdef CONFIG_GR_DEBUG_FS_VERBOSE
	EE("%s", buffer);
#endif
}

void kbasep_trace_add(struct kbase_device *kbdev, enum kbase_trace_code code, void *ctx, struct kbase_jd_atom *katom, u64 gpu_addr, u8 flags, int refcount, int jobslot, unsigned long info_val)
{
	unsigned long irqflags;
	struct kbase_trace *trace_msg;

	spin_lock_irqsave(&kbdev->trace_lock, irqflags);

	// jin: buffer size is KBASE_TRACE_SIZE (256 entries)
	trace_msg = &kbdev->trace_rbuf[kbdev->trace_next_in];

	/* Fill the message */
	trace_msg->thread_id = task_pid_nr(current);
	trace_msg->cpu = task_cpu(current);

#ifdef CONFIG_GR_DEBUG_FS_VERBOSE
	getrawmonotonic(&trace_msg->timestamp);
	trace_msg->ts = (u64)trace_msg->timestamp.tv_sec * NSECS_IN_SEC + trace_msg->timestamp.tv_nsec; 
#else
	getnstimeofday(&trace_msg->timestamp);
#endif

	trace_msg->code = code;
	trace_msg->ctx = ctx;

	if (NULL == katom) {
		trace_msg->katom = false;
	} else {
		trace_msg->katom = true;
		trace_msg->atom_number = kbase_jd_atom_id(katom->kctx, katom);
		trace_msg->atom_udata[0] = katom->udata.blob[0];
		trace_msg->atom_udata[1] = katom->udata.blob[1];
	}

	trace_msg->gpu_addr = gpu_addr;
	trace_msg->jobslot = jobslot;
	trace_msg->refcount = MIN((unsigned int)refcount, 0xFF);
	trace_msg->info_val = info_val;
	trace_msg->flags = flags;

	/* Update the ringbuffer indices */
	kbdev->trace_next_in = (kbdev->trace_next_in + 1) & KBASE_TRACE_MASK;
	if (kbdev->trace_next_in == kbdev->trace_first_out)
		kbdev->trace_first_out = (kbdev->trace_first_out + 1) & KBASE_TRACE_MASK;

	/* Done */

	spin_unlock_irqrestore(&kbdev->trace_lock, irqflags);
}

void kbasep_trace_clear(struct kbase_device *kbdev)
{
	unsigned long flags;

	spin_lock_irqsave(&kbdev->trace_lock, flags);
	kbdev->trace_first_out = kbdev->trace_next_in;
	spin_unlock_irqrestore(&kbdev->trace_lock, flags);
}

void kbasep_trace_dump(struct kbase_device *kbdev)
{
	unsigned long flags;
	u32 start;
	u32 end;

	dev_dbg(kbdev->dev, "Dumping trace:\nsecs,nthread,cpu,code,ctx,katom,gpu_addr,jobslot,refcount,info_val");
#ifdef CONFIG_GR_DEBUG_FS_VERBOSE
	EE("Dumping trace:\nsecs,nthread,cpu,code,ctx,katom,gpu_addr,jobslot,refcount,info_val");		// jin
#endif
	spin_lock_irqsave(&kbdev->trace_lock, flags);
	start = kbdev->trace_first_out;
	end = kbdev->trace_next_in;

	while (start != end) {
		struct kbase_trace *trace_msg = &kbdev->trace_rbuf[start];

		kbasep_trace_dump_msg(kbdev, trace_msg);

		start = (start + 1) & KBASE_TRACE_MASK;
	}
	dev_dbg(kbdev->dev, "TRACE_END");

	spin_unlock_irqrestore(&kbdev->trace_lock, flags);

	KBASE_TRACE_CLEAR(kbdev);
}

static void kbasep_trace_hook_wrapper(void *param)
{
	struct kbase_device *kbdev = (struct kbase_device *)param;

	kbasep_trace_dump(kbdev);
}

#ifdef CONFIG_DEBUG_FS
struct trace_seq_state {
	struct kbase_trace trace_buf[KBASE_TRACE_SIZE];
	u32 start;
	u32 end;
};

static void *kbasep_trace_seq_start(struct seq_file *s, loff_t *pos)
{
	struct trace_seq_state *state = s->private;
	int i;

	if (*pos > KBASE_TRACE_SIZE)
		return NULL;
	i = state->start + *pos;
	if ((state->end >= state->start && i >= state->end) ||
			i >= state->end + KBASE_TRACE_SIZE)
		return NULL;

	i &= KBASE_TRACE_MASK;

	return &state->trace_buf[i];
}

static void kbasep_trace_seq_stop(struct seq_file *s, void *data)
{
}

static void *kbasep_trace_seq_next(struct seq_file *s, void *data, loff_t *pos)
{
	struct trace_seq_state *state = s->private;
	int i;

	(*pos)++;

	i = (state->start + *pos) & KBASE_TRACE_MASK;
	if (i == state->end)
		return NULL;

	return &state->trace_buf[i];
}

static int kbasep_trace_seq_show(struct seq_file *s, void *data)
{
	struct kbase_trace *trace_msg = data;
	char buffer[DEBUG_MESSAGE_SIZE];

	kbasep_trace_format_msg(trace_msg, buffer, DEBUG_MESSAGE_SIZE);
	seq_printf(s, "%s\n", buffer);
	return 0;
}

static const struct seq_operations kbasep_trace_seq_ops = {
	.start = kbasep_trace_seq_start,
	.next = kbasep_trace_seq_next,
	.stop = kbasep_trace_seq_stop,
	.show = kbasep_trace_seq_show,
};

static int kbasep_trace_debugfs_open(struct inode *inode, struct file *file)
{
	struct kbase_device *kbdev = inode->i_private;
	unsigned long flags;

	struct trace_seq_state *state;

	state = __seq_open_private(file, &kbasep_trace_seq_ops, sizeof(*state));
	if (!state)
		return -ENOMEM;

	spin_lock_irqsave(&kbdev->trace_lock, flags);
	state->start = kbdev->trace_first_out;
	state->end = kbdev->trace_next_in;
	memcpy(state->trace_buf, kbdev->trace_rbuf, sizeof(state->trace_buf));
	spin_unlock_irqrestore(&kbdev->trace_lock, flags);

	return 0;
}

static const struct file_operations kbasep_trace_debugfs_fops = {
	.owner = THIS_MODULE,
	.open = kbasep_trace_debugfs_open,
	.read = seq_read,
	.llseek = seq_lseek,
	.release = seq_release_private,
};

void kbasep_trace_debugfs_init(struct kbase_device *kbdev)
{
	debugfs_create_file("mali_trace", S_IRUGO,
			kbdev->mali_debugfs_directory, kbdev,
			&kbasep_trace_debugfs_fops);
}

#else
void kbasep_trace_debugfs_init(struct kbase_device *kbdev)
{
}
#endif				/* CONFIG_DEBUG_FS */

#else				/* KBASE_TRACE_ENABLE  */
static int kbasep_trace_init(struct kbase_device *kbdev)
{
	CSTD_UNUSED(kbdev);
	return 0;
}

static void kbasep_trace_term(struct kbase_device *kbdev)
{
	CSTD_UNUSED(kbdev);
}

static void kbasep_trace_hook_wrapper(void *param)
{
	CSTD_UNUSED(param);
}

void kbasep_trace_dump(struct kbase_device *kbdev)
{
	CSTD_UNUSED(kbdev);
}
#endif				/* KBASE_TRACE_ENABLE  */

int kbase_device_early_init(struct kbase_device *kbdev)
{
	int err;

	err = kbasep_platform_device_init(kbdev);
	if (err)
		return err;

	err = kbase_pm_runtime_init(kbdev);
	if (err)
		goto fail_runtime_pm;

	/* Ensure we can access the GPU registers */

	// jin: when it does reg access enable or disable, it turn power on and then off
	// measuring time between them is time for GPU hard reset
	// do not print anything between
#if (defined(CONFIG_GR_MEASURE_TIME) && CONFIG_GR_MEASURE_TIME & GR_PM_TIME)
	k2_measure("pm_hard_reset_start");
	kbase_pm_register_access_enable(kbdev);
	kbase_pm_register_access_disable(kbdev);
	k2_measure("pm_hard_reset_done");
#endif

	kbase_pm_register_access_enable(kbdev);

	/* Find out GPU properties based on the GPU feature registers */
	kbase_gpuprops_set(kbdev);

	/* We're done accessing the GPU registers for now. */
	kbase_pm_register_access_disable(kbdev);

	err = kbase_install_interrupts(kbdev);
	if (err)
		goto fail_interrupts;

	return 0;

fail_interrupts:
	kbase_pm_runtime_term(kbdev);
fail_runtime_pm:
	kbasep_platform_device_term(kbdev);

	return err;
}

void kbase_device_early_term(struct kbase_device *kbdev)
{
	kbase_release_interrupts(kbdev);
	kbase_pm_runtime_term(kbdev);
	kbasep_platform_device_term(kbdev);
}
