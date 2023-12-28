#include <stdint.h>

#include "deflate.h"

/* insert_string */
extern void insert_string_c(deflate_state *const s, const uint32_t str, uint32_t count);

#ifdef X86_SSE42
extern void insert_string_sse42(deflate_state *const s, const uint32_t str, uint32_t count);
#define INSERT_STRING insert_string_sse42
#elif defined(ARM_ACLE)
extern void insert_string_acle(deflate_state *const s, const uint32_t str, uint32_t count);
#define INSERT_STRING insert_string_acle
#endif

/* quick_insert_string */
extern Pos quick_insert_string_c(deflate_state *const s, const uint32_t str);
#ifdef X86_SSE42
extern Pos quick_insert_string_sse42(deflate_state *const s, const uint32_t str);
#define QUICK_INSERT_STRING quick_insert_string_sse42
#elif defined(ARM_ACLE)
extern Pos quick_insert_string_acle(deflate_state *const s, const uint32_t str);
#define QUICK_INSERT_STRING quick_insert_string_acle
#endif

#ifndef HAVE_NATIVE_INSTRUCTIONS
#  include "functable.h"
#  undef INSERT_STRING
#  define INSERT_STRING functable.insert_string
#  undef QUICK_INSERT_STRING
#  define QUICK_INSERT_STRING functable.quick_insert_string
#else
#  ifndef INSERT_STRING
#    define INSERT_STRING insert_string_c
#  endif
#  ifndef QUICK_INSERT_STRING
#    define QUICK_INSERT_STRING quick_insert_string_c
#  endif
#endif

/* update_hash */
extern uint32_t update_hash_c(deflate_state *const s, uint32_t h, uint32_t val);
#ifdef X86_SSE42
extern uint32_t update_hash_sse42(deflate_state *const s, uint32_t h, uint32_t val);
#elif defined(ARM_ACLE)
extern uint32_t update_hash_acle(deflate_state *const s, uint32_t h, uint32_t val);
#endif
