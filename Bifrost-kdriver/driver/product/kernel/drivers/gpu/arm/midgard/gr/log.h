/*
 * k2: print to tracebuffer support.
 *
 * before include this header:
 * #define
 *
 * The guideline of using k2_print family:
 *
 * - in repeated funcs
 *      E   k2_PRINT: only for error msgs
 *      W k2_PRINt: key execution point for high-level sketch
 *      I k2_Print: important info for easy eye-balling
 *      V k2_print: normal info that may reveal bugs
 *      D k2_print_v: verbose info, just kept there for future use
 *
 * - in one-shot funcs (like procfs execution)
 *    E can be used for info, so that in measurement mode
 *    we can still see them.
 *
 * xzl: 07/01/2015 -- change the func names (easier to use)
 */

#ifndef  __K2_TRACEBUFFER__
#define __K2_TRACEBUFFER__

/* Select KAGE_GLOBAL_DEBUG_LEVEL (arch/arm/common/Kconfig)
 * to globally select dbg msg.
 *
 * The intersection of global level and per-file K2_NO_DEBUG decides
 * the final outpout.
 *
 * lower level == more verbose
 *
 * e.g., global level is 30 (>=info), and a file defines K2_NO_DEBUG,
 * thus for that file only E/k2_PRINT() are effective.
 *
 */

#ifndef CONFIG_KAGE_GLOBAL_DEBUG_LEVEL
//#error "Must define KAGE_GLOBAL_DEBUG_LEVEL"
// #define CONFIG_KAGE_GLOBAL_DEBUG_LEVEL 40 // default
#define CONFIG_KAGE_GLOBAL_DEBUG_LEVEL 45 // turn on JJ
// #define CONFIG_KAGE_GLOBAL_DEBUG_LEVEL 50 // default
#endif

/* example of defining custom funcs... */
#ifdef CONFIG_YOUR_OWN_PLATFORM
/* keep this even when K2_NO_DEBUG as it is used by k2_measure()
to show final trace */
extern void print_to_tracebuffer(const char *fmt, ...);

/* misc dbg helpers. */
#ifdef K2_NO_DEBUG
#define dump_l1_mmu()
#define k2_dump_stack()
#else
extern void dump_l1_mmu(void);
extern void k2_dump_stack(void);
#endif

extern int kage_enable_uart(void);
#else
//#define print_to_tracebuffer(fmt, arg...) printk(KERN_WARNING fmt, ##arg)
#ifdef __KERNEL__
#define print_to_tracebuffer printk
#else /* user space */
#define print_to_tracebuffer printf
#endif
#endif

/* colorful output support. minicom -c on */
#define _k2clr_red   "\033[0;31m"        /* 0 -> normal ;  31 -> red */
#define _k2clr_cyan  "\033[1;36m"        /* 1 -> bold ;  36 -> cyan */
#define _k2clr_green "\033[0;32m"        /* 4 -> underline ;  32 -> green */
#define _k2clr_blue  "\033[1;34m"        /* 9 -> strike ;  34 -> blue */

#define _k2clr_inv_yellow  "\033[7;43m"  /* 7 -> invert */

#define _k2clr_black  "\033[0;30m"
#define _k2clr_brown  "\033[0;33m"
#define _k2clr_magenta  "\033[0;35m"
#define _k2clr_gray  "\033[0;37m"
#define _k2clr_u_gray  "\033[4;37m"   /* 4 -> underline, gray */

#define _k2clr_none   "\033[0m"        /* to flush the previous property */

/* example of prefix in output msgs */
#ifdef CONFIG_CPU_V7
#define K2_PRINT_TAG _k2clr_blue "{k}" _k2clr_none
#elif defined CONFIG_CPU_V7M
#define K2_PRINT_TAG "{m3}"
#else
#define K2_PRINT_TAG ""
#endif

/* Usage: At the start of each .c file:
 * #define K2_NO_DEBUG to suppress everything but k2_PRINt (meant for error).
 * #define K2_DEBUG_VERBOSE to additionally enable k2_print_v().
 */

/* multi defined? let higher override the lower. XXX should warn */

#ifdef K2_DEBUG_VERBOSE
#define K2_LOCAL_DEBUG_LEVEL 10
#endif

