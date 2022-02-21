/*
 *
 * (C) COPYRIGHT 2016, 2019 ARM Limited. All rights reserved.
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

#include "mali_kbase.h"

#include "mali_kbase_regs_history_debugfs.h"

#if defined(CONFIG_DEBUG_FS) && !defined(CONFIG_MALI_NO_MALI)

#include <linux/debugfs.h>

#ifdef CONFIG_GR_DEBUG_FS_VERBOSE
#include "tl/mali_kbase_tlstream.h"
#include "tl/mali_kbase_timeline_priv.h"

#ifndef PACKET_HEADER_SIZE
#define PACKET_HEADER_SIZE 8 /* bytes */  
#endif
#ifndef PACKET_NUMBER_SIZE
#define PACKET_NUMBER_SIZE 4 /* bytes */
#endif

#if KBASE_TRACE_ENABLE

#ifndef DEBUG_MESSAGE_SIZE
#define DEBUG_MESSAGE_SIZE 256
#endif

static const char *kbasep_trace_code_string[] = {
	/* IMPORTANT: USE OF SPECIAL #INCLUDE OF NON-STANDARD HEADER FILE
	 * THIS MUST BE USED AT THE START OF THE ARRAY */
#define KBASE_TRACE_CODE_MAKE_CODE(X) # X
#include "tl/mali_kbase_trace_defs.h"
#undef  KBASE_TRACE_CODE_MAKE_CODE
};

#endif	// KBASE_TRACE_ENABLE

/* Message ids of trace events that are recorded in the timeline stream. */
enum tl_msg_id_obj {
	KBASE_TL_NEW_CTX,
	KBASE_TL_NEW_GPU,
	KBASE_TL_NEW_LPU,
	KBASE_TL_NEW_ATOM,
	KBASE_TL_NEW_AS,
	KBASE_TL_DEL_CTX,
	KBASE_TL_DEL_ATOM,
	KBASE_TL_LIFELINK_LPU_GPU,
	KBASE_TL_LIFELINK_AS_GPU,
	KBASE_TL_RET_CTX_LPU,
	KBASE_TL_RET_ATOM_CTX,
	KBASE_TL_RET_ATOM_LPU,
	KBASE_TL_NRET_CTX_LPU,
	KBASE_TL_NRET_ATOM_CTX,
	KBASE_TL_NRET_ATOM_LPU,
	KBASE_TL_RET_AS_CTX,
	KBASE_TL_NRET_AS_CTX,
	KBASE_TL_RET_ATOM_AS,
	KBASE_TL_NRET_ATOM_AS,
	KBASE_TL_ATTRIB_ATOM_CONFIG,
	KBASE_TL_ATTRIB_ATOM_PRIORITY,
	KBASE_TL_ATTRIB_ATOM_STATE,
	KBASE_TL_ATTRIB_ATOM_PRIORITIZED,
	KBASE_TL_ATTRIB_ATOM_JIT,
	KBASE_TL_JIT_USEDPAGES,
	KBASE_TL_ATTRIB_ATOM_JITALLOCINFO,
	KBASE_TL_ATTRIB_ATOM_JITFREEINFO,
	KBASE_TL_ATTRIB_AS_CONFIG,
	KBASE_TL_EVENT_LPU_SOFTSTOP,
	KBASE_TL_EVENT_ATOM_SOFTSTOP_EX,
	KBASE_TL_EVENT_ATOM_SOFTSTOP_ISSUE,
	KBASE_TL_EVENT_ATOM_SOFTJOB_START,
	KBASE_TL_EVENT_ATOM_SOFTJOB_END,
	KBASE_JD_GPU_SOFT_RESET,
	KBASE_TL_KBASE_NEW_DEVICE,
	KBASE_TL_KBASE_DEVICE_PROGRAM_CSG,
	KBASE_TL_KBASE_DEVICE_DEPROGRAM_CSG,
	KBASE_TL_KBASE_NEW_CTX,
	KBASE_TL_KBASE_DEL_CTX,
	KBASE_TL_KBASE_NEW_KCPUQUEUE,
	KBASE_TL_KBASE_DEL_KCPUQUEUE,
	KBASE_TL_KBASE_KCPUQUEUE_ENQUEUE_FENCE_SIGNAL,
	KBASE_TL_KBASE_KCPUQUEUE_ENQUEUE_FENCE_WAIT,
	KBASE_TL_KBASE_ARRAY_BEGIN_KCPUQUEUE_ENQUEUE_CQS_WAIT,
	KBASE_TL_KBASE_ARRAY_ITEM_KCPUQUEUE_ENQUEUE_CQS_WAIT,
	KBASE_TL_KBASE_ARRAY_END_KCPUQUEUE_ENQUEUE_CQS_WAIT,
	KBASE_TL_KBASE_ARRAY_BEGIN_KCPUQUEUE_ENQUEUE_CQS_SET,
	KBASE_TL_KBASE_ARRAY_ITEM_KCPUQUEUE_ENQUEUE_CQS_SET,
	KBASE_TL_KBASE_ARRAY_END_KCPUQUEUE_ENQUEUE_CQS_SET,
	KBASE_TL_KBASE_ARRAY_BEGIN_KCPUQUEUE_ENQUEUE_DEBUGCOPY,
	KBASE_TL_KBASE_ARRAY_ITEM_KCPUQUEUE_ENQUEUE_DEBUGCOPY,
	KBASE_TL_KBASE_ARRAY_END_KCPUQUEUE_ENQUEUE_DEBUGCOPY,
	KBASE_TL_KBASE_KCPUQUEUE_ENQUEUE_MAP_IMPORT,
	KBASE_TL_KBASE_KCPUQUEUE_ENQUEUE_UNMAP_IMPORT,
	KBASE_TL_KBASE_KCPUQUEUE_ENQUEUE_UNMAP_IMPORT_FORCE,
	KBASE_TL_KBASE_ARRAY_BEGIN_KCPUQUEUE_ENQUEUE_JIT_ALLOC,
	KBASE_TL_KBASE_ARRAY_ITEM_KCPUQUEUE_ENQUEUE_JIT_ALLOC,
	KBASE_TL_KBASE_ARRAY_END_KCPUQUEUE_ENQUEUE_JIT_ALLOC,
	KBASE_TL_KBASE_ARRAY_BEGIN_KCPUQUEUE_ENQUEUE_JIT_FREE,
	KBASE_TL_KBASE_ARRAY_ITEM_KCPUQUEUE_ENQUEUE_JIT_FREE,
	KBASE_TL_KBASE_ARRAY_END_KCPUQUEUE_ENQUEUE_JIT_FREE,
	KBASE_TL_KBASE_KCPUQUEUE_EXECUTE_FENCE_SIGNAL_START,
	KBASE_TL_KBASE_KCPUQUEUE_EXECUTE_FENCE_SIGNAL_END,
	KBASE_TL_KBASE_KCPUQUEUE_EXECUTE_FENCE_WAIT_START,
	KBASE_TL_KBASE_KCPUQUEUE_EXECUTE_FENCE_WAIT_END,
	KBASE_TL_KBASE_KCPUQUEUE_EXECUTE_CQS_WAIT_START,
	KBASE_TL_KBASE_KCPUQUEUE_EXECUTE_CQS_WAIT_END,
	KBASE_TL_KBASE_KCPUQUEUE_EXECUTE_CQS_SET,
	KBASE_TL_KBASE_KCPUQUEUE_EXECUTE_DEBUGCOPY_START,
	KBASE_TL_KBASE_KCPUQUEUE_EXECUTE_DEBUGCOPY_END,
	KBASE_TL_KBASE_KCPUQUEUE_EXECUTE_MAP_IMPORT_START,
	KBASE_TL_KBASE_KCPUQUEUE_EXECUTE_MAP_IMPORT_END,
	KBASE_TL_KBASE_KCPUQUEUE_EXECUTE_UNMAP_IMPORT_START,
	KBASE_TL_KBASE_KCPUQUEUE_EXECUTE_UNMAP_IMPORT_END,
	KBASE_TL_KBASE_KCPUQUEUE_EXECUTE_UNMAP_IMPORT_FORCE_START,
	KBASE_TL_KBASE_KCPUQUEUE_EXECUTE_UNMAP_IMPORT_FORCE_END,
	KBASE_TL_KBASE_KCPUQUEUE_EXECUTE_JIT_ALLOC_START,
	KBASE_TL_KBASE_ARRAY_BEGIN_KCPUQUEUE_EXECUTE_JIT_ALLOC_END,
	KBASE_TL_KBASE_ARRAY_ITEM_KCPUQUEUE_EXECUTE_JIT_ALLOC_END,
	KBASE_TL_KBASE_ARRAY_END_KCPUQUEUE_EXECUTE_JIT_ALLOC_END,
	KBASE_TL_KBASE_KCPUQUEUE_EXECUTE_JIT_FREE_START,
	KBASE_TL_KBASE_ARRAY_BEGIN_KCPUQUEUE_EXECUTE_JIT_FREE_END,
	KBASE_TL_KBASE_ARRAY_ITEM_KCPUQUEUE_EXECUTE_JIT_FREE_END,
	KBASE_TL_KBASE_ARRAY_END_KCPUQUEUE_EXECUTE_JIT_FREE_END,
	KBASE_TL_KBASE_KCPUQUEUE_EXECUTE_ERRORBARRIER,
	KBASE_OBJ_MSG_COUNT,
};

enum tl_msg_id_aux {
	KBASE_AUX_PM_STATE,
	KBASE_AUX_PAGEFAULT,
	KBASE_AUX_PAGESALLOC,
	KBASE_AUX_DEVFREQ_TARGET,
	KBASE_AUX_PROTECTED_ENTER_START,
	KBASE_AUX_PROTECTED_ENTER_END,
	KBASE_AUX_PROTECTED_LEAVE_START,
	KBASE_AUX_PROTECTED_LEAVE_END, 
	KBASE_AUX_JIT_STATS,
	KBASE_AUX_EVENT_JOB_SLOT,
	KBASE_AUX_MSG_COUNT,
};

