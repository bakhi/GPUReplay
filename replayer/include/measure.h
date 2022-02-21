/* this relies on the logging facility.
 * make sure to include log-xzl.h before this header.
 */
#ifndef _CTM_H
#define _CTM_H

#ifdef __cplusplus
extern "C" {
#endif

#ifdef __KERNEL__
#include <linux/time.h>
#include <asm/types.h>

extern void do_gettimeofday(struct timeval *tv);	// jin

#else
#include <stdint.h>
#include <sys/types.h>
#include <sys/time.h>
#endif

typedef uint32_t u32;

#define CONFIG_ARCH_OMAP4 	1	 // to enable gettimeofday()
//#ifdef CONFIG_ARCH_OMAP4
///* ~60-100 cycles on omap4460 */
//static inline unsigned int k2_get_cyclecount(void)
//{
//  unsigned int value;
//  // Read CCNT Register
//  asm volatile ("MRC p15, 0, %0, c9, c13, 0\t\n": "=r"(value));
//  return value;
//}
//#endif

#define USEC_PER_SEC	1000000L

#ifdef __KERNEL__
#define	k2_gettimeofday do_gettimeofday
#else
#define	k2_gettimeofday(x) gettimeofday(x, NULL)
#endif

#ifndef K2_NO_MEASUREMENT
u32 k2_measure(const char *msg);
void k2_measure_flush(void);
// void k2_flush_measure_format(void);		// jin
void k2_calibrate(void);
int k2_measure_clean(void);
void hex_dump(const void *data, size_t size);
#else
#define k2_do_measure(msg)
static inline void k2_measure(const char *msg) { }
static inline void k2_measure_flush(void){ }
// static inline void k2_flush_measure_format(void){ }		// jin
static inline void k2_calibrate(void){ }
static inline int k2_measure_clean(void){ return 0; }
static inline void hex_dump(const void *data, size_t size) { }
#endif

/* global perf stat. note this is indep of per-file KAGE_NO_MEASUREMENT */
struct _perf_stat {
#if 0
	/* cc */
	u32 cc_req;
	u32	cc_serv;
	/* nw sched */
	u32 nw_suspend;
	u32	nw_resume;
#endif
	/* measurement buffer overflow. */
	u32 mb_overflow;
#ifdef CONFIG_ARCH_OMAP_M3
	/* hetero, platform limits */
	u32 m3_redirect_toarm;
	u32 m3_redirect_tothumb;
	u32 m3_l1_swap;
	/* exceptions */
	u32 m3_busfault;
	u32 m3_hardfault;
	u32 m3_usagefault;
	u32 m3_memmanagefault;
#endif
	/* xxx */
};

/* the global perf stat d/s.
 * in theory, any access to it should use lock.
 * XXX do that later XXX
 */
extern struct _perf_stat k2_perf_stat;

extern void k2_perf_stat_snapshot(struct _perf_stat *out);

#ifdef __cplusplus
}		// extern C
#endif

#endif
