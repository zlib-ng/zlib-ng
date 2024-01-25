/* power_features.h -- check for POWER CPU features
 * Copyright (C) 2020 Matheus Castanho <msc@linux.ibm.com>, IBM
 * Copyright (C) 2021 Mika T. Lindqvist <postmaster@raasu.org>
 * For conditions of distribution and use, see copyright notice in zlib.h
 */

#ifndef POWER_H_
#define POWER_H_

struct power_cpu_features {
    int has_altivec;
    int has_arch_2_07;
    int has_arch_3_00;
};

void Z_INTERNAL power_check_features(struct power_cpu_features *features);

#ifdef CPU_FEATURES_H_

#ifdef PPC_VMX
extern uint32_t adler32_vmx(uint32_t adler, const uint8_t *buf, size_t len);
#  ifdef DEFLATE_H_
    extern void slide_hash_vmx(deflate_state *s);
#  endif
#endif

#ifdef POWER8_VSX
extern uint32_t adler32_power8(uint32_t adler, const uint8_t *buf, size_t len);
extern uint32_t chunksize_power8(void);
extern uint8_t* chunkmemset_safe_power8(uint8_t *out, unsigned dist, unsigned len, unsigned left);
extern uint32_t crc32_power8(uint32_t crc, const uint8_t *buf, size_t len);
extern void     inflate_fast_power8(PREFIX3(stream) *strm, uint32_t start);

#  ifdef DEFLATE_H_
    extern void slide_hash_power8(deflate_state *s);
#  endif
#endif

#ifdef POWER9
extern uint32_t compare256_power9(const uint8_t *src0, const uint8_t *src1);
#  ifdef DEFLATE_H_
    extern uint32_t longest_match_power9(deflate_state *const s, Pos cur_match);
    extern uint32_t longest_match_slow_power9(deflate_state *const s, Pos cur_match);
#  endif
#endif

#endif

#endif /* POWER_H_ */
