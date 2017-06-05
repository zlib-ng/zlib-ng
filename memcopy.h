/* memcopy.h -- inline functions to copy small data chunks.
 * For conditions of distribution and use, see copyright notice in zlib.h
 */
#ifndef MEMCOPY_H_
#define MEMCOPY_H_

#if (defined(__GNUC__) || defined(__clang__))
#define MEMCPY __builtin_memcpy
#define MEMSET __builtin_memset
#else
#define MEMCPY memcpy
#define MEMSET memset
#endif
#if __STDC_VERSION__ >= 199901L
#define RESTRICT restrict
#else
#define RESTRICT
#endif

#if (defined(__ARM_NEON__) || defined(__ARM_NEON))
#  include <arm_neon.h>
typedef uint8x16_t copychunk_t;
#else
#  include <stdint.h>
typedef uint64_t copychunk_t;
#endif

#define COPYCHUNKSIZE sizeof(copychunk_t)
#define COPYCHUNK_UNROLL 8

/*
   Ask the compiler to perform a wide, unaligned load with a machine
   instruction appropriate for the copychunk_t type.
 */
static inline copychunk_t loadchunk(const unsigned char *s) {
    copychunk_t c;
    MEMCPY(&c, s, sizeof(c));
    return c;
}

/*
   Ask the compiler to perform a wide, unaligned store with a machine
   instruction appropriate for the copychunk_t type.
 */
static inline void storechunk(unsigned char *d, copychunk_t c) {
    MEMCPY(d, &c, sizeof(c));
}

static inline void copychunk(unsigned char **d, const unsigned char **s) {
    storechunk(*d, loadchunk(*s));
    *d += COPYCHUNKSIZE;
    *s += COPYCHUNKSIZE;
}

static inline unsigned char *copy_1_bytes(unsigned char * RESTRICT out, const unsigned char * RESTRICT from) {
    *out++ = *from;
    return out;
}

static inline unsigned char *copy_2_bytes(unsigned char * RESTRICT out, const unsigned char * RESTRICT from) {
    uint16_t chunk;
    unsigned sz = sizeof(chunk);
    MEMCPY(&chunk, from, sz);
    MEMCPY(out, &chunk, sz);
    return out + sz;
}

static inline unsigned char *copy_3_bytes(unsigned char * RESTRICT out, const unsigned char * RESTRICT from) {
    out = copy_1_bytes(out, from);
    return copy_2_bytes(out, from + 1);
}

static inline unsigned char *copy_4_bytes(unsigned char * RESTRICT out, const unsigned char * RESTRICT from) {
    uint32_t chunk;
    unsigned sz = sizeof(chunk);
    MEMCPY(&chunk, from, sz);
    MEMCPY(out, &chunk, sz);
    return out + sz;
}

static inline unsigned char *copy_5_bytes(unsigned char * RESTRICT out, const unsigned char * RESTRICT from) {
    out = copy_1_bytes(out, from);
    return copy_4_bytes(out, from + 1);
}

static inline unsigned char *copy_6_bytes(unsigned char * RESTRICT out, const unsigned char * RESTRICT from) {
    out = copy_2_bytes(out, from);
    return copy_4_bytes(out, from + 2);
}

static inline unsigned char *copy_7_bytes(unsigned char * RESTRICT out, const unsigned char * RESTRICT from) {
    out = copy_3_bytes(out, from);
    return copy_4_bytes(out, from + 3);
}

static inline unsigned char *copy_8_bytes(unsigned char * RESTRICT out, const unsigned char * RESTRICT from) {
    uint64_t chunk;
    unsigned sz = sizeof(chunk);
    MEMCPY(&chunk, from, sz);
    MEMCPY(out, &chunk, sz);
    return out + sz;
}

