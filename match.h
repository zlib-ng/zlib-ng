#ifndef MATCH_H_
#define MATCH_H_

#include "zbuild.h"
#include "deflate.h"

#ifdef X86_FEATURES
#  include "fallback_builtins.h"
#endif
#ifndef HAVE_NATIVE_INSTRUCTIONS
#  include "functable.h"
#endif

/* longest_match */
extern uint32_t longest_match_c(deflate_state *const s, Pos cur_match);
#define native_longest_match longest_match_c
#if defined(UNALIGNED_OK) && BYTE_ORDER == LITTLE_ENDIAN
   extern uint32_t longest_match_unaligned_16(deflate_state *const s, Pos cur_match);
#  undef native_longest_match
#  define native_longest_match longest_match_unaligned_16
#  ifdef HAVE_BUILTIN_CTZ
     extern uint32_t longest_match_unaligned_32(deflate_state *const s, Pos cur_match);
#    undef native_longest_match
#    define native_longest_match longest_match_unaligned_32
#  endif
#  if defined(UNALIGNED64_OK) && defined(HAVE_BUILTIN_CTZLL)
     extern uint32_t longest_match_unaligned_64(deflate_state *const s, Pos cur_match);
#    undef native_longest_match
#    define native_longest_match longest_match_unaligned_64
#  endif
#endif

#if defined(ARM_NEON) && defined(HAVE_BUILTIN_CTZLL)
   extern uint32_t longest_match_neon(deflate_state *const s, Pos cur_match);
#  undef native_longest_match
#  define native_longest_match longest_match_neon
#endif
#ifdef POWER9
   extern uint32_t longest_match_power9(deflate_state *const s, Pos cur_match);
#  undef native_longest_match
#  define native_longest_match longest_match_power9
#endif
#ifdef RISCV_RVV
   extern uint32_t longest_match_rvv(deflate_state *const s, Pos cur_match);
#  undef native_longest_match
#  define native_longest_match longest_match_rvv
#endif
#if defined(X86_SSE2) && defined(HAVE_BUILTIN_CTZ)
   extern uint32_t longest_match_sse2(deflate_state *const s, Pos cur_match);
#  undef native_longest_match
#  define native_longest_match longest_match_sse2
#endif
#if defined(X86_AVX2) && defined(HAVE_BUILTIN_CTZ)
   extern uint32_t longest_match_avx2(deflate_state *const s, Pos cur_match);
#  undef native_longest_match
#  define native_longest_match longest_match_avx2
#endif

/* longest_match_slow */
extern uint32_t longest_match_slow_c(deflate_state *const s, Pos cur_match);
#define LONGEST_MATCH_SLOW longest_match_slow_c
#if defined(UNALIGNED_OK) && BYTE_ORDER == LITTLE_ENDIAN
   extern uint32_t longest_match_slow_unaligned_16(deflate_state *const s, Pos cur_match);
#  undef native_longest_match_slow
#  define native_longest_match_slow longest_match_slow_unaligned_16
#  ifdef HAVE_BUILTIN_CTZ
     extern uint32_t longest_match_slow_unaligned_32(deflate_state *const s, Pos cur_match);
#    undef native_longest_match_slow
#    define native_longest_match_slow longest_match_slow_unaligned_32
#  endif
#  if defined(UNALIGNED64_OK) && defined(HAVE_BUILTIN_CTZLL)
     extern uint32_t longest_match_slow_unaligned_64(deflate_state *const s, Pos cur_match);
#    undef native_longest_match_slow
#    define native_longest_match_slow longest_match_slow_unaligned_64
#  endif
#endif

#if defined(ARM_NEON) && defined(HAVE_BUILTIN_CTZLL)
   extern uint32_t longest_match_slow_neon(deflate_state *const s, Pos cur_match);
#  undef native_longest_match_slow
#  define native_longest_match_slow longest_match_slow_neon
#endif
#ifdef POWER9
   extern uint32_t longest_match_slow_power9(deflate_state *const s, Pos cur_match);
#  undef native_longest_match_slow
#  define native_longest_match_slow longest_match_slow_power9
#endif
#ifdef RISCV_RVV
   extern uint32_t longest_match_slow_rvv(deflate_state *const s, Pos cur_match);
#  undef native_longest_match_slow
#  define native_longest_match_slow longest_match_slow_rvv
#endif
#if defined(X86_SSE2) && defined(HAVE_BUILTIN_CTZ)
   extern uint32_t longest_match_slow_sse2(deflate_state *const s, Pos cur_match);
#  undef native_longest_match_slow
#define native_longest_match_slow longest_match_slow_sse2
#endif
#if defined(X86_AVX2) && defined(HAVE_BUILTIN_CTZ)
   extern uint32_t longest_match_slow_avx2(deflate_state *const s, Pos cur_match);
#  undef native_longest_match_slow
#  define native_longest_match_slow longest_match_slow_avx2
#endif

#endif
