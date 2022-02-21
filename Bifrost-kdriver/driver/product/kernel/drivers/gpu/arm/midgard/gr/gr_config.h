/* written by Heejin to configure GPUReplay with Mali Bifrost */

#ifndef _GR_CONFIG_H_
#define _GR_CONFIG_H_

#include "gr/log.h"
#include "gr/measure.h"

#define GR_CONTEXT_EXEC_TIME		0x1
#define GR_PM_TIME					0x2
#define GR_JOB_EXEC_TIME			0x4

/********************************************/
#define CONFIG_GR
#define CONFIG_GR_DEBUG
// #define CONFIG_GR_MEASURE_TIME	GR_CONTEXT_EXEC_TIME | GR_PM_TIME | GR_JOB_EXEC_TIME

// jin: [custom] measure interrupt latency
// #define JIN_MEASURE_IRQ
/********************************************/

#ifdef CONFIG_GR
	/* capture IO history */
	#define CONFIG_GR_IO_HISTORY
	// #define CONFIG_GR_REG_SNAPSHOT
#endif // End of CONFIG_GR

#ifdef CONFIG_GR_DEBUG
	#ifdef CONFIG_DEBUG_FS
		// jin: [custom] dump trace
		/* dump io history with DEBUG_FS */
		/* CONFIG_DEBUG_FS should be defined in advance */
		#define CONFIG_GR_DEBUG_FS_VERBOSE
	#endif

	#ifndef CONFIG_MALI_MIDGARD_ENABLE_TRACE
	// jin: it defines KBASE_TRACE_ENABLE
	// Enable tracing in kbase. Trace log available through the "mali_trace" debugfs file
	#define CONFIG_MALI_MIDGARD_ENABLE_TRACE
	#endif

	// jin: does not work with hikey default linux kernel
	// may need to compile a new kernel with some relevant flags e.g. CONFIG_FTRACE
	// If it is not defined, it uses driver internal trace buffer.
	// In such a case, use KBASE_DUMP_TRACE to dump collected trace
	/* #ifndef CONFIG_MALI_SYSTEM_TRACE */
	// #define CONFIG_MALI_SYSTEM_TRACE
	/* #endif */
	
	// jin: does not work for hikey default linux kenrel
	/* #ifndef CONFIG_GPU_TRACEPOINTS */
	// #define CONFIG_GPU_TRACEPOINTS
	/* #endif */

#endif	// End of CONFIG_GR_DEBUG

#endif // End of GR_CONFIG