static const char *obj[] = {
	"KBASE_TL_NEW_CTX",
	"KBASE_TL_NEW_GPU",
	"KBASE_TL_NEW_LPU",
	"KBASE_TL_NEW_ATOM",
	"KBASE_TL_NEW_AS",
	"KBASE_TL_DEL_CTX",
	"KBASE_TL_DEL_ATOM",
	"KBASE_TL_LIFELINK_LPU_GPU",
	"KBASE_TL_LIFELINK_AS_GPU",
	"KBASE_TL_RET_CTX_LPU",
	"KBASE_TL_RET_ATOM_CTX",		// 10
	"KBASE_TL_RET_ATOM_LPU",
	"KBASE_TL_NRET_CTX_LPU",
	"KBASE_TL_NRET_ATOM_CTX",
	"KBASE_TL_NRET_ATOM_LPU",
	"KBASE_TL_RET_AS_CTX",
	"KBASE_TL_NRET_AS_CTX",
	"KBASE_TL_RET_ATOM_AS",
	"KBASE_TL_NRET_ATOM_AS", 
	"KBASE_TL_ATTRIB_ATOM_CONFIG",
	"KBASE_TL_ATTRIB_ATOM_PRIORITY",	 //20
	"KBASE_TL_ATTRIB_ATOM_STATE",
	"KBASE_TL_ATTRIB_ATOM_PRIORITIZED",
	"KBASE_TL_ATTRIB_ATOM_JIT",
	"KBASE_TL_JIT_USEDPAGES",
	"KBASE_TL_ATTRIB_ATOM_JITALLOCINFO",
	"KBASE_TL_ATTRIB_ATOM_JITFREEINFO",
	"KBASE_TL_ATTRIB_AS_CONFIG",
	"KBASE_TL_EVENT_LPU_SOFTSTOP",
	"KBASE_TL_EVENT_ATOM_SOFTSTOP_EX",
	"KBASE_TL_EVENT_ATOM_SOFTSTOP_ISSUE",	//30
	"KBASE_TL_EVENT_ATOM_SOFTJOB_START",
	"KBASE_TL_EVENT_ATOM_SOFTJOB_END",
	"KBASE_JD_GPU_SOFT_RESET",
	"KBASE_TL_KBASE_NEW_DEVICE",
	"KBASE_TL_KBASE_DEVICE_PROGRAM_CSG",
	"KBASE_TL_KBASE_DEVICE_DEPROGRAM_CSG",
	"KBASE_TL_KBASE_NEW_CTX",
	"KBASE_TL_KBASE_DEL_CTX",
	"KBASE_TL_KBASE_NEW_KCPUQUEUE",
	"KBASE_TL_KBASE_DEL_KCPUQUEUE", //40
	"KBASE_TL_KBASE_KCPUQUEUE_ENQUEUE_FENCE_SIGNAL",
	"KBASE_TL_KBASE_KCPUQUEUE_ENQUEUE_FENCE_WAIT",
	"KBASE_TL_KBASE_ARRAY_BEGIN_KCPUQUEUE_ENQUEUE_CQS_WAIT",
	"KBASE_TL_KBASE_ARRAY_ITEM_KCPUQUEUE_ENQUEUE_CQS_WAIT",
	"KBASE_TL_KBASE_ARRAY_END_KCPUQUEUE_ENQUEUE_CQS_WAIT",
	"KBASE_TL_KBASE_ARRAY_BEGIN_KCPUQUEUE_ENQUEUE_CQS_SET",
	"KBASE_TL_KBASE_ARRAY_ITEM_KCPUQUEUE_ENQUEUE_CQS_SET",
	"KBASE_TL_KBASE_ARRAY_END_KCPUQUEUE_ENQUEUE_CQS_SET",
	"KBASE_TL_KBASE_ARRAY_BEGIN_KCPUQUEUE_ENQUEUE_DEBUGCOPY",
	"KBASE_TL_KBASE_ARRAY_ITEM_KCPUQUEUE_ENQUEUE_DEBUGCOPY",
	"KBASE_TL_KBASE_ARRAY_END_KCPUQUEUE_ENQUEUE_DEBUGCOPY",
	"KBASE_TL_KBASE_KCPUQUEUE_ENQUEUE_MAP_IMPORT",
	"KBASE_TL_KBASE_KCPUQUEUE_ENQUEUE_UNMAP_IMPORT",
	"KBASE_TL_KBASE_KCPUQUEUE_ENQUEUE_UNMAP_IMPORT_FORCE",
	"KBASE_TL_KBASE_ARRAY_BEGIN_KCPUQUEUE_ENQUEUE_JIT_ALLOC",
	"KBASE_TL_KBASE_ARRAY_ITEM_KCPUQUEUE_ENQUEUE_JIT_ALLOC",
	"KBASE_TL_KBASE_ARRAY_END_KCPUQUEUE_ENQUEUE_JIT_ALLOC",
	"KBASE_TL_KBASE_ARRAY_BEGIN_KCPUQUEUE_ENQUEUE_JIT_FREE",
	"KBASE_TL_KBASE_ARRAY_ITEM_KCPUQUEUE_ENQUEUE_JIT_FREE",
	"KBASE_TL_KBASE_ARRAY_END_KCPUQUEUE_ENQUEUE_JIT_FREE",
	"KBASE_TL_KBASE_KCPUQUEUE_EXECUTE_FENCE_SIGNAL_START",
	"KBASE_TL_KBASE_KCPUQUEUE_EXECUTE_FENCE_SIGNAL_END",
	"KBASE_TL_KBASE_KCPUQUEUE_EXECUTE_FENCE_WAIT_START",
	"KBASE_TL_KBASE_KCPUQUEUE_EXECUTE_FENCE_WAIT_END",
	"KBASE_TL_KBASE_KCPUQUEUE_EXECUTE_CQS_WAIT_START",
	"KBASE_TL_KBASE_KCPUQUEUE_EXECUTE_CQS_WAIT_END",
	"KBASE_TL_KBASE_KCPUQUEUE_EXECUTE_CQS_SET",
	"KBASE_TL_KBASE_KCPUQUEUE_EXECUTE_DEBUGCOPY_START",
	"KBASE_TL_KBASE_KCPUQUEUE_EXECUTE_DEBUGCOPY_END", 
	"KBASE_TL_KBASE_KCPUQUEUE_EXECUTE_MAP_IMPORT_START",
	"KBASE_TL_KBASE_KCPUQUEUE_EXECUTE_MAP_IMPORT_END",
	"KBASE_TL_KBASE_KCPUQUEUE_EXECUTE_UNMAP_IMPORT_START",
	"KBASE_TL_KBASE_KCPUQUEUE_EXECUTE_UNMAP_IMPORT_END",
	"KBASE_TL_KBASE_KCPUQUEUE_EXECUTE_UNMAP_IMPORT_FORCE_START",
	"KBASE_TL_KBASE_KCPUQUEUE_EXECUTE_UNMAP_IMPORT_FORCE_END",
	"KBASE_TL_KBASE_KCPUQUEUE_EXECUTE_JIT_ALLOC_START",
	"KBASE_TL_KBASE_ARRAY_BEGIN_KCPUQUEUE_EXECUTE_JIT_ALLOC_END",
	"KBASE_TL_KBASE_ARRAY_ITEM_KCPUQUEUE_EXECUTE_JIT_ALLOC_END",
	"KBASE_TL_KBASE_ARRAY_END_KCPUQUEUE_EXECUTE_JIT_ALLOC_END",
	"KBASE_TL_KBASE_KCPUQUEUE_EXECUTE_JIT_FREE_START",
	"KBASE_TL_KBASE_ARRAY_BEGIN_KCPUQUEUE_EXECUTE_JIT_FREE_END",
	"KBASE_TL_KBASE_ARRAY_ITEM_KCPUQUEUE_EXECUTE_JIT_FREE_END",
	"KBASE_TL_KBASE_ARRAY_END_KCPUQUEUE_EXECUTE_JIT_FREE_END",
	"KBASE_TL_KBASE_KCPUQUEUE_EXECUTE_ERRORBARRIER",
	"KBASE_OBJ_MSG_COUNT"
};

static const char *aux[] = {
	"KBASE_AUX_PM_STATE",
	"KBASE_AUX_PAGEFAULT",
	"KBASE_AUX_PAGESALLOC",
	"KBASE_AUX_DEVFREQ_TARGET",
	"KBASE_AUX_PROTECTED_ENTER_START",
	"KBASE_AUX_PROTECTED_ENTER_END",
	"KBASE_AUX_PROTECTED_LEAVE_START",
	"KBASE_AUX_PROTECTED_LEAVE_END",
	"KBASE_AUX_JIT_STATS",
	"KBASE_AUX_EVENT_JOB_SLOT",
	"KBASE_AUX_MSG_COUNT"
};
#endif	// CONFIG_GR_DEBUG_FS_VERBOSE

static int regs_history_size_get(void *data, u64 *val)
{
	struct kbase_io_history *const h = data;

	*val = h->size;

	return 0;
}

static int regs_history_size_set(void *data, u64 val)
{
	struct kbase_io_history *const h = data;

	return kbase_io_history_resize(h, (u16)val);
}


DEFINE_SIMPLE_ATTRIBUTE(regs_history_size_fops,
		regs_history_size_get,
		regs_history_size_set,
		"%llu\n");


