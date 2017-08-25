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

/* Load a short from IN and place the bytes at offset BITS in the result. */
static inline uint32_t load_short(const unsigned char *in, unsigned bits) {
    union {
        uint16_t a;
        uint8_t b[2];
    } chunk;
    MEMCPY(&chunk, in, sizeof(uint16_t));

#if BYTE_ORDER == LITTLE_ENDIAN
    uint32_t res = chunk.a;
    return res << bits;
#else
    uint32_t c0 = chunk.b[0];
    uint32_t c1 = chunk.b[1];
    c0 <<= bits;
    c1 <<= bits + 8;
    return c0 + c1;
#endif
}

static inline unsigned char *copy_1_bytes(unsigned char *out, unsigned char *from) {
    *out++ = *from;
    return out;
}

static inline unsigned char *copy_2_bytes(unsigned char *out, unsigned char *from) {
    uint16_t chunk;
    unsigned sz = sizeof(chunk);
    MEMCPY(&chunk, from, sz);
    MEMCPY(out, &chunk, sz);
    return out + sz;
}

static inline unsigned char *copy_3_bytes(unsigned char *out, unsigned char *from) {
    out = copy_1_bytes(out, from);
    return copy_2_bytes(out, from + 1);
}

static inline unsigned char *copy_4_bytes(unsigned char *out, unsigned char *from) {
    uint32_t chunk;
    unsigned sz = sizeof(chunk);
    MEMCPY(&chunk, from, sz);
    MEMCPY(out, &chunk, sz);
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
    MEMCPY(&chunk, from, sz);
    MEMCPY(out, &chunk, sz);
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
    MEMSET(out, c, sz);
    unsigned rem = len % sz;
    len /= sz;
    out += rem;
    from += rem;

    unsigned by8 = len % 8;
    len -= by8;
    switch (by8) {
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

    while (len) {
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

#endif /* MEMCOPY_H_ */