/* Copy LEN bytes (7 or fewer) from FROM into OUT. Return OUT + LEN. */
static inline unsigned char *copy_bytes(unsigned char * RESTRICT out, const unsigned char * RESTRICT from,
                                        unsigned len) {
    Assert(len < COPYCHUNKSIZE, "copy_bytes should be called for fewer than COPYCHUNKSIZE bytes");

#ifndef UNALIGNED_OK
    while (len--) {
        *out++ = *from++;
    }
    return out;
#else
    while (len > 8) {
        out = copy_8_bytes(out, from);
        from += 8;
        len -= 8;
    }
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

/* Copy LEN bytes (7 or fewer) from OUT-DIST into OUT. Return OUT + LEN. */
static inline unsigned char *copytail_lapped(unsigned char *out, unsigned dist, unsigned len) {
    const unsigned char *from = out - dist;
    Assert(len < 2 * COPYCHUNKSIZE, "copytail_lapped should be called for fewer than 2 * COPYCHUNKSIZE bytes");

#ifndef UNALIGNED_OK
    while (len--) {
        *out++ = *from++;
    }
    return out;
#else
    if (len >= 8) {
        /* TODO: remove this and extend the pattern below to handle lengths up
         * to 2 * COPYCHUNKSIZE bytes */
        while (len-- > 0) {
            *out = *(out - dist);
            out++;
        }
        return out;
    }

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
            out = copy_3_bytes(out, from);
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
            MEMSET(out, c, 7);
            return out + 7;
        case 6:
            MEMSET(out, c, 6);
            return out + 6;
        case 5:
            MEMSET(out, c, 5);
            return out + 5;
        case 4:
            MEMSET(out, c, 4);
            return out + 4;
        case 3:
            MEMSET(out, c, 3);
            return out + 3;
        case 2:
            MEMSET(out, c, 2);
            return out + 2;
        default:
            Assert(0, "should not happen");
            break;
        }
    }
    return out;
#endif /* UNALIGNED_OK */
}

/*
   Perform a memcpy-like operation, but assume that length is non-zero and that
   it's OK to overwrite at least COPYCHUNKSIZE bytes of output even if the
   length is shorter than this.

   It also guarantees that it will properly unroll the data if the distance
   between `out` and `from` is at least CHUNKCOPYSIZE, which we rely on in
   chunkcopy_relaxed().

   Aside from better memory bus utilisation, this means that short copies
   (COPYCHUNKSIZE bytes or fewer) will fall straight through the loop without
   iteration, which will hopefully make the branch prediction more reliable.
 */

static inline unsigned char *chunk_memcpy(unsigned char *out, const unsigned char *from, unsigned len) {
    --len;
    storechunk(out, loadchunk(from));
    out += (len % COPYCHUNKSIZE) + 1;
    from += (len % COPYCHUNKSIZE) + 1;
    len /= COPYCHUNKSIZE;
#if defined(COPYCHUNK_UNROLL)
    Assert(COPYCHUNK_UNROLL <= 8, "Unroll code needs more unrolling");
    switch (len % COPYCHUNK_UNROLL) {
    case 7: copychunk(&out, &from);
        /*FALLTHRU*/
    case 6: copychunk(&out, &from);
        /*FALLTHRU*/
    case 5: copychunk(&out, &from);
        /*FALLTHRU*/
    case 4: copychunk(&out, &from);
        /*FALLTHRU*/
    case 3: copychunk(&out, &from);
        /*FALLTHRU*/
    case 2: copychunk(&out, &from);
        /*FALLTHRU*/
    case 1: copychunk(&out, &from);
    }
    len /= COPYCHUNK_UNROLL;
    while (len > 0) {
        if (COPYCHUNK_UNROLL >= 1) copychunk(&out, &from);
        if (COPYCHUNK_UNROLL >= 2) copychunk(&out, &from);
        if (COPYCHUNK_UNROLL >= 3) copychunk(&out, &from);
        if (COPYCHUNK_UNROLL >= 4) copychunk(&out, &from);
        if (COPYCHUNK_UNROLL >= 5) copychunk(&out, &from);
        if (COPYCHUNK_UNROLL >= 6) copychunk(&out, &from);
        if (COPYCHUNK_UNROLL >= 7) copychunk(&out, &from);
        if (COPYCHUNK_UNROLL >= 8) copychunk(&out, &from);
        len--;
    }
#else
    while (len-- > 0) {
        storechunk(out, loadchunk(from));
        out += COPYCHUNKSIZE;
        from += COPYCHUNKSIZE;
    }
#endif
    return out;
}


/* Memset LEN bytes in OUT with the value at OUT - 1. Return OUT + LEN.
   Assume, regardless of len, that the output has enough room to write at least
   COPYCHUNKSIZE bytes. */
static inline unsigned char *chunkfill1_relaxed(unsigned char *out, unsigned len) {
    unsigned sz = COPYCHUNKSIZE;
    unsigned char c = out[-1];

    /* First, deal with the case when LEN is not a multiple of SZ. */
    MEMSET(out, c, sz);
    unsigned rem = len % sz;
    len /= sz;
    out += rem;

    switch (len % COPYCHUNK_UNROLL) {
    case 7:
        MEMSET(out, c, sz);
        out += sz;
    case 6:
        MEMSET(out, c, sz);
        out += sz;
    case 5:
        MEMSET(out, c, sz);
        out += sz;
    case 4:
        MEMSET(out, c, sz);
        out += sz;
    case 3:
        MEMSET(out, c, sz);
        out += sz;
    case 2:
        MEMSET(out, c, sz);
        out += sz;
    case 1:
        MEMSET(out, c, sz);
        out += sz;
    }
    len /= COPYCHUNK_UNROLL;

    while (len > 0) {
        /* When sz is a constant, the compiler replaces __builtin_memset with an
           inline version that does not incur a function call overhead. */
        MEMSET(out, c, sz);
        out += sz;
        MEMSET(out, c, sz);
        out += sz;
        MEMSET(out, c, sz);
        out += sz;
        MEMSET(out, c, sz);
        out += sz;
        MEMSET(out, c, sz);
        out += sz;
        MEMSET(out, c, sz);
        out += sz;
        MEMSET(out, c, sz);
        out += sz;
        MEMSET(out, c, sz);
        out += sz;
        len--;
    }

    return out;
}

