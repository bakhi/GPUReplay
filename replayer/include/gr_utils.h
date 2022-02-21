#ifndef _GR_UTILS_H_
#define _GR_UTILS_H_

#include <stdint.h>
#include "gr_defs.h"
#include "log.h"

void gr_clean_invalid_region(struct replay_context *rctx);
void gr_clean_used_atom(struct replay_context *rctx);
void gr_restore_pte(struct replay_context *rctx);
void gr_mem_contents_check(struct replay_context *rctx, const char *path);

#ifdef DEBUG
void hex_dump(const void *data, size_t size);
void gr_dump_phys(const char *name, uint64_t paddr, size_t size);
void gr_tune_mc(const char* rpath, const char* wpath);
#else
static inline void hex_dump(const void *data, size_t size) { }
static inline void gr_dump_phys(const char *name, uint64_t paddr, size_t size) { }
static inline void gr_tune_mc(const char* rpath, const char* wpath) { }
#endif

#endif