#ifdef K2_DEBUG_NORMAL
#define K2_LOCAL_DEBUG_LEVEL 20
#endif

#ifdef K2_DEBUG_INFO
#define K2_LOCAL_DEBUG_LEVEL 30
#endif

#ifdef K2_DEBUG_WARN
#define K2_LOCAL_DEBUG_LEVEL 40
#endif

#ifdef K2_NO_DEBUG
#define K2_LOCAL_DEBUG_LEVEL 50
#endif

#ifndef K2_LOCAL_DEBUG_LEVEL
/* default local lv, if the source file says nothing.
 * By default, we show "V" (and more important) messages -- even when global level suggests more verbose */
#define K2_LOCAL_DEBUG_LEVEL 5 // 20
#endif

/* global or local debug level, we select the higher (more concise) one.
 * (so that msgs are effectively suppressed) */
#if CONFIG_KAGE_GLOBAL_DEBUG_LEVEL > K2_LOCAL_DEBUG_LEVEL
#define K2_ACTUAL_DEBUG_LEVEL CONFIG_KAGE_GLOBAL_DEBUG_LEVEL
#else
#define K2_ACTUAL_DEBUG_LEVEL K2_LOCAL_DEBUG_LEVEL
#endif

/*

   Full format, including source path, which sometimes is long.
   Plus, there seems no reliable way to trim the path.

   #define k2_print(fmt, arg...) \
    print_to_tracebuffer(K2_PRINT_TAG "%s %d %s: " fmt, __FILE__, __LINE__, __func__, ## arg)

    Note: numbers should be consistent with arm/common/KConfig
*/

#if K2_ACTUAL_DEBUG_LEVEL <= 10
#define DD(fmt, arg...) print_to_tracebuffer(fmt, ## arg)
#else
#define DD(fmt, arg...)
#endif