/*
   Perform a memcpy-like operation, but assume that length is non-zero and that
   it's OK to overwrite at least COPYCHUNKSIZE bytes of output even if the
   length is shorter than this.

   Unlike chunk_memcpy() above, no guarantee is made regarding the behaviour of
   overlapping buffers, regardless of the distance between the pointers.  This
   is reflected in the `restrict`-qualified pointers, allowing the compiler to
   reorder loads and stores.

   Aside from better memory bus utilisation, this means that short copies
   (COPYCHUNKSIZE bytes or fewer) will fall straight through the loop without
   iteration, which will hopefully make the branch prediction more reliable.
 */
static inline unsigned char *chunkcopy_relaxed(unsigned char * RESTRICT out, const unsigned char * RESTRICT from,
                                                unsigned len) {
    return chunk_memcpy(out, from, len);
}

/*
   Like chunkcopy_relaxed, but avoid writing beyond of legal output.

   Accepts an additional pointer to the end of safe output.  A generic safe
   copy would use (out + len), but it's normally the case that the end of the
   output buffer is beyond the end of the current copy, and this can still be
   exploited.
 */
static inline unsigned char *chunkcopy_safe(unsigned char *out, const unsigned char * RESTRICT from, unsigned len,
                                           unsigned char *limit) {
    Assert(out + len <= limit, "chunk copy exceeds safety limit");
    if (limit - out < COPYCHUNKSIZE) {
        /* Output buffer is too small to write a whole COPYCHUNKSIZE chunk, so
           do things slowly and carefully. */
        return copy_bytes(out, from, len);
    }
    return chunk_memcpy(out, from, len);
}

/*
   Perform short copies until distance can be rewritten as being at least
   COPYCHUNKSIZE.

   This assumes that it's OK to overwrite at least the first 2*COPYCHUNKSIZE
   bytes of output even if the copy is shorter than this.  This assumption
   holds because inflate_fast() starts every iteration with at least 258 bytes
   of output space available (258 being the maximum length output from a single
   token; see inflate_fast()'s assumptions below).
 */
static inline unsigned char *chunkunroll_relaxed(unsigned char *out, unsigned *dist, unsigned *len) {
    const unsigned char *from = out - *dist;
    while (*dist < *len && *dist < COPYCHUNKSIZE) {
        storechunk(out, loadchunk(from));
        out += *dist;
        *len -= *dist;
        *dist += *dist;
    }
    return out;
}

/*
   Perform chunky copy within the same buffer, where the source and destination
   may potentially overlap.

   Assumes that len > 0 on entry, and that it's safe to write at least
   COPYCHUNKSIZE * 3 bytes to the output.
 */

static inline unsigned char *chunkcopy_lapped_relaxed(unsigned char *out, unsigned dist, unsigned len) {
    out = chunkunroll_relaxed(out, &dist, &len);
    return chunkcopy_relaxed(out, out - dist, len);
}

/*
   Behave like chunkcopy_lapped_relaxed, but avoid writing beyond of legal output.

   Accepts an additional pointer to the end of safe output.  A generic safe
   copy would use (out + len), but it's normally the case that the end of the
   output buffer is beyond the end of the current copy, and this can still be
   exploited.
 */
static inline unsigned char *chunkcopy_lapped_safe(unsigned char *out, unsigned dist, unsigned len,
                                                        unsigned char *limit) {
    Assert(out + len <= limit, "chunk copy exceeds safety limit");
    if (limit - out < COPYCHUNKSIZE * 2) {
        return copytail_lapped(out, dist, len);
    }
    out = chunkunroll_relaxed(out, &dist, &len);
    if (limit - out < COPYCHUNKSIZE) {
        return copy_bytes(out, out - dist, len);
    }
    return chunk_memcpy(out, out - dist, len);
}

#undef MEMCPY
#undef MEMSET
#undef RESTRICT

#endif /* MEMCOPY_H_ */