#ifdef CONFIG_GR_DEBUG_FS_VERBOSE
static char *jin_history_gpu_control(uint32_t reg, uint32_t value) {
	char *ret;

	/** switch ((uint32_t) offset & 0xFFF) { */
	switch (reg) {
		// jin: from "gpu/backend/mali_kbase_gpu_remap_jm.h"
		case CORE_FEATURES:
			ret = "CORE_FEATURES (RO)";	break;
		case JS_PRESENT:
			ret = "JS_PRESENT (RO)"; break;
		case LATEST_FLUSH:
			ret = "LATEST_FLUSH (RO)"; break;
		case PRFCNT_BASE_LO:
			ret = "PRFCNT_BASE_LO (RW)"; break;
		case PRFCNT_BASE_HI:
			ret = "PRFCNT_BASE_HI (RW)"; break;
		case PRFCNT_CONFIG:
			ret = "PRFCNT_CONFIG (RW)"; break;
		case PRFCNT_JM_EN:
			ret = "PRFCNT_JM_EN (RW)"; break;
		case PRFCNT_SHADER_EN:
			ret = "PRFCNT_SHADER_EN (RW)"; break;
		case PRFCNT_TILER_EN:
			ret = "PRFCNT_TILER_EN (RW)"; break;
		case PRFCNT_MMU_L2_EN:
			ret = "PRFCNT_MMU_L2_EN (RW)"; break;
		case JS0_FEATURES:
			ret = "JS0_FEATURES (RO)"; break;
		case JS1_FEATURES:
			ret = "JS1_FEATURES (RO)"; break;
		case JS2_FEATURES:
			ret = "JS2_FEATURES (RO)"; break;
		case JS3_FEATURES:
			ret = "JS3_FEATURES (RO)"; break;
		case JS4_FEATURES:
			ret = "JS4_FEATURES (RO)"; break;
		case JS5_FEATURES:
			ret = "JS5_FEATURES (RO)"; break;
		case JS6_FEATURES:
			ret = "JS6_FEATURES (RO)"; break;
		case JS7_FEATURES:
			ret = "JS7_FEATURES (RO)"; break;
		case JS8_FEATURES:
			ret = "JS8_FEATURES (RO)"; break;
		case JS9_FEATURES:
			ret = "JS9_FEATURES (RO)"; break;
		case JS10_FEATURES:
			ret = "JS10_FEATURES (RO)"; break;
		case JS11_FEATURES:
			ret = "JS11_FEATURES (RO)"; break;
		case JS12_FEATURES:
			ret = "JS12_FEATURES (RO)"; break;
		case JS13_FEATURES:
			ret = "JS13_FEATURES (RO)"; break;
		case JS14_FEATURES:
			ret = "JS14_FEATURES (RO)"; break;
		case JS15_FEATURES:
			ret = "JS15_FEATURES (RO)"; break;
		case JM_CONFIG:
			ret = "JM_CONFIG (RW)"; break;

			// jin: from "gpu/mali_kbase_gpu_remap.h"
		case GPU_ID:
			ret = "GPU_ID (RO)"; break;
		case L2_FEATURES:
			ret = "L2_FEATURES (RO)"; break;
		case TILER_FEATURES:
			ret = "TILER_FEATURES (RO)"; break;
		case MEM_FEATURES:
			ret = "MEM_FEATURES (RO)"; break;
		case MMU_FEATURES:
			ret = "MMU_FEATURES (RO)"; break;
		case AS_PRESENT:
			ret = "AS_PRESENT (RO)"; break;
		case GPU_IRQ_RAWSTAT:
			ret = "GPU_IRQ_RAWSTAT (RW)"; break;
		case GPU_IRQ_CLEAR:
			ret = "GPU_IRQ_CLEAR (WO)"; break;
		case GPU_IRQ_MASK:
			ret = "GPU_IRQ_MASK (RW)"; break;
		case GPU_IRQ_STATUS:
			ret = "GPU_IRQ_STATUS (RO)"; break;
		case GPU_COMMAND:
			switch (value) {
				case GPU_COMMAND_NOP:
					ret = "GPU_COMMAND (WO) | GPU_COMMAND_NOP"; break;
				case GPU_COMMAND_SOFT_RESET:
					ret = "GPU_COMMAND (WO) | GPU_COMMAND_SOFT_RESET"; break;
				case GPU_COMMAND_HARD_RESET:
					ret = "GPU_COMMAND (WO) | GPU_COMMAND_HARD_RESET"; break;
				case GPU_COMMAND_PRFCNT_CLEAR:
					ret = "GPU_COMMAND (WO) | GPU_COMMAND_PRFCNT_CLEAR"; break;
				case GPU_COMMAND_PRFCNT_SAMPLE:
					ret = "GPU_COMMAND (WO) | GPU_COMMAND_PRFCNT_SAMPLE"; break;
				case GPU_COMMAND_CYCLE_COUNT_START:
					ret = "GPU_COMMAND (WO) | GPU_COMMAND_CYCLE_COUNT_START"; break;
				case GPU_COMMAND_CYCLE_COUNT_STOP:
					ret = "GPU_COMMAND (WO) | GPU_COMMAND_CYCLE_COUNT_STOP"; break;
				case GPU_COMMAND_CLEAN_CACHES:
					ret = "GPU_COMMAND (WO) | GPU_COMMAND_CLEAN_CACHES"; break;
				case GPU_COMMAND_CLEAN_INV_CACHES:
					ret = "GPU_COMMAND (WO) | GPU_COMMAND_CLEAN_INV_CACHES"; break;
				case GPU_COMMAND_SET_PROTECTED_MODE:
					ret = "GPU_COMMAND (WO) | GPU_COMMAND_SET_PROTECTED_MODE"; break; 
				default:
					ret = "GPU_COMMAND (WO) | Unkown GPU_COMMAND"; 
			}
			break;
			/** ret = "GPU_COMMAND (WO)"; break; */
		case GPU_STATUS:	// 0x34 (RO)
			ret = "GPU_STATUS (RO)"; break;
			// GPU_DBGEN

		case GPU_FAULTSTATUS:
			ret = "GPU_FAULTSTATUS (RO)"; break;
		case GPU_FAULTADDRESS_LO:
			ret = "GPU_FAULTADDRESS_LO (RO)"; break;
		case GPU_FAULTADDRESS_HI:
			ret = "GPU_FAULTADDRESS_HI (RO)"; break;
		case L2_CONFIG:
			ret = "L2_CONFIG (RW)"; break;
			// GROUPS_L2_COHERENT
			// SUPER_L2_COHERENT

		case PWR_KEY:
			ret = "PWR_KEY (WO)"; break;
		case PWR_OVERRIDE0:
			ret = "PWR_OVERRIDE0 (RW)"; break;
		case PWR_OVERRIDE1:
			ret = "PWR_OVERRIDE1 (RW)"; break;

		case CYCLE_COUNT_LO:
			ret = "CYCLE_COUNT_LO (RO)"; break;
		case CYCLE_COUNT_HI:
			ret = "CYCLE_COUNT_HI (RO)"; break;
		case TIMESTAMP_LO:
			ret = "TIMESTAMP_LO (RO)"; break;
		case TIMESTAMP_HI:
			ret = "TIMESTAMP_HI (RO)"; break;

		case THREAD_MAX_THREADS:
			ret = "THREAD_MAX_THREAD (RO)"; break;
		case THREAD_MAX_WORKGROUP_SIZE:
			ret = "THREAD_MAX_WORKGROUP_SIZE (RO)"; break;
		case THREAD_MAX_BARRIER_SIZE:
			ret = "THREAD_MAX_BARRIER_SIZE (RO)"; break;
		case THREAD_FEATURES:
			ret = "THREAD_FEATURES (RO)"; break;
		case THREAD_TLS_ALLOC:
			ret = "THREAD_TLS_ALLOC (RO)"; break;

		case TEXTURE_FEATURES_0:
			ret = "TEXTURE_FEATURES_0 (RO)"; break;
		case TEXTURE_FEATURES_1:
			ret = "TEXTURE_FEATURES_1 (RO)"; break;
		case TEXTURE_FEATURES_2:
			ret = "TEXTURE_FEATURES_2 (RO)"; break;
		case TEXTURE_FEATURES_3:
			ret = "TEXTURE_FEATURES_3 (RO)"; break;

		case SHADER_PRESENT_LO:
			ret = "SHADER_PRESENT_LO (RO)"; break;
		case SHADER_PRESENT_HI:
			ret = "SHADER_PRESENT_HI (RO)"; break;

		case TILER_PRESENT_LO:
			ret = "TILER_PRESENT_LO (RO)"; break;
		case TILER_PRESENT_HI:
			ret = "TILER_PRESENT_HI (RO)"; break;

		case L2_PRESENT_LO:
			ret = "L2_PRESENT_LO (RO)"; break;
		case L2_PRESENT_HI:	
			ret = "L2_PRESENT_HI (RO)"; break;

		case STACK_PRESENT_LO:
			ret = "STACK_PRESENT_LO (RO)"; break;
		case STACK_PRESENT_HI:
			ret = "STACK_PRESENT_HI (RO)"; break;

		case SHADER_READY_LO:
			ret = "SHADER_READY_LO (RO)"; break;
		case SHADER_READY_HI:
			ret = "SHADER_READY_HI (RO)"; break;

		case TILER_READY_LO:
			ret = "TILER_READY_LO (RO)"; break;
		case TILER_READY_HI:	// 0x34 (RO)
			ret = "TILER_READY_HI (RO)"; break;

		case L2_READY_LO:
			ret = "L2_READY_LO (RO)"; break;
		case L2_READY_HI:
			ret = "L2_READY_HI (RO)"; break;

		case STACK_READY_LO:
			ret = "STACK_READY_LO (RO)"; break;
		case STACK_READY_HI:
			ret = "STACK_READY_HI (RO)"; break;

		case SHADER_PWRON_LO:
			ret = "SHADER_PWRON_LO (WO)"; break;
		case SHADER_PWRON_HI:
			ret = "SHADER_PWRON_HI (WO)"; break;

		case TILER_PWRON_LO:
			ret = "TILER_PWRON_LO (WO)"; break;
		case TILER_PWRON_HI:
			ret = "TILER_PWRON_HI (WO)"; break;

		case L2_PWRON_LO:
			ret = "L2_PWRON_LO (WO)"; break;
		case L2_PWRON_HI:
			ret = "L2_PWRON_HI (WO)"; break;

		case STACK_PWRON_LO:
			ret = "STACK_PWRON_LO (RO)"; break;
		case STACK_PWRON_HI:
			ret = "STACK_PWRON_HI (RO)"; break;

		case SHADER_PWROFF_LO:
			ret = "SHADER_PWROFF_LO (WO)"; break;
		case SHADER_PWROFF_HI:
			ret = "SHADER_PWROFF_HI (WO)"; break;

		case TILER_PWROFF_LO:
			ret = "TILER_PWROFF_LO (RO)"; break;
		case TILER_PWROFF_HI:
			ret = "TILER_PWROFF_HI (RO)"; break;

		case L2_PWROFF_LO:
			ret = "L2_PWROFF_LO (WO)"; break;
		case L2_PWROFF_HI:
			ret = "L2_PWROFF_HI (WO)"; break;

		case STACK_PWROFF_LO:
			ret = "STACK_PWROFF_LO (RO)"; break;
		case STACK_PWROFF_HI:
			ret = "STACK_PWROFF_HI (RO)"; break;

		case SHADER_PWRTRANS_LO:
			ret = "SHADER_PWRTRANS_LO (RO)"; break;
		case SHADER_PWRTRANS_HI:
			ret = "SHADER_PWRTRANS_HI (RO)"; break;

		case TILER_PWRTRANS_LO:
			ret = "TILER_PWRTRANS_LO (RO)"; break;
		case TILER_PWRTRANS_HI:
			ret = "TILER_PWRTRANS_HI (RO)"; break;

		case L2_PWRTRANS_LO:
			ret = "L2_PWRTRANS_LO (RO)"; break;
		case L2_PWRTRANS_HI:
			ret = "L2_PWRTRANS_HI (RO)"; break;

		case STACK_PWRTRANS_LO:
			ret = "STACK_PWRTRANS_LO (RO)"; break;
		case STACK_PWRTRANS_HI:
			ret = "STACK_PWRTRANS_LO (RO)"; break;

		case SHADER_PWRACTIVE_LO:
			ret = "SHADER_PWRACTIVE_LO (RO)"; break;
		case SHADER_PWRACTIVE_HI:
			ret = "SHADER_PWRACTIVE_HI (RO)"; break;

		case TILER_PWRACTIVE_LO:
			ret = "TILER_PWRACTIVE_LO (RO)"; break;
		case TILER_PWRACTIVE_HI:
			ret = "TILER_PWRACTIVE_HI (RO)"; break;

		case L2_PWRACTIVE_LO:
			ret = "L2_PWRACTIVE_LO (RO)"; break;
		case L2_PWRACTIVE_HI:
			ret = "L2_PWRACTIVE_HI (RO)"; break;

		case COHERENCY_FEATURES:
			ret = "COHERENCY_FEATURES (RO)"; break;
		case COHERENCY_ENABLE:
			ret = "COHERENCY_ENABLE (RW)"; break;

		case SHADER_CONFIG:
			ret = "SHADER_CONFIG (RW)"; break;
		case TILER_CONFIG:
			ret = "TILER_CONFIG (RW)"; break;
		case L2_MMU_CONFIG:
			ret = "L2_MMU_CONFIG (RW)"; break;

		default:
			ret = "[UNKOWN] not classified";
	}

	return ret;
}

