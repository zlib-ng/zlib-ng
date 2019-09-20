/* memcopy.h -- inline functions to copy small data chunks.
 * For conditions of distribution and use, see copyright notice in zlib.h
 */
#ifndef MEMCOPY_H_
 #define MEMCOPY_H_

 #include "zendian.h"

/* Load 64 bits from IN and place the bytes at offset BITS in the result. */
static inline uint64_t load_64_bits(const unsigned char *in, unsigned bits) {
  uint64_t chunk;
  memcpy(&chunk, in, sizeof(chunk));

 #if BYTE_ORDER == LITTLE_ENDIAN
  return chunk << bits;
 #else
  return ZSWAP64(chunk) << bits;
 #endif
}

 #if defined(__ARM_NEON__) || defined(__ARM_NEON)
  #include <arm_neon.h>
typedef uint8x16_t inffast_chunk_t;
  #define INFFAST_CHUNKSIZE sizeof(inffast_chunk_t)
 #endif

 #if defined(X86_SSE2)
  #include <immintrin.h>
typedef __m128i inffast_chunk_t;
  #define INFFAST_CHUNKSIZE sizeof(inffast_chunk_t)
 #endif

 #ifdef INFFAST_CHUNKSIZE
/*
   Ask the compiler to perform a wide, unaligned load with an machine
   instruction appropriate for the inffast_chunk_t type.
 */
static inline inffast_chunk_t loadchunk(unsigned char const* s) {
    inffast_chunk_t c;
    memcpy(&c, s, sizeof(c));
    return c;
}

/*
   Ask the compiler to perform a wide, unaligned store with an machine
   instruction appropriate for the inffast_chunk_t type.
 */
static inline void storechunk(unsigned char* d, inffast_chunk_t c) {
    memcpy(d, &c, sizeof(c));
}

/*
   Behave like memcpy, but assume that it's OK to overwrite at least
   INFFAST_CHUNKSIZE bytes of output even if the length is shorter than this,
   that the length is non-zero, and that `from` lags `out` by at least
   INFFAST_CHUNKSIZE bytes (or that they don't overlap at all or simply that
   the distance is less than the length of the copy).

   Aside from better memory bus utilisation, this means that short copies
   (INFFAST_CHUNKSIZE bytes or fewer) will fall straight through the loop
   without iteration, which will hopefully make the branch prediction more
   reliable.
 */
static inline unsigned char* chunkcopy(unsigned char *out, unsigned char const *from, unsigned len) {
    --len;
    storechunk(out, loadchunk(from));
    out += (len % INFFAST_CHUNKSIZE) + 1;
    from += (len % INFFAST_CHUNKSIZE) + 1;
    len /= INFFAST_CHUNKSIZE;
    while (len > 0) {
        storechunk(out, loadchunk(from));
        out += INFFAST_CHUNKSIZE;
        from += INFFAST_CHUNKSIZE;
        --len;
    }
    return out;
}

/*
   Behave like chunkcopy, but avoid writing beyond of legal output.
 */
static inline unsigned char* chunkcopysafe(unsigned char *out, unsigned char const *from, unsigned len,
                                           unsigned char *safe) {
    if ((safe - out) < (ptrdiff_t)INFFAST_CHUNKSIZE) {
        if (len & 8) {
            memcpy(out, from, 8);
            out += 8;
            from += 8;
        }
        if (len & 4) {
            memcpy(out, from, 4);
            out += 4;
            from += 4;
        }
        if (len & 2) {
            memcpy(out, from, 2);
            out += 2;
            from += 2;
        }
        if (len & 1) {
            *out++ = *from++;
        }
        return out;
    }
    return chunkcopy(out, from, len);
}

/*
   Perform short copies until distance can be rewritten as being at least
   INFFAST_CHUNKSIZE.

   This assumes that it's OK to overwrite at least the first
   2*INFFAST_CHUNKSIZE bytes of output even if the copy is shorter than this.
   This assumption holds because inflate_fast() starts every iteration with at
   least 258 bytes of output space available (258 being the maximum length
   output from a single token; see inflate_fast()'s assumptions below).
 */
