#include <stdint.h>

#include "deflate.h"

#ifdef X86_FEATURES
#  include "fallback_builtins.h"
#endif

/* longest_match */
extern uint32_t longest_match_c(deflate_state *const s, Pos cur_match);
#if defined(UNALIGNED_OK) && BYTE_ORDER == LITTLE_ENDIAN
extern uint32_t longest_match_unaligned_16(deflate_state *const s, Pos cur_match);
#ifdef HAVE_BUILTIN_CTZ
extern uint32_t longest_match_unaligned_32(deflate_state *const s, Pos cur_match);
#endif
#if defined(UNALIGNED64_OK) && defined(HAVE_BUILTIN_CTZLL)
extern uint32_t longest_match_unaligned_64(deflate_state *const s, Pos cur_match);
#endif
#endif
#if defined(X86_SSE2) && defined(HAVE_BUILTIN_CTZ)
extern uint32_t longest_match_sse2(deflate_state *const s, Pos cur_match);
#endif
#if defined(X86_AVX2) && defined(HAVE_BUILTIN_CTZ)
extern uint32_t longest_match_avx2(deflate_state *const s, Pos cur_match);
#endif
#if defined(ARM_NEON) && defined(HAVE_BUILTIN_CTZLL)
extern uint32_t longest_match_neon(deflate_state *const s, Pos cur_match);
#endif
#ifdef POWER9
extern uint32_t longest_match_power9(deflate_state *const s, Pos cur_match);
#endif
#ifdef RISCV_RVV
extern uint32_t longest_match_rvv(deflate_state *const s, Pos cur_match);
#endif

/* longest_match_slow */
extern uint32_t longest_match_slow_c(deflate_state *const s, Pos cur_match);
#if defined(UNALIGNED_OK) && BYTE_ORDER == LITTLE_ENDIAN
extern uint32_t longest_match_slow_unaligned_16(deflate_state *const s, Pos cur_match);
extern uint32_t longest_match_slow_unaligned_32(deflate_state *const s, Pos cur_match);
#ifdef UNALIGNED64_OK
extern uint32_t longest_match_slow_unaligned_64(deflate_state *const s, Pos cur_match);
#endif
#endif
#if defined(X86_SSE2) && defined(HAVE_BUILTIN_CTZ)
extern uint32_t longest_match_slow_sse2(deflate_state *const s, Pos cur_match);
#endif
#if defined(X86_AVX2) && defined(HAVE_BUILTIN_CTZ)
extern uint32_t longest_match_slow_avx2(deflate_state *const s, Pos cur_match);
#endif
#if defined(ARM_NEON) && defined(HAVE_BUILTIN_CTZLL)
extern uint32_t longest_match_slow_neon(deflate_state *const s, Pos cur_match);
#endif
#ifdef POWER9
extern uint32_t longest_match_slow_power9(deflate_state *const s, Pos cur_match);
#endif
#ifdef RISCV_RVV
extern uint32_t longest_match_slow_rvv(deflate_state *const s, Pos cur_match);
#endif
