// SPDX-License-Identifier: GPL-2.0
/*
 *
 * (C) COPYRIGHT 2019-2020 ARM Limited. All rights reserved.
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
 * Base kernel context APIs for Job Manager GPUs
 */

#include <context/mali_kbase_context_internal.h>
#include <gpu/mali_kbase_gpu_regmap.h>
#include <mali_kbase.h>
#include <mali_kbase_ctx_sched.h>
#include <mali_kbase_dma_fence.h>
#include <mali_kbase_mem_linux.h>
#include <mali_kbase_mem_pool_group.h>
#include <mmu/mali_kbase_mmu.h>
#include <tl/mali_kbase_timeline.h>
#include <tl/mali_kbase_tracepoints.h>

#ifdef CONFIG_DEBUG_FS
#include <mali_kbase_debug_mem_view.h>
#include <mali_kbase_mem_pool_debugfs.h>

#include <linux/kernel.h>	// jin
#ifdef CONFIG_GR
#include "gr/gr_defs.h"
#endif

void kbase_context_debugfs_init(struct kbase_context *const kctx)
{
	kbase_debug_mem_view_init(kctx);
	kbase_mem_pool_debugfs_init(kctx->kctx_dentry, kctx);
	kbase_jit_debugfs_init(kctx);
	kbasep_jd_debugfs_ctx_init(kctx);
	kbase_debug_job_fault_context_init(kctx);
}
KBASE_EXPORT_SYMBOL(kbase_context_debugfs_init);

void kbase_context_debugfs_term(struct kbase_context *const kctx)
{
	debugfs_remove_recursive(kctx->kctx_dentry);
	kbase_debug_job_fault_context_term(kctx);
}
KBASE_EXPORT_SYMBOL(kbase_context_debugfs_term);
#endif /* CONFIG_DEBUG_FS */

static int kbase_context_kbase_timer_setup(struct kbase_context *kctx)
{
	kbase_timer_setup(&kctx->soft_job_timeout,
			  kbasep_soft_job_timeout_worker);

	return 0;
}

static int kbase_context_submit_check(struct kbase_context *kctx)
{
	struct kbasep_js_kctx_info *js_kctx_info = &kctx->jctx.sched_info;
	unsigned long irq_flags = 0;

	base_context_create_flags const flags = kctx->create_flags;

	mutex_lock(&js_kctx_info->ctx.jsctx_mutex);
	spin_lock_irqsave(&kctx->kbdev->hwaccess_lock, irq_flags);

	/* Translate the flags */
	if ((flags & BASE_CONTEXT_SYSTEM_MONITOR_SUBMIT_DISABLED) == 0)
		kbase_ctx_flag_clear(kctx, KCTX_SUBMIT_DISABLED);

	spin_unlock_irqrestore(&kctx->kbdev->hwaccess_lock, irq_flags);
	mutex_unlock(&js_kctx_info->ctx.jsctx_mutex);

	return 0;
}

static const struct kbase_context_init context_init[] = {
	{kbase_context_common_init, kbase_context_common_term, NULL},
	{kbase_context_mem_pool_group_init, kbase_context_mem_pool_group_term,
			"Memory pool goup initialization failed"},
	{kbase_mem_evictable_init, kbase_mem_evictable_deinit,
			"Memory evictable initialization failed"},
	{kbasep_js_kctx_init, kbasep_js_kctx_term,
			"JS kctx initialization failed"},
	{kbase_jd_init, kbase_jd_exit,
			"JD initialization failed"},
	{kbase_event_init, kbase_event_cleanup,
			"Event initialization failed"},
	{kbase_dma_fence_init, kbase_dma_fence_term,		// jin: not enabled by default
			"DMA fence initialization failed"},
#ifdef CONFIG_GR
	//jin : should be earlier than context_mmu_init
	{gr_context_init, gr_context_term, "GR context initialization failed"},
#endif
	{kbase_context_mmu_init, kbase_context_mmu_term,
			"MMU initialization failed"},
	{kbase_context_mem_alloc_page, kbase_context_mem_pool_free,
			"Memory alloc page failed"},
	{kbase_region_tracker_init, kbase_region_tracker_term,
			"Region tracker initialization failed"},
	{kbase_sticky_resource_init, kbase_context_sticky_resource_term,
			"Sticky resource initialization failed"},
	{kbase_jit_init, kbase_jit_term,
			"JIT initialization failed"},
	{kbase_context_kbase_timer_setup, NULL, NULL},
	{kbase_context_submit_check, NULL, NULL},
};

static void kbase_context_term_partial(
	struct kbase_context *kctx,
	unsigned int i)
{
	while (i-- > 0) {
		if (context_init[i].term)
			context_init[i].term(kctx);
	}
}