static char *jin_history_job_control(uint32_t reg, char *job_cmd_reg, uint32_t value) {
	char *ret = "";
	uint32_t job_slot;

	switch (reg) {
		// jin: from "gpu/mali_kbase_gpu_remap.h"
		case JOB_IRQ_RAWSTAT:
			ret = "JOB_IRQ_RAWSTAT"; break;
		case JOB_IRQ_CLEAR:
			ret = "JOB_IRQ_CLEAR"; break;
		case JOB_IRQ_MASK:
			ret = "JOB_IRQ_MASK"; break;
		case JOB_IRQ_STATUS:
			ret = "JOB_IRQ_STATUS"; break;

			// jin: from "gpu/backend/mali_kbase_gpu_remap_jm.h"
		case JOB_IRQ_JS_STATE:
			ret = "JOB_IRQ_JS_STATE"; break;
		case JOB_IRQ_THROTTLE:
			ret = "JOB_IRQ_THROTTLE"; break;

		default:
			/**          uint32_t job_slot; */
			job_slot = reg & 0xF80;
			/** EE("job slot : 0x%08x", job_slot); */
			switch(job_slot) {
				/** switch(reg & 0xFF0) { */
				case JOB_SLOT0:
					ret = "JOB_SLOT0"; break;
				case JOB_SLOT1:
					ret = "JOB_SLOT1"; break;
				case JOB_SLOT2:
					ret = "JOB_SLOT2"; break;
				case JOB_SLOT3:
					ret = "JOB_SLOT3"; break;
				case JOB_SLOT4:
					ret = "JOB_SLOT4"; break;
				case JOB_SLOT5:
					ret = "JOB_SLOT5"; break;
				case JOB_SLOT6:
					ret = "JOB_SLOT6"; break;
				case JOB_SLOT7:
					ret = "JOB_SLOT7"; break;
				default:
					ret = "[UNKOWN] JOB_SLOT";
			}
			/** EE("job_cmd_reg = 0x%08x", reg & ~job_slot); */
			switch(reg & ~job_slot) {
				case JS_HEAD_LO:
					strcpy(job_cmd_reg, "JS_HEAD_LO (RO)"); break;
				case JS_HEAD_HI:
					strcpy(job_cmd_reg, "JS_HEAD_HI (RO)"); break;
				case JS_TAIL_LO:
					strcpy(job_cmd_reg, "JS_TAIL_LO (RO)"); break;
				case JS_TAIL_HI:
					strcpy(job_cmd_reg, "JS_TAIL_HI (RO)"); break;
				case JS_AFFINITY_LO:
					strcpy(job_cmd_reg, "JS_AFFINITY_LO (RO)"); break;
				case JS_AFFINITY_HI:
					strcpy(job_cmd_reg, "JS_AFFINITY_HI (RO)"); break;				
				case JS_CONFIG:
					strcpy(job_cmd_reg, "JS_CONFIG (RO)"); break;
				case JS_XAFFINITY:
					strcpy(job_cmd_reg, "JS_XAFFINITY"); break;

				case JS_COMMAND:
					switch (value) {
						case JS_COMMAND_NOP:	
							strcpy(job_cmd_reg, "JS_COMMAND (WO) | JS_COMMAND_NOP"); break;
						case JS_COMMAND_START:	
							strcpy(job_cmd_reg, "JS_COMMAND (WO) | JS_COMMAND_START"); break;
						case JS_COMMAND_SOFT_STOP:	
							strcpy(job_cmd_reg, "JS_COMMAND (WO) | JS_COMMAND_SOFT_STOP"); break;
						case JS_COMMAND_HARD_STOP:	
							strcpy(job_cmd_reg, "JS_COMMAND (WO) | JS_COMMAND_HARD_STOP"); break;
						case JS_COMMAND_SOFT_STOP_0:	
							strcpy(job_cmd_reg, "JS_COMMAND (WO) | JS_COMMAND_SOFT_STOP_0"); break;
						case JS_COMMAND_HARD_STOP_0:	
							strcpy(job_cmd_reg, "JS_COMMAND (WO) | JS_COMMAND_HARD_STOP_0"); break;
						case JS_COMMAND_SOFT_STOP_1:	
							strcpy(job_cmd_reg, "JS_COMMAND (WO) | JS_COMMAND_SOFT_STOP_1"); break;
						case JS_COMMAND_HARD_STOP_1:	
							strcpy(job_cmd_reg, "JS_COMMAND (WO) | JS_COMMAND_HARD_STOP_1"); break;
						default:
							strcpy(job_cmd_reg, "JS_COMMAND (WO) | Unknown JS_COMMAND");
					}
					break;
					/** strcpy(job_cmd_reg, "JS_COMMAND (WO)"); break; */
				case JS_STATUS:
					strcpy(job_cmd_reg, "JS_STATUS (RO)"); break;

				case JS_HEAD_NEXT_LO:
					strcpy(job_cmd_reg, "JS_HEAD_NEXT_LO (RW)"); break;
				case JS_HEAD_NEXT_HI:
					strcpy(job_cmd_reg, "JS_HEAD_NEXT_HI (RW)"); break;

				case JS_AFFINITY_NEXT_LO:
					strcpy(job_cmd_reg, "JS_AFFINITY_NEXT_LO (RW)"); break;
				case JS_AFFINITY_NEXT_HI:
					strcpy(job_cmd_reg, "JS_AFFINITY_NEXT_HI (RW)"); break;
				case JS_CONFIG_NEXT:
					strcpy(job_cmd_reg, "JS_CONFIG_NEXT (RW)"); break;
				case JS_XAFFINITY_NEXT:
					strcpy(job_cmd_reg, "JS_XAFFINITY_NEXT (RW)"); break;

				case JS_COMMAND_NEXT:
					switch (value) {
						case JS_COMMAND_NOP:	
							strcpy(job_cmd_reg, "JS_COMMAND_NEXT (WO) | JS_COMMAND_NOP"); break;
						case JS_COMMAND_START:	
							strcpy(job_cmd_reg, "JS_COMMAND_NEXT (WO) | JS_COMMAND_START"); break;
						case JS_COMMAND_SOFT_STOP:	
							strcpy(job_cmd_reg, "JS_COMMAND_NEXT (WO) | JS_COMMAND_SOFT_STOP"); break;
						case JS_COMMAND_HARD_STOP:	
							strcpy(job_cmd_reg, "JS_COMMAND_NEXT (WO) | JS_COMMAND_HARD_STOP"); break;
						case JS_COMMAND_SOFT_STOP_0:	
							strcpy(job_cmd_reg, "JS_COMMAND_NEXT (WO) | JS_COMMAND_SOFT_STOP_0"); break;
						case JS_COMMAND_HARD_STOP_0:	
							strcpy(job_cmd_reg, "JS_COMMAND_NEXT (WO) | JS_COMMAND_HARD_STOP_0"); break;
						case JS_COMMAND_SOFT_STOP_1:	
							strcpy(job_cmd_reg, "JS_COMMAND_NEXT (WO) | JS_COMMAND_SOFT_STOP_1"); break;
						case JS_COMMAND_HARD_STOP_1:	
							strcpy(job_cmd_reg, "JS_COMMAND_NEXT (WO) | JS_COMMAND_HARD_STOP_1"); break;
						default:
							strcpy(job_cmd_reg, "JS_COMMAND_NEXT (WO) | Unknown JS_COMMAND"); 
					}
					break;
					/** strcpy(job_cmd_reg, "JS_COMMAND_NEXT (RW)"); break; */
				case JS_FLUSH_ID_NEXT:
					strcpy(job_cmd_reg, "JS_FLUSH_ID_NEXT (RW)"); break;

				default :
					strcpy(job_cmd_reg, "[UNKOWN_JOB_CMD_REG]");
			}
		}
	return ret;
}