/* V conflicts with boost */
#if K2_ACTUAL_DEBUG_LEVEL <= 20
#define VV(fmt, arg...) \
  print_to_tracebuffer("%d %s: " fmt _k2clr_none "\n", __LINE__, __func__, ## arg)
#else
#define VV(fmt, arg...)
#endif

#if K2_ACTUAL_DEBUG_LEVEL <= 30
#define II(fmt, arg...) \
  print_to_tracebuffer(_k2clr_green "%d %s: " fmt _k2clr_none "\n", __LINE__, __func__, ## arg)
#else
#define II(fmt, arg...)
#endif

#if K2_ACTUAL_DEBUG_LEVEL <= 40
#define WW(fmt, arg...) \
  print_to_tracebuffer(_k2clr_brown "%d %s: " fmt _k2clr_none "\n", __LINE__, __func__, ## arg)
#else
#define WW(fmt, arg...)
#endif

#if K2_ACTUAL_DEBUG_LEVEL <= 45
#define JJ(fmt, arg...) \
  print_to_tracebuffer(_k2clr_blue "%d %s: " fmt _k2clr_none "\n", __LINE__, __func__, ## arg)
#else
#define JJ(fmt, arg...)
#endif

#if K2_ACTUAL_DEBUG_LEVEL <= 50
#define EE(fmt, arg...) \
  print_to_tracebuffer(_k2clr_red fmt _k2clr_none "\n", ## arg)
//  print_to_tracebuffer(_k2clr_red "%d %s: " fmt _k2clr_none "\n", __LINE__, __func__, ## arg)
/* serious notice (e.g. misconfiguration) that will be shown regardless of dbg level */
#define EE_WARN(fmt, arg...) \
	print_to_tracebuffer(K2_PRINT_TAG _k2clr_inv_yellow "%s:%d : " fmt _k2clr_none "\n", __FILE__, __LINE__,  ## arg)
#else
#error "not implemented or wrong debug level"
#endif

/* When specified global NO_DEBUG, if no local flag, fake one.
 * helps to suppress info msgs by print_to_tracebuffer() and guarded with K2_NO_DEBUG.
 * eg see fault.c */
#if (CONFIG_KAGE_GLOBAL_DEBUG_LEVEL == 50) && !defined(K2_NO_DEBUG)
#define K2_NO_DEBUG
#endif


/* Crash/BUG()
 * we try not to crash A9 as it takes time to reboot */
#ifdef __KERNEL__
#define xzl_bug() BUG()
#else
//#define xzl_bug() assert(0);
#define xzl_bug(MSG) do { \
	EE(MSG); \
	abort(); \
} while (0)

#define bug(MSG) do { EE(MSG); abort(); } while (0)
/* idea from kernel */
#define likely(x)       __builtin_expect((x),1)
#define unlikely(x)     __builtin_expect((x),0)
#define xzl_bug_on(condition) do { if (unlikely(condition)) xzl_bug("bug:condition failed"); } while (0)
#define xzl_bug_on_msg(condition, msg) do { if (unlikely(condition)) xzl_bug(msg); } while (0)

#endif

/* used to silience unused warning for assert(x) in release build */
#define UNUSED(x) (void)(x)

/*
 * http://cnicholson.net/2009/02/stupid-c-tricks-adventures-in-assert/
 */

#if 0	// turn off
#include <assert.h>

#ifdef NDEBUG /* release */
#define xzl_assert(x) do { (void)sizeof(x); } while(0)
#else
#define xzl_assert(x) assert(x)
#endif

/* Wrapper to check for errors */
#define CHECK_ERROR(a)                                       \
   if (a)                                                    \
   {                                                         \
      perror("Error at line\n\t" #a "\nSystem Msg");         \
      assert ((a) == 0);                                     \
   }
#endif

/* print configuration macro values
 *
 * http://stackoverflow.com/questions/1164652/printing-name-and-value-of-a-macro
 *
 * NB: "you are willing to put up with the fact that SOMESTRING=SOMESTRING
 * indicates that SOMESTRING has not been defined.."
 * */
#define xzl_str(x)   #x
#define xzl_show_define(x) printf("%s %40s %10s %s\n", _k2clr_green, #x, xzl_str(x), _k2clr_none);
#define xzl_show_undefine(x) printf("%s %40s %10s %s\n", _k2clr_gray, #x, "undefined", _k2clr_none);


/*convert int number to G/M/K... */
#define xzl_int_val(x) \
({ \
	long v; 								\
	if (x < 1024)					\
		v = x;							\
	else if (x < 1024 * 1024)	\
		v = x / 1024;	\
	else if (x < 1024 * 1024 * 1024)	\
		v = x / (1024 * 1024);	\
	else	\
		v = x / (1024 * 1024 * 1024);	\
	v;	\
})

#define xzl_int_postfix(x) \
({ \
	const char *v; 								\
	if (x < 1024)					\
		v = "";							\
	else if (x < 1024 * 1024)	\
		v = "K";	\
	else if (x < 1024 * 1024 * 1024)	\
		v = "M";	\
	else	\
		v = "G";	\
	v;	\
})

/* convert a string like "128M" to value 134217728.
 *
 * return value can't be neg. must be within the range of unsigned long.
 *
 * NB: we may overwrite chars pointed by @x. (for impl simplicity) */
#if 0
#include <string.h>
#include <limits.h>
#include <stdlib.h>
#include <stdio.h>

static inline unsigned long xzl_ulong_postfix(char *x)
{
  size_t len;
  unsigned long factor = 1;
  char c;
  long v;
  unsigned long vv;
  const int base = 1000; /* or 1024 */

  xzl_bug_on(!x);
  len = strnlen(x, 32 /* maxlen */);
  c = x[len - 1];

  if (c == 'k' || c == 'K') {
    factor = base;
    x[len - 1] = '\0';
  } else if (c == 'm' || c == 'M') {
    factor = base * base;
    x[len - 1] = '\0';
  }  else if (c == 'g' || c == 'G') {
    factor = base * base * base;
    x[len - 1] = '\0';
  } else
    xzl_bug_on_msg(!(c >= '0' && c <= '9'), "unknown postfix");

  v = strtol(x, NULL, 10 /* base */);
  xzl_bug_on(v < 0 || v == LONG_MAX || v == LONG_MIN); /* unsupported or overflow */

  /* check bound */
  xzl_bug_on_msg(ULONG_MAX / factor < (unsigned long)v, "out of the bound of ULONG_MAX");

  vv = v * factor;

  return vv;
}
#endif

#endif // __K2_TRACEBUFFER__
