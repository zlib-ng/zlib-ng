/*
 * Set match_start to the longest match starting at the given string and
 * return its length. Matches shorter or equal to prev_length are discarded,
 * in which case the result is equal to prev_length and match_start is garbage.
 *
 * IN assertions: cur_match is the head of the hash chain for the current
 * string (strstart) and its distance is <= MAX_DIST, and prev_length >=1
 * OUT assertion: the match length is not greater than s->lookahead
 */

#include "zbuild.h"
#include "deflate.h"

#if (defined(UNALIGNED_OK) && MAX_MATCH == 258)
#  define std2_longest_match
#else
#  define std1_longest_match
#endif

#ifdef std1_longest_match

/*
 * Standard longest_match
 *
 */
ZLIB_INTERNAL unsigned longest_match(deflate_state *const s, IPos cur_match) {
    const unsigned wmask = s->w_mask;
    const Pos *prev = s->prev;

    unsigned chain_length;
    IPos limit;
    unsigned int len, best_len, nice_match;
    unsigned char *scan, *match, *strend, scan_end, scan_end1;

    /*
     * The code is optimized for HASH_BITS >= 8 and MAX_MATCH-2 multiple
     * of 16. It is easy to get rid of this optimization if necessary.
     */
    Assert(s->hash_bits >= 8 && MAX_MATCH == 258, "Code too clever");

    /*
     * Do not waste too much time if we already have a good match
     */
    best_len = s->prev_length;
    chain_length = s->max_chain_length;
    if (best_len >= s->good_match)
        chain_length >>= 2;

    /*
     * Do not looks for matches beyond the end of the input. This is
     * necessary to make deflate deterministic
     */
    nice_match = (unsigned int)s->nice_match > s->lookahead ? s->lookahead : s->nice_match;

    /*
     * Stop when cur_match becomes <= limit. To simplify the code,
     * we prevent matches with the string of window index 0
     */
    limit = s->strstart > MAX_DIST(s) ? s->strstart - MAX_DIST(s) : 0;

    scan = s->window + s->strstart;
    strend = s->window + s->strstart + MAX_MATCH;
    scan_end1 = scan[best_len-1];
    scan_end = scan[best_len];

    Assert((unsigned long)s->strstart <= s->window_size - MIN_LOOKAHEAD, "need lookahead");
    do {
        if (cur_match >= s->strstart) {
          break;
        }
        match = s->window + cur_match;

        /*
         * Skip to next match if the match length cannot increase
         * or if the match length is less than 2. Note that the checks
         * below for insufficient lookahead only occur occasionally
         * for performance reasons. Therefore uninitialized memory
         * will be accessed and conditional jumps will be made that
         * depend on those values. However the length of the match
         * is limited to the lookahead, so the output of deflate is not
         * affected by the uninitialized values.
         */
        if (match[best_len] != scan_end ||
            match[best_len-1] != scan_end1 ||
            *match != *scan ||
            *++match != scan[1])
            continue;

        /*
         * The check at best_len-1 can be removed because it will
         * be made again later. (This heuristic is not always a win.)
         * It is not necessary to compare scan[2] and match[2] since
         * they are always equal when the other bytes match, given
         * that the hash keys are equal and that HASH_BITS >= 8.
         */
        scan += 2;
        match++;
        Assert(*scan == *match, "match[2]?");

        /*
         * We check for insufficient lookahead only every 8th
         * comparision; the 256th check will be made at strstart + 258.
         */
        do {
        } while (*++scan == *++match && *++scan == *++match &&
             *++scan == *++match && *++scan == *++match &&
             *++scan == *++match && *++scan == *++match &&
             *++scan == *++match && *++scan == *++match &&
             scan < strend);

        Assert(scan <= s->window+(unsigned int)(s->window_size-1), "wild scan");

        len = MAX_MATCH - (int)(strend - scan);
        scan = strend - MAX_MATCH;

        if (len > best_len) {
            s->match_start = cur_match;
            best_len = len;
            if (len >= nice_match)
                break;
            scan_end1 = scan[best_len-1];
            scan_end = scan[best_len];
        } else {
            /*
             * The probability of finding a match later if we here
             * is pretty low, so for performance it's best to
             * outright stop here for the lower compression levels
             */
            if (s->level < TRIGGER_LEVEL)
                break;
        }
    } while ((cur_match = prev[cur_match & wmask]) > limit && --chain_length);

    if ((unsigned int)best_len <= s->lookahead)
        return best_len;
    return s->lookahead;
}
#endif

#ifdef std2_longest_match
/*
 * UNALIGNED_OK longest_match
 *
 */