static char *jin_history_memory_management(uint32_t reg, char *mmu_cmd_reg, uint32_t value) {
	char *ret = "";
	uint32_t mmu_slot;

	switch (reg) {
		case MMU_IRQ_RAWSTAT:
			ret = "MMU_IRQ_RAWSTAT (RW)"; break;
		case MMU_IRQ_CLEAR:
			ret = "MMU_IRQ_CLEAR (WO)"; break;
		case MMU_IRQ_MASK:
			ret = "MMU_IRQ_MASK (RW)"; break;
		case MMU_IRQ_STATUS:
			ret = "MMU_IRQ_STATUS (RO)"; break;
		default:
			mmu_slot = reg & 0xFC0;
			switch (mmu_slot) {
				case MMU_AS0:
					ret = "MMU_AS0"; break;
				case MMU_AS1:
					ret = "MMU_AS1"; break;
				case MMU_AS2:
					ret = "MMU_AS2"; break;
				case MMU_AS3:
					ret = "MMU_AS3"; break;
				case MMU_AS4:
					ret = "MMU_AS4"; break;
				case MMU_AS5:
					ret = "MMU_AS5"; break;
				case MMU_AS6:
					ret = "MMU_AS6"; break;
				case MMU_AS7:
					ret = "MMU_AS7"; break;
				case MMU_AS8:
					ret = "MMU_AS8"; break;
				case MMU_AS9:
					ret = "MMU_AS9"; break;
				case MMU_AS10:
					ret = "MMU_AS10"; break;
				case MMU_AS11:
					ret = "MMU_AS11"; break;
				case MMU_AS12:
					ret = "MMU_AS12"; break;
				case MMU_AS13:
					ret = "MMU_AS13"; break;
				case MMU_AS14:
					ret = "MMU_AS14"; break;
				case MMU_AS15:
					ret = "MMU_AS15"; break;
				default:
					ret = "[UNKOWN] not classified";
			}

			switch (reg & ~mmu_slot) {
				case AS_TRANSTAB_LO:
					strcpy(mmu_cmd_reg, "AS_TRANSTAB_LO (RW)"); break;
				case AS_TRANSTAB_HI:
					strcpy(mmu_cmd_reg, "AS_TRANSTAB_HI (RW)"); break;
				case AS_MEMATTR_LO:
					strcpy(mmu_cmd_reg, "AS_MEMATTR_LO (RW)"); break;
				case AS_MEMATTR_HI:
					strcpy(mmu_cmd_reg, "AS_MEMATTR_HI (RW)"); break;
				case AS_LOCKADDR_LO:
					strcpy(mmu_cmd_reg, "AS_LOCKADDR_LO (RW)"); break;
				case AS_LOCKADDR_HI:
					strcpy(mmu_cmd_reg, "AS_LOCKADDR_HI (RW)"); break;
				case AS_COMMAND:
					switch(value) {
						case AS_COMMAND_NOP:
							strcpy(mmu_cmd_reg, "AS_COMMAND (WO) | AS_COMMAND_NOP"); break;
						case AS_COMMAND_UPDATE:
							strcpy(mmu_cmd_reg, "AS_COMMAND (WO) | AS_COMMAND_UPDATE"); break;
						case AS_COMMAND_LOCK:
							strcpy(mmu_cmd_reg, "AS_COMMAND (WO) | AS_COMMAND_LOCK"); break;
						case AS_COMMAND_UNLOCK:
							strcpy(mmu_cmd_reg, "AS_COMMAND (WO) | AS_COMMAND_UNLOCK"); break;
							/** case AS_COMMAND_FLUSH: */	// deprecated - only for use with T60X
							/** strcpy(mmu_cmd_reg, "AS_COMMAND (WO) | AS_COMMAND_FLUSH"); break; */
						case AS_COMMAND_FLUSH_PT:
							strcpy(mmu_cmd_reg, "AS_COMMAND (WO) | AS_COMMAND_FLUSH_PT"); break;
						case AS_COMMAND_FLUSH_MEM:
							strcpy(mmu_cmd_reg, "AS_COMMAND (WO) | AS_COMMAND_FLUSH_MEM"); break;
						default :
							strcpy(mmu_cmd_reg, "AS_COMMAND (WO) | Unkonwn AS_COMMAND"); 
					}
					break;
					/** strcpy(mmu_cmd_reg, "AS_COMMAND (WO)"); break; */
				case AS_FAULTSTATUS:
					strcpy(mmu_cmd_reg, "AS_FAULTSTATUS (RO)"); break;
				case AS_FAULTADDRESS_LO:
					strcpy(mmu_cmd_reg, "AS_FAULTADDRESS_LO (RO)"); break;
				case AS_FAULTADDRESS_HI:
					strcpy(mmu_cmd_reg, "AS_FAULTADDRESS_HI (HI)"); break;
				case AS_STATUS:
					strcpy(mmu_cmd_reg, "AS_STATUS (RO)"); break;

				case AS_TRANSCFG_LO:
					strcpy(mmu_cmd_reg, "AS_TRANSCFG_LO"); break;
				case AS_TRANSCFG_HI:
					strcpy(mmu_cmd_reg, "AS_TRANSCFG_HI"); break;
				case AS_FAULTEXTRA_LO:
					strcpy(mmu_cmd_reg, "AS_FAULTEXTRA_LO"); break;
				case AS_FAULTEXTRA_HI:
					strcpy(mmu_cmd_reg, "AS_FAULTEXTRA_HI"); break;
				default:
					strcpy(mmu_cmd_reg, "[UNKNOWN MMU_CMD_REG]");
			}
	}
	return ret;
}



static void regs_history_with_info(struct seq_file *sfile, const struct kbase_io_history *h)
{
	u16 i;
	size_t iters;

	iters = (h->size > h->count) ? h->count : h->size;
	for (i = 0; i < iters; ++i) {
		struct kbase_io_access *io =
			&h->buf[(h->count - iters + i) % h->size];

		char const access = (io->addr & 1) ? 'W' : 'R';
		uintptr_t offset = io->addr & ~0x1;
		char *type = "";
		char *tmp = "";
		char cmd_reg[50] = "";

		seq_printf(sfile, "%5llu\t%5s\t%6d\t%4d: %c ", io->timestamp, "IO", io->thread_id, i, access);
		switch ((offset >> 12) & 0xF) {
			case 0x0:
				type = "GPU_CTRL";
				tmp = jin_history_gpu_control(offset & 0xFFF, io->value);
				seq_printf(sfile, "reg 0x%08x val %08x [ %10s | %s ]\n", (u32)(io->addr & ~0x1), io->value, type, tmp); 
				break;
			case 0x1:
				type = "JOB_CTRL";
				tmp = jin_history_job_control(offset & 0xFFF, cmd_reg, io->value);
				seq_printf(sfile, "reg 0x%08x val %08x [ %10s | %10s | %s]\n", (u32)(io->addr & ~0x1), io->value, type, tmp, cmd_reg);
				break;
			case 0x2:
				type = "MEM_MGMT";
				tmp = jin_history_memory_management(offset & 0xFFF, cmd_reg, io->value);
				seq_printf(sfile, "reg 0x%08x val %08x [ %10s | %10s | %s]\n", (u32)(io->addr & ~0x1), io->value, type, tmp, cmd_reg);
				break;
			default:
				type = "UNKNOWN";
				tmp = "UNKNWON";
		}
	}
}
#endif	// CONFIG_GR_DEBUG_FS_VERBOSE

/**
 * regs_history_show - show callback for the register access history file.
 *
 * @sfile: The debugfs entry
 * @data: Data associated with the entry
 *
 * This function is called to dump all recent accesses to the GPU registers.
 *
 * @return 0 if successfully prints data in debugfs entry file, failure
 * otherwise
 */
static int regs_history_show(struct seq_file *sfile, void *data)
{
	struct kbase_io_history *const h = sfile->private;
#ifndef CONFIG_GR_DEBUG_FS_VERBOSE
	u16 i;
#endif
	size_t iters;
	unsigned long flags;

	if (!h->enabled) {
		seq_puts(sfile, "The register access history is disabled\n");
		goto out;
	}

	spin_lock_irqsave(&h->lock, flags);

#ifdef CONFIG_GR_DEBUG_FS_VERBOSE
	iters = (h->size > h->count) ? h->count : h->size;
	regs_history_with_info(sfile, h);
#else	// jin: original implementation
	iters = (h->size > h->count) ? h->count : h->size;
	seq_printf(sfile, "Last %zu register accesses of %zu total:\n", iters,
			h->count);

	for (i = 0; i < iters; ++i) {
		struct kbase_io_access *io =
			&h->buf[(h->count - iters + i) % h->size];

		char const access = (io->addr & 1) ? 'w' : 'r';

			 seq_printf(sfile, "%6i: %c: reg 0x%016lx val %08x\n", i, access,
		(unsigned long)(io->addr & ~0x1), io->value);
	}
#endif

	spin_unlock_irqrestore(&h->lock, flags);

out:
	return 0;
}


/**
 * regs_history_open - open operation for regs_history debugfs file
 *
 * @in: &struct inode pointer
 * @file: &struct file pointer
 *
 * @return file descriptor
 */
static int regs_history_open(struct inode *in, struct file *file)
{
	return single_open(file, &regs_history_show, in->i_private);
}


static const struct file_operations regs_history_fops = {
	.owner = THIS_MODULE,
	.open = &regs_history_open,
	.read = seq_read,
	.llseek = seq_lseek,
	.release = single_release,
};

#ifdef CONFIG_GR_DEBUG_FS_VERBOSE

