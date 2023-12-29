#ifndef INSERT_STRING_H_
#define INSERT_STRING_H_

#include "zbuild.h"
#include "deflate.h"

#ifndef HAVE_NATIVE_INSTRUCTIONS
#  include "functable.h"
#endif

/* insert_string */
extern void insert_string_c(deflate_state *const s, const uint32_t str, uint32_t count);

#ifdef X86_SSE42
   extern void insert_string_sse42(deflate_state *const s, const uint32_t str, uint32_t count);
#  define native_insert_string insert_string_sse42
#elif defined(ARM_ACLE)
   extern void insert_string_acle(deflate_state *const s, const uint32_t str, uint32_t count);
#  define native_insert_string insert_string_acle
#endif

/* quick_insert_string */
extern Pos quick_insert_string_c(deflate_state *const s, const uint32_t str);
#ifdef X86_SSE42
   extern Pos quick_insert_string_sse42(deflate_state *const s, const uint32_t str);
#  define native_quick_insert_string quick_insert_string_sse42
#elif defined(ARM_ACLE)
   extern Pos quick_insert_string_acle(deflate_state *const s, const uint32_t str);
#  define native_quick_insert_string quick_insert_string_acle
#endif

/* update_hash */
extern uint32_t update_hash_c(deflate_state *const s, uint32_t h, uint32_t val);
#ifdef X86_SSE42
   extern uint32_t update_hash_sse42(deflate_state *const s, uint32_t h, uint32_t val);
#  define native_update_hash update_hash_sse42
#elif defined(ARM_ACLE)
   extern uint32_t update_hash_acle(deflate_state *const s, uint32_t h, uint32_t val);
#  define native_update_hash update_hash_acle
#endif

#ifndef native_insert_string
#  define native_insert_string insert_string_c
#endif
#ifndef native_quick_insert_string
#  define native_quick_insert_string quick_insert_string_c
#endif
#ifndef native_update_hash
#  define native_update_hash update_hash_c
#endif

#endif