ZLIB_INTERNAL unsigned longest_match(deflate_state *const s, IPos cur_match) {
    unsigned chain_length = s->max_chain_length;/* max hash chain length */
    unsigned char *scan = s->window + s->strstart; /* current string */
    unsigned int len;                  /* length of current match */
    unsigned int best_len = s->prev_length;     /* best match length so far */
    Assert(best_len > 1, "no single char match");

    unsigned int nice_match = s->nice_match;    /* stop if match long enough */
    int offset = 0;                             /* offset of current hash chain */
    IPos limit_base = s->strstart > (IPos)MAX_DIST(s) ?
        s->strstart - (IPos)MAX_DIST(s) : NIL;
    /*?? are MAX_DIST matches allowed ?! */
    IPos limit = limit_base;                    /* limit will be limit_base+offset */
    /* Stop when cur_match becomes <= limit. To simplify the code,
     * we prevent matches with the string of window index 0.
     */
    unsigned char *match_base = s->window;              /* s->window - offset */
    unsigned char *match_base2 = match_base + best_len - 1;
    /* "offset search" mode will speedup only with large chain_length; plus it is
     * impossible for deflate_fast(), because this function does not perform
     * INSERT_STRING() for matched strings (hash table have "holes"). deflate_fast()'s
     * max_chain is <= 32, deflate_slow() max_chain > 64 starting from compression
     * level 6; so - offs0_mode==true only for deflate_slow() with level >= 6)
     */
    int offs0_mode = chain_length < 64;         /* bool, mode with offset==0 */
    Pos *prev = s->prev;                        /* lists of the hash chains */
    unsigned int wmask = s->w_mask;

    unsigned char *strend = s->window + s->strstart + MAX_MATCH - 1;
        /* points to last byte for maximal-length scan */

    uint16_t scan_end = 0;
    MEMCPY(&scan_end, scan + best_len - 1, sizeof(scan_end));


    /* The code is optimized for HASH_BITS >= 8 and MAX_MATCH-2 multiple of 16.
     * It is easy to get rid of this optimization if necessary.
     */
    Assert(s->hash_bits >= 8 && MAX_MATCH == 258, "Code too clever");

    /* Do not waste too much time if we already have a good match: */
    if (s->prev_length >= s->good_match)
      chain_length >>= 2;

    /* Do not look for matches beyond the end of the input. This is necessary
     * to make deflate deterministic.
     */
    if ((unsigned int)nice_match > s->lookahead)
      nice_match = s->lookahead;
    Assert((unsigned long)s->strstart <= s->window_size-MIN_LOOKAHEAD, "need lookahead");

    if (best_len >= MIN_MATCH) {
      /* We're continuing search (lazy evaluation).
       * Note: for deflate_fast best_len is always MIN_MATCH-1 here.
       */
      /* Find a most distant chain starting from scan with index=1 (index=0
       * corresponds to cur_match). Note: we cannot use s->prev[strstart+1,...]
       * immediately, because these strings are not yet inserted into hash table
       * yet.
       */
      unsigned int hash = 0;
      UPDATE_HASH(s, hash, scan[1]);
      UPDATE_HASH(s, hash, scan[2]);
      int i = 3;
      for (; i <= best_len; i++) {
        UPDATE_HASH(s, hash, scan[i]);
        /* If we're starting with best_len >= 3, we can use offset search. */
        IPos pos = s->head[hash];
        if (pos < cur_match) {
          offset = i - 2;
          cur_match = pos;
        }
      }
      /* Update variables to corresponding offset. */
      limit = limit_base + offset;
      if (cur_match <= limit)
        goto break_matching;
      match_base -= offset;
      match_base2 -= offset;
    }

#define NEXT_CHAIN                                                             \
  do {                                                                         \
    cur_match = prev[cur_match & wmask];                                       \
    if (cur_match <= limit)                                                    \
      goto break_matching;                                                     \
    if (--chain_length == 0)                                                   \
      goto break_matching;                                                     \
    Assert(cur_match - offset < s->strstart, "no future");                     \
  } while (0)

    uint16_t scan_start = 0;    /* 1st 2 bytes of scan */
    uint32_t scan_start32 = 0;  /* 1st 4 bytes of scan */
    MEMCPY(&scan_start, scan, sizeof(scan_start));
    MEMCPY(&scan_start32, scan, sizeof(scan_start32));
    do {
      /* Find a candidate for matching using hash table. Jump over hash
       * table chain until we'll have a partial match. Doing "break" when
       * matched, and NEXT_CHAIN to try different place.
       */
      if (best_len < MIN_MATCH) {
        /* Here we have best_len < MIN_MATCH, and this means, that
         * offset == 0. So, we need to check only first 2 bytes of
         * match (remaining 1 byte will be the same, because of nature of
         * hash function)
         */
        for (;;) {
          uint16_t chunk = 0;
          MEMCPY(&chunk, match_base + cur_match, sizeof(chunk));
          if (chunk == scan_start)
            break;
          NEXT_CHAIN;
        }
      } else if (best_len > MIN_MATCH) {
        /* current len > MIN_MATCH (>= 4 bytes); compare 1st 4 bytes and last 2
           bytes */
        for (;;) {
          uint32_t chunk32 = 0;
          uint16_t chunk = 0;

          MEMCPY(&chunk32, match_base + cur_match, sizeof(chunk32));
          MEMCPY(&chunk, match_base2 + cur_match, sizeof(chunk));
          if (chunk == scan_end && chunk32 == scan_start32)
            break;
          NEXT_CHAIN;
        }
      } else {
        /* current len is exactly MIN_MATCH (3 bytes); compare 4 bytes */
        for (;;) {
          uint32_t chunk32 = 0;
          MEMCPY(&chunk32, match_base + cur_match, sizeof(chunk32));
          if (chunk32 == scan_start32)
            break;
          NEXT_CHAIN;
        }
      }

      /* Skip 1 byte */
      unsigned char *match = match_base + cur_match + 1;
      scan++;
        do {
        } while (*++scan == *++match && *++scan == *++match &&
             *++scan == *++match && *++scan == *++match &&
             *++scan == *++match && *++scan == *++match &&
             *++scan == *++match && *++scan == *++match &&
             scan < strend);

      Assert(scan <= s->window + (unsigned)(s->window_size - 1), "wild scan");
      if (*scan == *match)
        scan++;

      len = (MAX_MATCH - 1) - (int)(strend - scan);
      scan = strend - (MAX_MATCH - 1);

      if (len > best_len) {
        /* new string is longer than previous - remember it */
        s->match_start = cur_match - offset;
        best_len = len;
        if (len >= nice_match)
          break;

        MEMCPY(&scan_end, scan + best_len - 1, sizeof(scan_end));

        /* look for better string offset */
        /*!! TODO: check if "cur_match - offset + len < s->strstart" condition
         * is really needed - it restricts RLE-like compression */
        if (len > MIN_MATCH && cur_match - offset + len < s->strstart &&
            !offs0_mode) {
          /* NOTE: if deflate algorithm will perform INSERT_STRING for
           *   a whole scan (not for scan[0] only), can remove
           *   "cur_match + len < s->strstart" limitation and replace it
           *   with "cur_match + len < strend".
           */

          /* go back to offset 0 */
          cur_match -= offset;
          offset = 0;

          IPos next_pos = cur_match;
          int i = 0;
          for (; i <= len - MIN_MATCH; i++) {
            IPos pos = prev[(cur_match + i) & wmask];
            if (pos < next_pos) {
              /* this hash chain is more distant, use it */
              if (pos <= limit_base + i)
                goto break_matching;
              next_pos = pos;
              offset = i;
            }
          }
          /* Switch cur_match to next_pos chain */
          cur_match = next_pos;

          /* Try hash head at len-(MIN_MATCH-1) position to see if we could get
           * a better cur_match at the end of string. Using (MIN_MATCH-1) lets
           * us to include one more byte into hash - the byte which will be
           * checked in main loop now, and which allows to grow match by 1.
           */
          uint32_t hash = 0;
          unsigned char *scanend = scan + len - MIN_MATCH + 1;
          UPDATE_HASH(s, hash, scanend[0]);
          UPDATE_HASH(s, hash, scanend[1]);
          UPDATE_HASH(s, hash, scanend[2]);
          IPos pos = s->head[hash];
          if (pos < cur_match) {
            offset = len - MIN_MATCH + 1;
            if (pos <= limit_base + offset)
              goto break_matching;
            cur_match = pos;
          }

          /* update offset-dependent vars */
          limit = limit_base + offset;
          match_base = s->window - offset;
          match_base2 = match_base + best_len - 1;
          continue;
        } else {
          /* There's no way to change offset - simply update match_base2 for new
             best_len (this is similar to what original algorithm does). */
          match_base2 = match_base + best_len - 1;
        }
      }

      /* follow hash chain */
      cur_match = prev[cur_match & wmask];
    } while (cur_match > limit && --chain_length != 0);

 break_matching:
    if ((unsigned int)best_len <= s->lookahead)
        return (unsigned int)best_len;
    return s->lookahead;
}
#endif