static void timeline_history_obj(struct seq_file *sfile, struct kbase_tlstream *stream)
{
	unsigned long flags;
	unsigned int wb_idx_raw;
	unsigned int wb_idx;
	size_t wb_size;

	char *buf;
	size_t idx;
	u32 type, tid;
	u64 ts;
	size_t i;

	spin_lock_irqsave(&stream->lock, flags);

	wb_idx_raw 	= atomic_read(&stream->wbi);
	wb_idx		= wb_idx_raw % PACKET_COUNT;

	for (i = 0; i <= wb_idx; i++) {
		wb_size = atomic_read(&stream->buffer[i].size);

		if (stream->numbered)
			idx = PACKET_HEADER_SIZE + PACKET_NUMBER_SIZE;
		else
			idx = PACKET_HEADER_SIZE;

		buf = &stream->buffer[i].data[0];
   /**      seq_printf(sfile, "[jin] wb_idx: %lu\n", i); */
		/** seq_printf(sfile, "[jin] wb_size : %lu, idx: %lu\n", wb_size, idx); */

		// TODO clean printing format 
		while (idx < wb_size) {
			type = *(u32*) &buf[idx];
			idx += sizeof(u32);
			ts = *(u64*) &buf[idx];
			idx += sizeof(u64);
			tid = *(u32*) &buf[idx];
			idx += sizeof(u32);

			seq_printf(sfile, "%5llu\t%5s\t%6d\t%30s\t", ts, "TL", tid, obj[type]);
			switch (type) {
				case KBASE_TL_NEW_CTX:
					seq_printf(sfile, "ctx: 0x%llx, ctx_nr: %u, ugid: %u\n", \
							*(u64*)&buf[idx], *(u32*)&buf[idx + 8], *(u32*)&buf[idx + 12]);
					idx += 16;
					break;
				case KBASE_TL_NEW_GPU:
					seq_printf(sfile, "gpu: 0x%llx, gpu_id: %u, core_count: %u\n", \
							*(u64*)&buf[idx], *(u32*)&buf[idx + 8], *(u32*)&buf[idx + 12]);
					idx += 16;
					break;
				case KBASE_TL_NEW_LPU:
					seq_printf(sfile, "lpu: 0x%llx, lpu_nr: %u, lpu_fn: %u\n", \
							*(u64*)&buf[idx], *(u32*)&buf[idx + 8], *(u32*)&buf[idx + 12]);
					idx += 16;
					break;
				case KBASE_TL_NEW_ATOM:
					seq_printf(sfile, "atom: 0%llx, atom_nr: %u\n", \
							*(u64*)&buf[idx], *(u32*)&buf[idx + 8]);
					idx += 12;
					break;
				case KBASE_TL_NEW_AS:
					seq_printf(sfile, "address_space: 0x%llx, as_nr: %u]n", \
							*(u64*)&buf[idx], *(u32*)&buf[idx + 8]);
					idx += 12;
					break;
				case KBASE_TL_DEL_CTX:
					seq_printf(sfile, "ctx: 0x%llx\n", \
							*(u64*)&buf[idx]);
					idx += 8;
					break;
				case KBASE_TL_DEL_ATOM:
					seq_printf(sfile, "atom: 0x%llx\n", \
							*(u64*)&buf[idx]);
					idx += 8;
					break;
				case KBASE_TL_LIFELINK_LPU_GPU:
					seq_printf(sfile, "lpu: 0x%llx, gpu: 0x%llx\n", \
							*(u64*)&buf[idx], *(u64*)&buf[idx + 8]);
					idx += 16;
					break;
				case KBASE_TL_LIFELINK_AS_GPU:
					seq_printf(sfile, "address_space: 0x%llx, gpu: 0x%llx\n", \
							*(u64*)&buf[idx], *(u64*)&buf[idx + 8]);
					idx += 16;
					break;
				case KBASE_TL_RET_CTX_LPU:
					seq_printf(sfile, "ctx: 0x%llx, lpu: 0x%llx\n", \
							*(u64*)&buf[idx], *(u64*)&buf[idx + 8]);
					idx += 16;
					break;
				case KBASE_TL_RET_ATOM_CTX:
					seq_printf(sfile, "atom: 0x%llx, ctx: 0x%llx\n", \
							*(u64*)&buf[idx], *(u64*)&buf[idx + 8]);
					idx += 16;
					break;
				case KBASE_TL_RET_ATOM_LPU:
					seq_printf(sfile, "atom: 0x%llx, lpu: 0x%llx\n", \
							*(u64*)&buf[idx], *(u64*)&buf[idx + 8]);
					/** seq_printf(sfile, "[%llu] %30s\t\n", ts, obj[type]);	// TODO: figure out format */
					// jin: how to print? cannot know exact size of this packet
					/**          seq_printf(sfile, "[jin][%llu] %30s\t atom: %lu, lpu: %lu", \ */
					/** ts, obj[type], buf[idx], buf[idx + 8]); */
					// jin: msg_size = 47
					// jin: sizeof(attrib_match_list) = 14
					/** idx += 47; */
					idx += 35;
					break;
				case KBASE_TL_NRET_CTX_LPU:
					seq_printf(sfile, "ctx: 0x%llx, lpu: 0x%llx\n", \
							*(u64*)&buf[idx], *(u64*)&buf[idx + 8]);
					idx += 16;
					break;
				case KBASE_TL_NRET_ATOM_CTX:
					seq_printf(sfile, "atom: 0x%llx, ctx: 0x%llx\n", \
							*(u64*)&buf[idx], *(u64*)&buf[idx + 8]);
					idx += 16;
					break;
				case KBASE_TL_NRET_ATOM_LPU:
					seq_printf(sfile, "ctx: 0x%llx, lpu: 0x%llx\n", \
							*(u64*)&buf[idx], *(u64*)&buf[idx + 8]);
					idx += 16;
					break;
				case KBASE_TL_RET_AS_CTX:
					seq_printf(sfile, "address_space: 0x%llx, ctx: 0x%llx\n", \
							*(u64*)&buf[idx], *(u64*)&buf[idx + 8]);
					idx += 16;
					break;
				case KBASE_TL_NRET_AS_CTX:
					seq_printf(sfile, "address_space: 0x%llx, ctx: 0x%llx\n", \
							*(u64*)&buf[idx], *(u64*)&buf[idx + 8]);
					idx += 16;
					break;
				case KBASE_TL_RET_ATOM_AS:
					seq_printf(sfile, "atom: 0x%llx, address_space: 0x%llx\n", \
							*(u64*)&buf[idx], *(u64*)&buf[idx + 8]);
					idx += 16;
					break;
				case KBASE_TL_NRET_ATOM_AS:
					seq_printf(sfile, "atom: 0x%llx, address_space: 0x%llx\n", \
							*(u64*)&buf[idx], *(u64*)&buf[idx + 8]);
					idx += 16;
					break;
				case KBASE_TL_ATTRIB_ATOM_CONFIG:
					seq_printf(sfile, "atom: 0x%llx, descriptor: 0x%llx, affinity: 0x%llx,	config: 0x%x\n", *(u64*)&buf[idx], *(u64*)&buf[idx + 8], \
							*(u64*)&buf[idx + 16], *(u32*)&buf[idx + 24]);
					idx += 28;
					break;
				case KBASE_TL_ATTRIB_ATOM_PRIORITY:
					seq_printf(sfile, "atom: 0x%llx, prio: %u\n", \
							*(u64*)&buf[idx], *(u32*)&buf[idx + 8]);
					idx += 12;
					break;
				case KBASE_TL_ATTRIB_ATOM_STATE:
					seq_printf(sfile, "atom: 0x%llx, state: %u\n", \
							*(u64*)&buf[idx], *(u32*)&buf[idx + 8]);
					idx += 12;
					break;
				case KBASE_TL_ATTRIB_ATOM_PRIORITIZED:
					seq_printf(sfile, "atom: 0x%llx\n", \
							*(u64*)&buf[idx]);
					idx += 8;
					break;
				case KBASE_TL_ATTRIB_ATOM_JIT:
					seq_printf(sfile, "\n");	// TODO: figure out format
					// jin: 8 + 8 + 8 + 4 + 8 + 4 + 8 + 8 + 8
					idx += 64;
					break;
				case KBASE_TL_JIT_USEDPAGES:
					seq_printf(sfile, "used_pages: %llu, j_id: %u\n", \
							*(u64*)&buf[idx], *(u32*)&buf[idx + 8]);
					idx += 12;
					break;
				case KBASE_TL_ATTRIB_ATOM_JITALLOCINFO:
					seq_printf(sfile, "\n");	// TODO: figure out format
					// jin: 32 + 20
					idx += 	52;
					break;
				case KBASE_TL_ATTRIB_ATOM_JITFREEINFO:
					seq_printf(sfile, "atom: 0x%llx, j_id: %u\n", \
							*(u64*)&buf[idx], *(u32*)&buf[idx + 8]);
					idx += 12;
					break;
				case KBASE_TL_ATTRIB_AS_CONFIG:	// jin: called from mmu_hw_configure
					seq_printf(sfile, "address_space: 0x%llx, transtab: 0x%llx, memattr: 0x%llx, transcfg: 0x%llx\n", \
							*(u64*)&buf[idx], *(u64*)&buf[idx + 8], *(u64*)&buf[idx + 16], *(u64*)&buf[idx + 24]);
					idx += 32;
					break;
				case KBASE_TL_EVENT_LPU_SOFTSTOP:
					seq_printf(sfile, "lpu: 0x%llx\n", \
							*(u64*)&buf[idx]);
					idx += 8;
					break;
				case KBASE_TL_EVENT_ATOM_SOFTSTOP_EX:
					seq_printf(sfile, "atom: 0x%llx\n", \
							*(u64*)&buf[idx]);
					idx += 8;
					break;
				case KBASE_TL_EVENT_ATOM_SOFTSTOP_ISSUE:
					seq_printf(sfile, "atom: 0x%llx\n", \
							*(u64*)&buf[idx]);
					idx += 8;
					break;
				case KBASE_TL_EVENT_ATOM_SOFTJOB_START:
					seq_printf(sfile, "atom: 0x%llx\n", \
							*(u64*)&buf[idx]);
					idx += 8;
					break;
				case KBASE_TL_EVENT_ATOM_SOFTJOB_END:
					seq_printf(sfile, "atom: 0x%llx\n", \
							*(u64*)&buf[idx]);
					idx += 8;
					break;
				case KBASE_JD_GPU_SOFT_RESET:
					seq_printf(sfile, "gpu: 0x%llx\n", \
							*(u64*)&buf[idx]);
					idx += 8;
					break;
				case KBASE_TL_KBASE_NEW_DEVICE:
					seq_printf(sfile, "kbase_device_id: %u, \
							kbase_device_gpu_core_count: %u, kbase_device_max_num_csgs: %u\n", \
							*(u32*)&buf[idx], *(u32*)&buf[idx + 4], *(u32*)&buf[idx +8]);
					idx += 12;
					break;
				case KBASE_TL_KBASE_DEVICE_PROGRAM_CSG:
					seq_printf(sfile, "kbase_device_id: %u, \
							kbase_cmdq_grp_handle: %u, kbase_device_csg_slot_index: %u\n", \
							*(u32*)&buf[idx], *(u32*)&buf[idx + 4], *(u32*)&buf[idx +8]);
					idx += 12;
					break;
				case KBASE_TL_KBASE_DEVICE_DEPROGRAM_CSG:
					seq_printf(sfile, "kbase_device_id: %u, \
							kbase_device_csg_slot_index: %u\n", \
							*(u32*)&buf[idx], *(u32*)&buf[idx + 4]);
					idx += 8;
					break;
				case KBASE_TL_KBASE_NEW_CTX:
					seq_printf(sfile, "kbase_ctx_id: %u, kbase_device_id: %u\n", \
							*(u32*)&buf[idx], *(u32*)&buf[idx + 4]);
					idx += 8;
					break;
				case KBASE_TL_KBASE_DEL_CTX:
					seq_printf(sfile, "kbase_ctx_id: %u\n", \
							*(u32*)&buf[idx]);
					idx += 4;
					break;
				case KBASE_TL_KBASE_NEW_KCPUQUEUE:
					seq_printf(sfile, "kcpu_queue: %llu, kbase_ctx_id: %u\n, \
							kbase_num_pending_cmds: %u", *(u64*)&buf[idx], *(u32*)&buf[idx + 8], *(u32*)&buf[idx + 12]);
					idx += 16;
					break;
				case KBASE_TL_KBASE_DEL_KCPUQUEUE:
					seq_printf(sfile, "kcpu_queue: %llu\n", \
							*(u64*)&buf[idx]);
					idx += 8;
					break;
				case KBASE_TL_KBASE_KCPUQUEUE_ENQUEUE_FENCE_SIGNAL:
					seq_printf(sfile, "kcpu_queue: %llu, fence: %llu\n", \
							*(u64*)&buf[idx], *(u64*)&buf[idx + 8]);
					idx += 16;
					break;
				case KBASE_TL_KBASE_KCPUQUEUE_ENQUEUE_FENCE_WAIT:
					seq_printf(sfile, "kcpu_queue: %llu, fence: %llu\n", \
							*(u64*)&buf[idx], *(u64*)&buf[idx + 8]);
					idx += 16;
				case KBASE_TL_KBASE_ARRAY_BEGIN_KCPUQUEUE_ENQUEUE_CQS_WAIT:
					seq_printf(sfile, "kcpu_queue: %llu\n", \
							*(u64*)&buf[idx]);
					idx += 8;
					break;
				case KBASE_TL_KBASE_ARRAY_ITEM_KCPUQUEUE_ENQUEUE_CQS_WAIT:
					seq_printf(sfile, "kcpu_queue: %llu, cqs_obj_gpu_addr: 0x%llx \
							cqs_obj_compare_value: %u\n", *(u64*)&buf[idx], *(u64*)&buf[idx + 8], \
							*(u32*)&buf[idx + 16]);
					idx += 20;
					break;
				case KBASE_TL_KBASE_ARRAY_END_KCPUQUEUE_ENQUEUE_CQS_WAIT:
					seq_printf(sfile, "kcpu_queue: %llu\n", \
							*(u64*)&buf[idx]);
					idx += 8;
					break;
				case KBASE_TL_KBASE_ARRAY_BEGIN_KCPUQUEUE_ENQUEUE_CQS_SET:
					seq_printf(sfile, "kcpu_queue: %llu\n", \
							*(u64*)&buf[idx]);
					idx += 8;
					break;
				case KBASE_TL_KBASE_ARRAY_ITEM_KCPUQUEUE_ENQUEUE_CQS_SET:
					seq_printf(sfile, "kcpu_queue: %llu, cpu_obj_gpu_addr: 0x%llx\n", \
							*(u64*)&buf[idx], *(u64*)&buf[idx + 8]);
					idx += 16;
					break;
				case KBASE_TL_KBASE_ARRAY_END_KCPUQUEUE_ENQUEUE_CQS_SET:
					seq_printf(sfile, "kcpu_queue: %llu\n", \
							*(u64*)&buf[idx]);
					idx += 8;
					break;
				case KBASE_TL_KBASE_ARRAY_BEGIN_KCPUQUEUE_ENQUEUE_DEBUGCOPY:
					seq_printf(sfile, "kcpu_queue: %llu\n", \
							*(u64*)&buf[idx]);
					idx += 8;
					break;
				case KBASE_TL_KBASE_ARRAY_ITEM_KCPUQUEUE_ENQUEUE_DEBUGCOPY:
					seq_printf(sfile, "kcpu_queue: %llu, debugcopy_dst_size: %llu\n", \
							*(u64*)&buf[idx], *(u64*)&buf[idx + 8]);
					idx += 16;
					break;
				case KBASE_TL_KBASE_ARRAY_END_KCPUQUEUE_ENQUEUE_DEBUGCOPY:
					seq_printf(sfile, "kcpu_queue: %llu\n", \
							*(u64*)&buf[idx]);
					idx += 8;
					break;
				case KBASE_TL_KBASE_KCPUQUEUE_ENQUEUE_MAP_IMPORT:
					seq_printf(sfile, "kcpu_queue: %llu, map_import_buf_gpu_addr: 0x%llx\n", \
							*(u64*)&buf[idx], *(u64*)&buf[idx + 8]);
					idx += 16;
					break;
				case KBASE_TL_KBASE_KCPUQUEUE_ENQUEUE_UNMAP_IMPORT:
					seq_printf(sfile, "kcpu_queue: %llu, map_import_buf_gpu_addr: 0x%llx\n", \
							*(u64*)&buf[idx], *(u64*)&buf[idx + 8]);
					idx += 16;
					break;
				case KBASE_TL_KBASE_KCPUQUEUE_ENQUEUE_UNMAP_IMPORT_FORCE:
					seq_printf(sfile, "kcpu_queue: %llu, map_import_buf_gpu_addr: 0x%llx\n", \
							*(u64*)&buf[idx], *(u64*)&buf[idx + 8]);
					idx += 16;
					break;
				case KBASE_TL_KBASE_ARRAY_BEGIN_KCPUQUEUE_ENQUEUE_JIT_ALLOC:
					seq_printf(sfile, "kcpu_queue: %llu\n", \
							*(u64*)&buf[idx]);
					idx += 8;
					break;
				case KBASE_TL_KBASE_ARRAY_ITEM_KCPUQUEUE_ENQUEUE_JIT_ALLOC:
					seq_printf(sfile, "TODO: need to do it\n");
					idx += 60;
					break;
				case KBASE_TL_KBASE_ARRAY_END_KCPUQUEUE_ENQUEUE_JIT_ALLOC:
					seq_printf(sfile, "kcpu_queue: %llu\n", \
							*(u64*)&buf[idx]);
					idx += 8;
					break;
				case KBASE_TL_KBASE_ARRAY_BEGIN_KCPUQUEUE_ENQUEUE_JIT_FREE:
					seq_printf(sfile, "kcpu_queue: %llu\n", \
							*(u64*)&buf[idx]);
					idx += 8;
					break;
				case KBASE_TL_KBASE_ARRAY_ITEM_KCPUQUEUE_ENQUEUE_JIT_FREE:
					seq_printf(sfile, "kcpu_queue: %llu, jit_alloc_jit_id: %u\n", \
							*(u64*)&buf[idx], *(u32*)&buf[idx + 8]);
					idx += 12;
					break;
				case KBASE_TL_KBASE_ARRAY_END_KCPUQUEUE_ENQUEUE_JIT_FREE:
					seq_printf(sfile, "kcpu_queue: %llu\n", \
							*(u64*)&buf[idx]);
					idx += 8;
					break;
				case KBASE_TL_KBASE_KCPUQUEUE_EXECUTE_FENCE_SIGNAL_START:
					seq_printf(sfile, "kcpu_queue: %llu\n", \
							*(u64*)&buf[idx]);
					idx += 8;
					break;
				case KBASE_TL_KBASE_KCPUQUEUE_EXECUTE_FENCE_SIGNAL_END:
					seq_printf(sfile, "kcpu_queue: %llu\n", \
							*(u64*)&buf[idx]);
					idx += 8;
					break;
				case KBASE_TL_KBASE_KCPUQUEUE_EXECUTE_FENCE_WAIT_START:
					seq_printf(sfile, "kcpu_queue: %llu\n", \
							*(u64*)&buf[idx]);
					idx += 8;
					break;
				case KBASE_TL_KBASE_KCPUQUEUE_EXECUTE_FENCE_WAIT_END:
					seq_printf(sfile, "kcpu_queue: %llu\n", \
							*(u64*)&buf[idx]);
					idx += 8;
					break;
				case KBASE_TL_KBASE_KCPUQUEUE_EXECUTE_CQS_WAIT_START:
					seq_printf(sfile, "kcpu_queue: %llu\n", \
							*(u64*)&buf[idx]);
					idx += 8;
					break;
				case KBASE_TL_KBASE_KCPUQUEUE_EXECUTE_CQS_WAIT_END:
					seq_printf(sfile, "kcpu_queue: %llu\n", \
							*(u64*)&buf[idx]);
					idx += 8;
					break;
				case KBASE_TL_KBASE_KCPUQUEUE_EXECUTE_CQS_SET:
					seq_printf(sfile, "kcpu_queue: %llu\n", \
							*(u64*)&buf[idx]);
					idx += 8;
					break;
				case KBASE_TL_KBASE_KCPUQUEUE_EXECUTE_DEBUGCOPY_START:
					seq_printf(sfile, "kcpu_queue: %llu\n", \
							*(u64*)&buf[idx]);
					idx += 8;
					break;
				case KBASE_TL_KBASE_KCPUQUEUE_EXECUTE_DEBUGCOPY_END:
					seq_printf(sfile, "kcpu_queue: %llu\n", \
							*(u64*)&buf[idx]);
					idx += 8;
					break;
				case KBASE_TL_KBASE_KCPUQUEUE_EXECUTE_MAP_IMPORT_START:
					seq_printf(sfile, "kcpu_queue: %llu\n", \
							*(u64*)&buf[idx]);
					idx += 8;
					break;
				case KBASE_TL_KBASE_KCPUQUEUE_EXECUTE_MAP_IMPORT_END:
					seq_printf(sfile, "kcpu_queue: %llu\n", \
							*(u64*)&buf[idx]);
					idx += 8;
					break;
				case KBASE_TL_KBASE_KCPUQUEUE_EXECUTE_UNMAP_IMPORT_START:
					seq_printf(sfile, "kcpu_queue: %llu\n", \
							*(u64*)&buf[idx]);
					idx += 8;
					break;
				case KBASE_TL_KBASE_KCPUQUEUE_EXECUTE_UNMAP_IMPORT_END:
					seq_printf(sfile, "kcpu_queue: %llu\n", \
							*(u64*)&buf[idx]);
					idx += 8;
					break;
				case KBASE_TL_KBASE_KCPUQUEUE_EXECUTE_UNMAP_IMPORT_FORCE_START:
					seq_printf(sfile, "kcpu_queue: %llu\n", \
							*(u64*)&buf[idx]);
					idx += 8;
					break;
				case KBASE_TL_KBASE_KCPUQUEUE_EXECUTE_UNMAP_IMPORT_FORCE_END:
					seq_printf(sfile, "kcpu_queue: %llu\n", \
							*(u64*)&buf[idx]);
					idx += 8;
					break;
				case KBASE_TL_KBASE_KCPUQUEUE_EXECUTE_JIT_ALLOC_START:
					seq_printf(sfile, "kcpu_queue: %llu\n", \
							*(u64*)&buf[idx]);
					idx += 8;
					break;
				case KBASE_TL_KBASE_ARRAY_BEGIN_KCPUQUEUE_EXECUTE_JIT_ALLOC_END:
					seq_printf(sfile, "kcpu_queue: %llu\n", \
							*(u64*)&buf[idx]);
					idx += 8;
					break;
				case KBASE_TL_KBASE_ARRAY_ITEM_KCPUQUEUE_EXECUTE_JIT_ALLOC_END:
					seq_printf(sfile, "kcpu_queue: %llu\n", \
							*(u64*)&buf[idx]);
					idx += 8;
					break;
				case KBASE_TL_KBASE_ARRAY_END_KCPUQUEUE_EXECUTE_JIT_ALLOC_END:
					seq_printf(sfile, "TODO: update it later\n");
					idx += 24;
					break;
				case KBASE_TL_KBASE_KCPUQUEUE_EXECUTE_JIT_FREE_START:
					seq_printf(sfile, "kcpu_queue: %llu\n", \
							*(u64*)&buf[idx]);
					idx += 8;
					break;
				case KBASE_TL_KBASE_ARRAY_BEGIN_KCPUQUEUE_EXECUTE_JIT_FREE_END:
					seq_printf(sfile, "kcpu_queue: %llu\n", \
							*(u64*)&buf[idx]);
					idx += 8;
					break;
				case KBASE_TL_KBASE_ARRAY_ITEM_KCPUQUEUE_EXECUTE_JIT_FREE_END:
					seq_printf(sfile, "kcpu_queue: %llu, jit_free_pages_used: %llu\n", \
							*(u64*)&buf[idx], *(u64*)&buf[idx+8]);
					idx += 16;
					break;
				case KBASE_TL_KBASE_ARRAY_END_KCPUQUEUE_EXECUTE_JIT_FREE_END:
					seq_printf(sfile, "kcpu_queue: %llu\n", \
							*(u64*)&buf[idx]);
					idx += 8;
					break;
				case KBASE_TL_KBASE_KCPUQUEUE_EXECUTE_ERRORBARRIER:
					seq_printf(sfile, "kcpu_queue: %llu\n", \
							*(u64*)&buf[idx]);
					idx += 8;
					break;
				case KBASE_OBJ_MSG_COUNT:
				default:
					break;
			}
		}
	}

	spin_unlock_irqrestore(&stream->lock, flags);
}