static inline unsigned char* chunkunroll(unsigned char *out, unsigned *dist, unsigned *len) {
    unsigned char const *from = out - *dist;
    while (*dist < *len && *dist < INFFAST_CHUNKSIZE) {
        storechunk(out, loadchunk(from));
        out += *dist;
        *len -= *dist;
        *dist += *dist;
    }
    return out;
}

static inline inffast_chunk_t chunkmemset_1(unsigned char *from) {
  #if defined(X86_SSE2)
    int8_t c;
    memcpy(&c, from, sizeof(c));
    return _mm_set1_epi8(c);
  #elif defined(__ARM_NEON__) || defined(__ARM_NEON)
    return vld1q_dup_u8(from);
  #endif
}

static inline inffast_chunk_t chunkmemset_2(unsigned char *from) {
    int16_t c;
    memcpy(&c, from, sizeof(c));
  #if defined(X86_SSE2)
    return _mm_set1_epi16(c);
  #elif defined(__ARM_NEON__) || defined(__ARM_NEON)
    return vreinterpretq_u8_s16(vdupq_n_s16(c));
  #endif
}

static inline inffast_chunk_t chunkmemset_4(unsigned char *from) {
    int32_t c;
    memcpy(&c, from, sizeof(c));
  #if defined(X86_SSE2)
    return _mm_set1_epi32(c);
  #elif defined(__ARM_NEON__) || defined(__ARM_NEON)
    return vreinterpretq_u8_s32(vdupq_n_s32(c));
  #endif
}

static inline inffast_chunk_t chunkmemset_8(unsigned char *from) {
  #if defined(X86_SSE2)
    int64_t c;
    memcpy(&c, from, sizeof(c));
    return _mm_set1_epi64x(c);
  #elif defined(__ARM_NEON__) || defined(__ARM_NEON)
    return vcombine_u8(vld1_u8(from), vld1_u8(from));
  #endif
}

  #if defined(__ARM_NEON__) || defined(__ARM_NEON)
static inline unsigned char *chunkmemset_3(unsigned char *out, unsigned char *from, unsigned dist, unsigned len) {
    uint8x8x3_t chunks;
    unsigned sz = sizeof(chunks);
    if (len < sz) {
        out = chunkunroll(out, &dist, &len);
        return chunkcopy(out, out - dist, len);
    }

    /* Load 3 bytes 'a,b,c' from FROM and duplicate across all lanes:
       chunks[0] = {a,a,a,a,a,a,a,a}
       chunks[1] = {b,b,b,b,b,b,b,b}
       chunks[2] = {c,c,c,c,c,c,c,c}. */
    chunks = vld3_dup_u8(from);

    unsigned rem = len % sz;
    len -= rem;
    while (len) {
        /* Store "a,b,c, ..., a,b,c". */
        vst3_u8(out, chunks);
        out += sz;
        len -= sz;
    }

    if (!rem)
        return out;

    /* Last, deal with the case when LEN is not a multiple of SZ. */
    out = chunkunroll(out, &dist, &rem);
    return chunkcopy(out, out - dist, rem);
}
  #endif

  #if defined(__aarch64__) || defined(_M_ARM64)
static inline unsigned char *chunkmemset_6(unsigned char *out, unsigned char *from, unsigned dist, unsigned len) {
    uint16x8x3_t chunks;
    unsigned sz = sizeof(chunks);
    if (len < sz) {
        out = chunkunroll(out, &dist, &len);
        return chunkcopy(out, out - dist, len);
    }

    /* Load 6 bytes 'ab,cd,ef' from FROM and duplicate across all lanes:
       chunks[0] = {ab,ab,ab,ab,ab,ab,ab,ab}
       chunks[1] = {cd,cd,cd,cd,cd,cd,cd,cd}
       chunks[2] = {ef,ef,ef,ef,ef,ef,ef,ef}. */
    chunks = vld3q_dup_u16((unsigned short *)from);

    unsigned rem = len % sz;
    len -= rem;
    while (len) {
        /* Store "ab,cd,ef, ..., ab,cd,ef". */
        vst3q_u16((unsigned short *)out, chunks);
        out += sz;
        len -= sz;
    }

    if (!rem)
        return out;

    /* Last, deal with the case when LEN is not a multiple of SZ. */
    out = chunkunroll(out, &dist, &rem);
    return chunkcopy(out, out - dist, rem);
}
  #endif