struct kbase_context *kbase_create_context(struct kbase_device *kbdev,
	bool is_compat,
	base_context_create_flags const flags,
	unsigned long const api_version,
	struct file *const filp)
{
	struct kbase_context *kctx;
	unsigned int i = 0;

/** #if (defined(CONFIG_GR_MEASURE_TIME) && CONFIG_GR_MEASURE_TIME &  GR_CONTEXT_EXEC_TIME) */

#if 0
	k2_measure_clean();

	struct kbase_pm_backend_data *backend = &kbdev->pm.backend;
	// jin: If true, slow down GPU clock during L2 power cycle
	EE("gpu_clock_slow_down_wa: %d", backend->gpu_clock_slow_down_wa);
	
	// jin: True if we want lower GPU clock for safe L2 power cycle.
	// False if want GPU clock to back to normalized one.
	// This is updated only in L2 state machine, kbase_pm_l2_update_state
	EE("gpu_clock_slow_down_desired: %d", backend->gpu_clock_slow_down_desired);

	// jin: During L2 power cycle,
	// True: if gpu clock is set at lower frequency for safe L2 power down
	// False: if gpu clock gets restored to previous speed.
	// This is updated only in work function, kbase_pm_gpu_clock_control_worker.
	EE("gpu_clock_slowed_down: %d", backend->gpu_clock_slowed_down);

	EE("protected_entry_transition_override: %d", backend->protected_entry_transition_override);
	
	EE("protected_transition_override: %d", backend->protected_transition_override);

	EE("protected_l2_override: %d", backend->protected_l2_override);

	EE("shaders_desired: %d", backend->shaders_desired);

	EE("l2_desired: %d", backend->l2_desired);

	extern bool corestack_driver_control;
	EE("corestack driver control: %d", corestack_driver_control);

#endif

#ifdef CONFIG_GR_MEASURE_TIME
	k2_measure("context init");
#endif

	if (WARN_ON(!kbdev))
		return NULL;

	/* Validate flags */
	if (WARN_ON(flags != (flags & BASEP_CONTEXT_CREATE_KERNEL_FLAGS)))
		return NULL;

	/* zero-inited as lot of code assume it's zero'ed out on create */
	kctx = vzalloc(sizeof(*kctx));
	if (WARN_ON(!kctx))
		return NULL;

	kctx->kbdev = kbdev;
	kctx->api_version = api_version;
	kctx->filp = filp;
	kctx->create_flags = flags;

	if (is_compat)
		kbase_ctx_flag_set(kctx, KCTX_COMPAT);
#if defined(CONFIG_64BIT)
	else
		kbase_ctx_flag_set(kctx, KCTX_FORCE_SAME_VA);
#endif /* !defined(CONFIG_64BIT) */

	for (i = 0; i < ARRAY_SIZE(context_init); i++) {
		int err = context_init[i].init(kctx);
		WW("[jin][%d] %s but initialized", 
							i, context_init[i].err_mes);	// jin

		if (err) {
			dev_err(kbdev->dev, "%s error = %d\n",
						context_init[i].err_mes, err);
			kbase_context_term_partial(kctx, i);
			return NULL;
		}
	}

	return kctx;
}
KBASE_EXPORT_SYMBOL(kbase_create_context);

void kbase_destroy_context(struct kbase_context *kctx)
{
	struct kbase_device *kbdev;

	if (WARN_ON(!kctx))
		return;

	kbdev = kctx->kbdev;
	if (WARN_ON(!kbdev))
		return;

	/* Ensure the core is powered up for the destroy process
	 * A suspend won't happen here, because we're in a syscall
	 * from a userspace thread.
	 */
	kbase_pm_context_active(kbdev);

	kbase_mem_pool_group_mark_dying(&kctx->mem_pools);

	kbase_jd_zap_context(kctx);
	flush_workqueue(kctx->jctx.job_done_wq);

	kbase_context_term_partial(kctx, ARRAY_SIZE(context_init));

	kbase_pm_context_idle(kbdev);

#ifdef CONFIG_GR_IO_HISTORY
	gr_io_history_dump(kbdev);
	filp_close(kbdev->gr_io_file, NULL);
	EE("io history flush done");
#endif

#ifdef CONFIG_GR_REG_SNAPSHOT
	gr_reg_flush(kbdev);
	filp_close(kbdev->reg_dump_file, NULL);
	EE("reg snapshot flush done");
#endif

#ifdef CONFIG_GR_MEASURE_TIME
	k2_measure("context finish");
	k2_measure_flush();
#endif
}
KBASE_EXPORT_SYMBOL(kbase_destroy_context);