static void timeline_history_aux(struct seq_file *sfile, struct kbase_tlstream *stream)
{
	unsigned long flags;
	unsigned int wb_idx_raw;
	unsigned int wb_idx;
	size_t wb_size;

	char *buf;
	size_t idx;
	u32 type, tid;
	u64 ts;
	size_t i;

	spin_lock_irqsave(&stream->lock, flags);

	wb_idx_raw 	= atomic_read(&stream->wbi);
	wb_idx		= wb_idx_raw % PACKET_COUNT;

	for (i = 0; i <= wb_idx; i++) {
		wb_size = atomic_read(&stream->buffer[i].size);

		if (stream->numbered)
			idx = PACKET_HEADER_SIZE + PACKET_NUMBER_SIZE;
		else
			idx = PACKET_HEADER_SIZE;

		buf = &stream->buffer[i].data[0];
   /**      seq_printf(sfile, "[jin] wb_idx: %lu\n", i); */
		/** seq_printf(sfile, "[jin] wb_size : %lu, idx: %lu\n", wb_size, idx); */

#if 1
		while (idx < wb_size) {
			type = *(u32*) &buf[idx];
			idx += sizeof(u32);
			ts = *(u64*) &buf[idx];
			idx += sizeof(u64);
			tid = *(u32*) &buf[idx];
			idx += sizeof(u32);
			/** printk("[jin][%lu] type : %u", ts, type); */
			/** printk("[jin][%lu] type : %s", ts, aux[type]); */

			seq_printf(sfile, "%5llu\t%5s\t%6d\t%30s\t", ts, "AUX", tid, aux[type]);
			switch (type) {
				case KBASE_AUX_PM_STATE :
					/** printk("\033[0;35m" "[jin][%5llu] %30s\t core_type: %u, core_state_bitset: %llu" "\033[0m", \ */
					seq_printf(sfile, "core_type: 0x%x, core_state_bitset: %llu\n", \
							*(u32*)&buf[idx], *(u64*)&buf[idx + 4]);
					idx += 12;
					break;
				case KBASE_AUX_PAGEFAULT:
					seq_printf(sfile, "ctx_nr: %u, as_nr: %u, page_cnt_change: %llu\n", \
							*(u32*)&buf[idx], *(u32*)&buf[idx + 4], *(u64*)&buf[idx + 8]);
					idx += 24;
					break;
				case KBASE_AUX_PAGESALLOC:
					seq_printf(sfile, "ctx_nr: %u, page_cnt: %llu\n", \
							*(u32*)&buf[idx], *(u64*)&buf[idx + 4]);
					idx += 12;
					break;
				case KBASE_AUX_DEVFREQ_TARGET:
					seq_printf(sfile, "target_freq: %llu\n", \
							*(u64*)&buf[idx]);
					idx += 8;
					break;
				case KBASE_AUX_PROTECTED_ENTER_START:
					seq_printf(sfile, "gpu: 0x%llx\n", \
							*(u64*)&buf[idx]);
					idx += 8;
					break;
				case KBASE_AUX_PROTECTED_ENTER_END:
					seq_printf(sfile, "gpu: 0x%llx\n", \
							*(u64*)&buf[idx]);
					idx += 8;
					break;
				case KBASE_AUX_PROTECTED_LEAVE_START:
					seq_printf(sfile, "gpu: 0x%llx\n", \
							*(u64*)&buf[idx]);
					idx += 8;
					break;
				case KBASE_AUX_PROTECTED_LEAVE_END:
					seq_printf(sfile, "gpu: 0x%llx\n", \
							*(u64*)&buf[idx]);
					idx += 8;
					break;
				case KBASE_AUX_JIT_STATS:
					seq_printf(sfile, "ctx_nr: %u, bid: %u, max_allocs: %u, allocs: %u\n, \
							va_pages: %u, ph_pages: %u", *(u32*)&buf[idx], \
							*(u32*)&buf[idx + 4], *(u32*)&buf[idx + 8], *(u32*)&buf[idx + 12], \
							*(u32*)&buf[idx + 16], *(u32*)&buf[idx + 20]);
					idx += 24;
					break;
				case KBASE_AUX_EVENT_JOB_SLOT:
					seq_printf(sfile, "ctx: 0x%llx, slot_nr: %u, atom_nr: %u, event: %u\n", \
							*(u64*)&buf[idx], *(u32*)&buf[idx + 4], *(u32*)&buf[idx + 8], *(u32*)&buf[idx + 12]);
					idx += 20;
					break;
				case KBASE_AUX_MSG_COUNT:
				default:
					break;
			}	// switch
		}	// while
#endif
	}	// else if
	spin_unlock_irqrestore(&stream->lock, flags);

}