/* Copy DIST bytes from OUT - DIST into OUT + DIST * k, for 0 <= k < LEN/DIST. Return OUT + LEN. */
static inline unsigned char *chunkmemset(unsigned char *out, unsigned dist, unsigned len) {
    Assert(len >= sizeof(uint64_t), "chunkmemset should be called on larger chunks");
    Assert(dist > 0, "cannot have a distance 0");

    unsigned char *from = out - dist;
    inffast_chunk_t chunk;
    unsigned sz = sizeof(chunk);
    if (len < sz) {
        do {
            *out++ = *from++;
            --len;
        } while (len != 0);
        return out;
    }

    switch (dist) {
    case 1: {
        chunk = chunkmemset_1(from);
        break;
    }
    case 2: {
        chunk = chunkmemset_2(from);
        break;
    }
  #if defined(__ARM_NEON__) || defined(__ARM_NEON)
    case 3:
      return chunkmemset_3(out, from, dist, len);
  #endif
    case 4: {
        chunk = chunkmemset_4(from);
        break;
    }
  #if defined(__aarch64__) || defined(_M_ARM64)
    case 6:
        return chunkmemset_6(out, from, dist, len);
  #endif
    case 8: {
        chunk = chunkmemset_8(from);
        break;
    }
    case 16:
        memcpy(&chunk, from, sz);
        break;

    default:
        out = chunkunroll(out, &dist, &len);
        return chunkcopy(out, out - dist, len);
    }

    unsigned rem = len % sz;
    len -= rem;
    while (len) {
        memcpy(out, &chunk, sz);
        out += sz;
        len -= sz;
    }

    /* Last, deal with the case when LEN is not a multiple of SZ. */
    if (rem)
        memcpy(out, &chunk, rem);
    out += rem;
    return out;
}

static inline unsigned char* chunkmemsetsafe(unsigned char *out, unsigned dist, unsigned len, unsigned left) {
    if (left < (unsigned)(3 * INFFAST_CHUNKSIZE)) {
        while (len > 0) {
          *out = *(out - dist);
          out++;
          --len;
        }
        return out;
    }

    return chunkmemset(out, dist, len);
}

 #else /* INFFAST_CHUNKSIZE */

static inline unsigned char *copy_1_bytes(unsigned char *out, unsigned char *from) {
    *out++ = *from;
    return out;
}

static inline unsigned char *copy_2_bytes(unsigned char *out, unsigned char *from) {
    uint16_t chunk;
    unsigned sz = sizeof(chunk);
    memcpy(&chunk, from, sz);
    memcpy(out, &chunk, sz);
    return out + sz;
}

static inline unsigned char *copy_3_bytes(unsigned char *out, unsigned char *from) {
    out = copy_1_bytes(out, from);
    return copy_2_bytes(out, from + 1);
}

static inline unsigned char *copy_4_bytes(unsigned char *out, unsigned char *from) {
    uint32_t chunk;
    unsigned sz = sizeof(chunk);
    memcpy(&chunk, from, sz);
    memcpy(out, &chunk, sz);
    return out + sz;
}

static inline unsigned char *copy_5_bytes(unsigned char *out, unsigned char *from) {
    out = copy_1_bytes(out, from);
    return copy_4_bytes(out, from + 1);
}

static inline unsigned char *copy_6_bytes(unsigned char *out, unsigned char *from) {
    out = copy_2_bytes(out, from);
    return copy_4_bytes(out, from + 2);
}

static inline unsigned char *copy_7_bytes(unsigned char *out, unsigned char *from) {
    out = copy_3_bytes(out, from);
    return copy_4_bytes(out, from + 3);
}

static inline unsigned char *copy_8_bytes(unsigned char *out, unsigned char *from) {
    uint64_t chunk;
    unsigned sz = sizeof(chunk);
    memcpy(&chunk, from, sz);
    memcpy(out, &chunk, sz);
    return out + sz;
}

