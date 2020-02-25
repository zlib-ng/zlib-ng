
#include "zbuild.h"
#include "deflate.h"

/*
 * Set match_start to the longest match starting at the given string and
 * return its length. Matches shorter or equal to prev_length are discarded,
 * in which case the result is equal to prev_length and match_start is garbage.
 *
 * IN assertions: cur_match is the head of the hash chain for the current
 * string (strstart) and its distance is <= MAX_DIST, and prev_length >=1
 * OUT assertion: the match length is not greater than s->lookahead
 */
unsigned LONGEST_MATCH(deflate_state *const s, IPos cur_match) {
    unsigned int strstart = s->strstart;
    const unsigned wmask = s->w_mask;
    unsigned char *window = s->window;
    register unsigned char *match;                    /* match string */
    register unsigned char *scan = window + strstart; /* current string */
    const Pos *prev = s->prev;
    unsigned chain_length;
    IPos limit;
    unsigned int len, best_len, nice_match;
    bestcmp_t scan_end, scan_start;

    /*
     * The code is optimized for HASH_BITS >= 8 and MAX_MATCH-2 multiple
     * of 16. It is easy to get rid of this optimization if necessary.
     */
    Assert(s->hash_bits >= 8 && MAX_MATCH == 258, "Code too clever");

    /*
     * Do not waste too much time if we already have a good match
     */
    best_len = s->prev_length ? s->prev_length : 1;
    chain_length = s->max_chain_length;
    if (best_len >= s->good_match)
        chain_length >>= 2;

    /*
     * Do not look for matches beyond the end of the input. This is
     * necessary to make deflate deterministic
     */
    nice_match = (unsigned int)s->nice_match > s->lookahead ? s->lookahead : (unsigned int)s->nice_match;

    /*
     * Stop when cur_match becomes <= limit. To simplify the code,
     * we prevent matches with the string of window index 0
     */
    limit = strstart > MAX_DIST(s) ? strstart - MAX_DIST(s) : 0;

    scan_start = *(bestcmp_t *)(scan);
    scan_end = *(bestcmp_t *)(scan+best_len-1);

    Assert((unsigned long)strstart <= s->window_size - MIN_LOOKAHEAD, "need lookahead");
    do {
        if (cur_match >= strstart) {
            break;
        }

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
        int cont = 1;
        do {
            match = window + cur_match;
            if (LIKELY(*(bestcmp_t *)(match+best_len-1) != scan_end) ||
                       *(bestcmp_t *)(match) != scan_start) {
                if ((cur_match = prev[cur_match & wmask]) > limit && --chain_length) {
                    continue;
                }
                cont = 0;
            }
            break;
        } while (1);

        if (!cont)
            break;

        len = COMPARE258(match, scan);
        Assert(scan+len <= window+(unsigned)(s->window_size-1), "wild scan");

        if (len > best_len) {
            s->match_start = cur_match;
            best_len = len;
            if (len >= nice_match)
                break;
            scan_end = *(bestcmp_t *)(scan+best_len-1);
        } else {
            /*
             * The probability of finding a match later if we here
             * is pretty low, so for performance it's best to
             * outright stop here for the lower compression levels
             */
            if (s->level < TRIGGER_LEVEL)
                break;
        }
    } while (--chain_length && (cur_match = prev[cur_match & wmask]) > limit);

    if ((unsigned)best_len <= s->lookahead)
        return best_len;
    return s->lookahead;
}