static int timeline_history_show(struct seq_file *sfile, void *data)
{
	struct kbase_timeline *const timeline =  sfile->private;
	enum tl_stream_type stype;

	for (stype = 0; stype < TL_STREAM_TYPE_COUNT; stype++) {
		mutex_lock(&timeline->reader_lock);

		if (stype == TL_STREAM_TYPE_OBJ) {
			timeline_history_obj(sfile, &timeline->streams[stype]);

		} else if (stype == TL_STREAM_TYPE_AUX) {
			timeline_history_aux(sfile, &timeline->streams[stype]);

		} else {
			// jin: No stream
		}
		mutex_unlock(&timeline->reader_lock);
	}

	return 0;
}

static int timeline_history_open(struct inode *in, struct file *file)
{
	return single_open(file, &timeline_history_show, in->i_private);
}

static const struct file_operations timeline_history_fops = {
	.owner = THIS_MODULE,
	.open = &timeline_history_open,
	.read = seq_read,
	.llseek = seq_lseek,
	.release = single_release,
};

static void rbuf_dump_msg(struct seq_file *sfile, struct kbase_trace *trace_msg)
{
	char buffer[DEBUG_MESSAGE_SIZE];
	int len = DEBUG_MESSAGE_SIZE;
	s32 written = 0;

#ifdef CONFIG_GR_DEBUG_FS_VERBOSE
	/** written += MAX(snprintf(buffer + written, MAX(len - written, 0), "[%5llu][RBUF] ", trace_msg->ts), 0); */
	written += MAX(snprintf(buffer + written, MAX(len - written, 0), "%5llu\t%5s\t%6d", trace_msg->ts, "RBUF", trace_msg->thread_id), 0);

	/* Initial part of message */
   /**  written += MAX(snprintf(buffer + written, MAX(len - written, 0), "%s, ctx:%p, tid:%d, core:%d  ", kbasep_trace_code_string[trace_msg->code], trace_msg->ctx, trace_msg->thread_id, trace_msg->cpu), 0); */
	// jin: thread_id, cpu
	/** written += MAX(snprintf(buffer + written, MAX(len - written, 0), "%25s, tid:%d, core:%d  ", kbasep_trace_code_string[trace_msg->code], trace_msg->thread_id, trace_msg->cpu), 0); */
	// jin: no thread_id, cpu
	written += MAX(snprintf(buffer + written, MAX(len - written, 0), "\t%30s, ", kbasep_trace_code_string[trace_msg->code]), 0);
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
	else
		written += MAX(snprintf(buffer + written, MAX(len - written, 0), ","), 0);

	/* Refcount present */
	if (trace_msg->flags & KBASE_TRACE_FLAG_REFCOUNT)
		written += MAX(snprintf(buffer + written, MAX(len - written, 0), "%d", trace_msg->refcount), 0);

	written += MAX(snprintf(buffer + written, MAX(len - written, 0), ","), 0);

	/* Rest of message */
	written += MAX(snprintf(buffer + written, MAX(len - written, 0), "0x%.8lx", trace_msg->info_val), 0);

#ifdef CONFIG_GR_DEBUG_FS_VERBOSE
	/** written += MAX(snprintf(buffer + written, MAX(len - written, 0), " [tid:%d, core:%d] ", trace_msg->thread_id, trace_msg->cpu), 0); */
#endif

	seq_printf(sfile, "%s\n", buffer); 
	
}

#ifdef KBASE_TRACE_ENABLE
static int rbuf_history_show(struct seq_file *sfile, void *data)
{
	struct kbase_device *const kbdev =  sfile->private;

	unsigned long flags;
	u32 start;
	u32 end;

	spin_lock_irqsave(&kbdev->trace_lock, flags);
	start = kbdev->trace_first_out;
	end = kbdev->trace_next_in;

	while (start != end) {
		struct kbase_trace *trace_msg = &kbdev->trace_rbuf[start];

		rbuf_dump_msg(sfile, trace_msg);
		/** kbasep_trace_format_msg(trace_msg, buffer, DEBUG_MESSAGE_SIZE); */

		start = (start + 1) & KBASE_TRACE_MASK;
	}
	spin_unlock_irqrestore(&kbdev->trace_lock, flags);
	/** KBASE_TRACE_CLEAR(kbdev); */

	return 0;
}

static int rbuf_history_open(struct inode *in, struct file *file)
{
	return single_open(file, &rbuf_history_show, in->i_private);
}

static const struct file_operations rbuf_history_fops = {
	.owner = THIS_MODULE,
	.open = &rbuf_history_open,
	.read = seq_read,
	.llseek = seq_lseek,
	.release = single_release,
};
#endif	// KBASE_TRACE_ENABLE

#endif	// CONFIG_GR_DEBUG_FS_VERBOSE

void kbasep_regs_history_debugfs_init(struct kbase_device *kbdev)
{
	debugfs_create_bool("regs_history_enabled", S_IRUGO | S_IWUSR,
			kbdev->mali_debugfs_directory,
			&kbdev->io_history.enabled);
	debugfs_create_file("regs_history_size", S_IRUGO | S_IWUSR,
			kbdev->mali_debugfs_directory,
			&kbdev->io_history, &regs_history_size_fops);
	debugfs_create_file("regs_history", S_IRUGO,
			kbdev->mali_debugfs_directory, &kbdev->io_history,
			&regs_history_fops);
#ifdef CONFIG_GR_DEBUG_FS_VERBOSE
	debugfs_create_file("timeline_history", S_IRUGO,
			kbdev->mali_debugfs_directory, kbdev->timeline,
			&timeline_history_fops);
#ifdef KBASE_TRACE_ENABLE
	debugfs_create_file("rbuf_history", S_IRUGO,
			kbdev->mali_debugfs_directory, kbdev,
			&rbuf_history_fops);
#endif	// KBASE_TRACE_ENABLE
#endif	// CONFIG_GR_DEBUG_FS_VERBOSE
}

#endif /* CONFIG_DEBUG_FS */