/* Copy LEN bytes (7 or fewer) from FROM into OUT. Return OUT + LEN. */
static inline unsigned char *copy_bytes(unsigned char *out, unsigned char *from, unsigned len) {
    Assert(len < 8, "copy_bytes should be called with less than 8 bytes");

 #ifndef UNALIGNED_OK
    while (len--) {
        *out++ = *from++;
    }
    return out;
 #else
    switch (len) {
    case 7:
        return copy_7_bytes(out, from);
    case 6:
        return copy_6_bytes(out, from);
    case 5:
        return copy_5_bytes(out, from);
    case 4:
        return copy_4_bytes(out, from);
    case 3:
        return copy_3_bytes(out, from);
    case 2:
        return copy_2_bytes(out, from);
    case 1:
        return copy_1_bytes(out, from);
    case 0:
        return out;
    default:
        Assert(0, "should not happen");
    }

    return out;
 #endif /* UNALIGNED_OK */
}

/* Copy LEN bytes (7 or fewer) from FROM into OUT. Return OUT + LEN. */
static inline unsigned char *set_bytes(unsigned char *out, unsigned char *from, unsigned dist, unsigned len) {
    Assert(len < 8, "set_bytes should be called with less than 8 bytes");

 #ifndef UNALIGNED_OK
    (void)dist;
    while (len--) {
        *out++ = *from++;
    }
    return out;
 #else
    if (dist >= len)
        return copy_bytes(out, from, len);

    switch (dist) {
    case 6:
        Assert(len == 7, "len should be exactly 7");
        out = copy_6_bytes(out, from);
        return copy_1_bytes(out, from);

    case 5:
        Assert(len == 6 || len == 7, "len should be either 6 or 7");
        out = copy_5_bytes(out, from);
        return copy_bytes(out, from, len - 5);

    case 4:
        Assert(len == 5 || len == 6 || len == 7, "len should be either 5, 6, or 7");
        out = copy_4_bytes(out, from);
        return copy_bytes(out, from, len - 4);

    case 3:
        Assert(4 <= len && len <= 7, "len should be between 4 and 7");
        out = copy_3_bytes(out, from);
        switch (len) {
        case 7:
            return copy_4_bytes(out, from);
        case 6:
            return copy_3_bytes(out, from);
        case 5:
            return copy_2_bytes(out, from);
        case 4:
            return copy_1_bytes(out, from);
        default:
            Assert(0, "should not happen");
            break;
        }

    case 2:
        Assert(3 <= len && len <= 7, "len should be between 3 and 7");
        out = copy_2_bytes(out, from);
        switch (len) {
        case 7:
            out = copy_4_bytes(out, from);
            out = copy_1_bytes(out, from);
            return out;
        case 6:
            out = copy_4_bytes(out, from);
            return out;
        case 5:
            out = copy_2_bytes(out, from);
            out = copy_1_bytes(out, from);
            return out;
        case 4:
            out = copy_2_bytes(out, from);
            return out;
        case 3:
            out = copy_1_bytes(out, from);
            return out;
        default:
            Assert(0, "should not happen");
            break;
        }

    case 1:
        Assert(2 <= len && len <= 7, "len should be between 2 and 7");
        unsigned char c = *from;
        switch (len) {
        case 7:
            memset(out, c, 7);
            return out + 7;
        case 6:
            memset(out, c, 6);
            return out + 6;
        case 5:
            memset(out, c, 5);
            return out + 5;
        case 4:
            memset(out, c, 4);
            return out + 4;
        case 3:
            memset(out, c, 3);
            return out + 3;
        case 2:
            memset(out, c, 2);
            return out + 2;
        default:
            Assert(0, "should not happen");
            break;
        }
    }
    return out;
 #endif /* UNALIGNED_OK */
}

/* Byte by byte semantics: copy LEN bytes from OUT + DIST and write them to OUT. Return OUT + LEN. */
static inline unsigned char *chunk_memcpy(unsigned char *out, unsigned char *from, unsigned len) {
    unsigned sz = sizeof(uint64_t);
    Assert(len >= sz, "chunk_memcpy should be called on larger chunks");

    /* Copy a few bytes to make sure the loop below has a multiple of SZ bytes to be copied. */
    copy_8_bytes(out, from);

    unsigned rem = len % sz;
    len /= sz;
    out += rem;
    from += rem;

    unsigned by8 = len % sz;
    len -= by8;
    switch (by8) {
    case 7:
        out = copy_8_bytes(out, from);
        from += sz;
    case 6:
        out = copy_8_bytes(out, from);
        from += sz;
    case 5:
        out = copy_8_bytes(out, from);
        from += sz;
    case 4:
        out = copy_8_bytes(out, from);
        from += sz;
    case 3:
        out = copy_8_bytes(out, from);
        from += sz;
    case 2:
        out = copy_8_bytes(out, from);
        from += sz;
    case 1:
        out = copy_8_bytes(out, from);
        from += sz;
    }

    while (len) {
        out = copy_8_bytes(out, from);
        from += sz;
        out = copy_8_bytes(out, from);
        from += sz;
        out = copy_8_bytes(out, from);
        from += sz;
        out = copy_8_bytes(out, from);
        from += sz;
        out = copy_8_bytes(out, from);
        from += sz;
        out = copy_8_bytes(out, from);
        from += sz;
        out = copy_8_bytes(out, from);
        from += sz;
        out = copy_8_bytes(out, from);
        from += sz;

        len -= 8;
    }

    return out;
}

/* Memset LEN bytes in OUT with the value at OUT - 1. Return OUT + LEN. */
static inline unsigned char *byte_memset(unsigned char *out, unsigned len) {
    unsigned sz = sizeof(uint64_t);
    Assert(len >= sz, "byte_memset should be called on larger chunks");

    unsigned char *from = out - 1;
    unsigned char c = *from;

    /* First, deal with the case when LEN is not a multiple of SZ. */
    memset(out, c, sz);
    unsigned rem = len % sz;
    len /= sz;
    out += rem;

    unsigned by8 = len % 8;
    len -= by8;
    switch (by8) {
    case 7:
        memset(out, c, sz);
        out += sz;
    case 6:
        memset(out, c, sz);
        out += sz;
    case 5:
        memset(out, c, sz);
        out += sz;
    case 4:
        memset(out, c, sz);
        out += sz;
    case 3:
        memset(out, c, sz);
        out += sz;
    case 2:
        memset(out, c, sz);
        out += sz;
    case 1:
        memset(out, c, sz);
        out += sz;
    }

    while (len) {
        /* When sz is a constant, the compiler replaces __builtin_memset with an
           inline version that does not incur a function call overhead. */
        memset(out, c, sz);
        out += sz;
        memset(out, c, sz);
        out += sz;
        memset(out, c, sz);
        out += sz;
        memset(out, c, sz);
        out += sz;
        memset(out, c, sz);
        out += sz;
        memset(out, c, sz);
        out += sz;
        memset(out, c, sz);
        out += sz;
        memset(out, c, sz);
        out += sz;
        len -= 8;
    }

    return out;
}

/* Copy DIST bytes from OUT - DIST into OUT + DIST * k, for 0 <= k < LEN/DIST. Return OUT + LEN. */
static inline unsigned char *chunk_memset(unsigned char *out, unsigned char *from, unsigned dist, unsigned len) {
    if (dist >= len)
        return chunk_memcpy(out, from, len);

    Assert(len >= sizeof(uint64_t), "chunk_memset should be called on larger chunks");

    /* Double up the size of the memset pattern until reaching the largest pattern of size less than SZ. */
    unsigned sz = sizeof(uint64_t);
    while (dist < len && dist < sz) {
        copy_8_bytes(out, from);

        out += dist;
        len -= dist;
        dist += dist;

        /* Make sure the next memcpy has at least SZ bytes to be copied.  */
        if (len < sz)
            /* Finish up byte by byte when there are not enough bytes left. */
            return set_bytes(out, from, dist, len);
    }

    return chunk_memcpy(out, from, len);
}

/* Byte by byte semantics: copy LEN bytes from FROM and write them to OUT. Return OUT + LEN. */
static inline unsigned char *chunk_copy(unsigned char *out, unsigned char *from, int dist, unsigned len) {
    if (len < sizeof(uint64_t)) {
        if (dist > 0)
            return set_bytes(out, from, dist, len);

        return copy_bytes(out, from, len);
    }

    if (dist == 1)
        return byte_memset(out, len);

    if (dist > 0)
        return chunk_memset(out, from, dist, len);

    return chunk_memcpy(out, from, len);
}
 #endif /* INFFAST_CHUNKSIZE */
#endif /* MEMCOPY_H_ */
