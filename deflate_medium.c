/* deflate_medium.c -- The deflate_medium deflate strategy
 *
 * Copyright (C) 2013 Intel Corporation. All rights reserved.
 * Authors:
 *  Arjan van de Ven    <arjan@linux.intel.com>
 *
 * For conditions of distribution and use, see copyright notice in zlib.h
 */
#ifndef NO_MEDIUM_STRATEGY
#include "zbuild.h"
#include "deflate.h"
#include "deflate_p.h"
#include "functable.h"
#include "insert_string_p.h"

struct match {
    uint16_t match_start;
    uint16_t match_length;
    uint16_t strstart;
    uint16_t orgstart;
};

/* insert_match assumes:
 * - s->lookahead > match.match_length + WANT_MIN_MATCH
 * - match_len >= WANT_MIN_MATCH
 */
static void insert_match(deflate_state *s, unsigned char *Z_RESTRICT window, struct match match, const uint32_t max_len) {
    uint32_t start;
    uint32_t match_len = match.match_length;
    uint32_t strstart = match.strstart + 1; // string at strstart already in table
    uint32_t end = strstart + match_len - 1;

    /* Insert new strings in the hash table only if the match length
     * is not too large. This saves time but degrades compression.
     */
    if (UNLIKELY(match_len > max_len)) {
        // For too long matches, insert only the tail position.
        start = end - 1;
    } else {
        start = strstart;
    }

    if (UNLIKELY(start < (uint32_t)match.orgstart))
        start = match.orgstart;

    insert_knuth_batch(s, window, start, end - start);
}

Z_FORCEINLINE static struct match find_best_match(deflate_state *s, uint32_t hash_head) {
    struct match m;
    int32_t dist;

    m.strstart = (uint16_t)s->strstart;
    m.orgstart = m.strstart;

    dist = (int32_t)s->strstart - (int32_t)hash_head;
    if (dist <= (int32_t)MAX_DIST(s) && dist > 0 && hash_head != 0) {
        /* To simplify the code, we prevent matches with the string
         * of window index 0 (in particular we have to avoid a match
         * of the string with itself at the start of the input file).
         */
        m.match_length = (uint16_t)FUNCTABLE_CALL(longest_match)(s, hash_head);
        m.match_start = (uint16_t)s->match_start;
        if (UNLIKELY(m.match_length < WANT_MIN_MATCH))
            m.match_length = 1;
        if (UNLIKELY(m.match_start >= m.strstart)) {
            /* this can happen due to some restarts */
            m.match_length = 1;
        }
    } else {
        /* Set up the match to be a 1 byte literal */
        m.match_start = 0;
        m.match_length = 1;
    }

    return m;
}

/* fizzle_matches investigates whether next_match (which starts after current_match) can grow backwards
 * to absorb current_match entirely, or reduce it to a single literal.
 * This occurs because next_match points to a different historical dictionary position, allowing it to discover
 * a better matching alignment that current_match bypassed due to the medium-strategy skipping positions.
 *
 * fizzle_matches assumes:
 * - current_match.match_length > 1
 * - current_match.match_length - 1 <= next->match_start
 * - current_match.match_length - 1 <= next->strstart
 * - next_match.match_length >= WANT_MIN_MATCH
 *
 * fuzzle_matches returns:
 * - If successful, current and next are returned modified.
 *   - current_match.match_length is then either 0, 1
 */
static void fizzle_matches(deflate_state *s, unsigned char *Z_RESTRICT window, struct match *Z_RESTRICT current, struct match *Z_RESTRICT next) {
    unsigned char *match = window + next->match_start + 1 - current->match_length;
    unsigned char *orig  = window + next->strstart + 1 - current->match_length;

    /* quick exit check.. if this fails then don't bother with anything else */
    if (LIKELY(*match != *orig))
        return;

    int32_t limit = (int32_t)next->strstart > (int32_t)MAX_DIST(s) ? (int32_t)next->strstart - (int32_t)MAX_DIST(s) : 0;

    // Steps needed to successfully fizzle match
    uint16_t need = current->match_length - 1;

    // Protect next->strstart from moving past maximum distance
    int32_t max_steps_to_limit = (int32_t)next->strstart - limit;

    // Protect next->match_length from exceeding 256
    int32_t max_growth_allowed = 256 - (int32_t)next->match_length;

    // Protect next->match_start from going too far back
    int32_t max_steps_to_history = (int32_t)next->match_start - 1;

    // steps is the max number of backward steps allowed for each limitation
    int32_t steps1 = MIN((int32_t)current->match_length, max_steps_to_limit);
    int32_t steps2 = MIN(max_growth_allowed, max_steps_to_history);
    int32_t steps = MIN(steps1, steps2);

    // If we can't possibly fizzle the current match out, return early
    if (LIKELY(steps < (int32_t)need))
        return;

    // The quick exit above already checked the first byte of that range.
    // Compare the whole range here.
    if (LIKELY(memcmp(match, orig, (size_t)need) != 0))
        return;

    // Check whether the final extra backward byte is also possible.
    // This decides whether the current match becomes length 1 or 0.
    int extra_byte_ok = (steps == (int32_t)current->match_length);

    // Update variables, reduces current->match_length to 1.
    next->match_start  = next->match_start - need;
    next->strstart     = next->strstart - need;
    next->match_length = next->match_length + need;
    next->orgstart++;
    current->match_length = 1;

    // If every constraint allowed one more backward byte, test it.
    // If it matches, the current match is fully absorbed and becomes length 0.
    if (extra_byte_ok) {
        match = window + next->match_start - 1;
        orig  = window + next->strstart - 1;

        if (*match == *orig) {
            next->match_start--;
            next->strstart--;
            next->match_length++;
            current->match_length = 0;
        }
    }
}

Z_INTERNAL block_state deflate_medium(deflate_state *s, int flush) {
    /* Align the first struct to start on a new cacheline, this allows us to fit both structs in one cacheline */
    ALIGNED_(16) struct match current_match = {0};
                 struct match next_match = {0};
    unsigned char *window = s->window;
    uint32_t window_end = s->window_size - MIN_LOOKAHEAD;
    uint32_t max_len = 16 * s->max_insert_length;

    /* For levels below 5, don't check the next position for a better match */
    int early_exit = s->level < 5;

    for (;;) {
        int bflush = 0;       /* set if current block must be flushed */
        uint32_t curr_match_len;

        /* Make sure that we always have enough lookahead, except
         * at the end of the input file. We need STD_MAX_MATCH bytes
         * for the next match, plus WANT_MIN_MATCH bytes to insert the
         * string following the next current_match.
         */
        if (UNLIKELY(s->lookahead < MIN_LOOKAHEAD)) {
            PREFIX(fill_window)(s);
            if (UNLIKELY(s->lookahead < MIN_LOOKAHEAD && flush == Z_NO_FLUSH)) {
                return need_more;
            }
            if (UNLIKELY(s->lookahead == 0))
                break; /* flush the current block */
            next_match.match_length = 0;
        }

        /* Insert the string window[strstart .. strstart+2] in the
         * dictionary, and set hash_head to the head of the hash chain:
         */

        /* If we already have a future match from a previous round, just use that */
        if (!early_exit && next_match.match_length > 0) {
            current_match = next_match;
            next_match.match_length = 0;
        } else {
            uint32_t hash_head = 0;   /* head of the hash chain */
            if (LIKELY(s->lookahead >= WANT_MIN_MATCH)) {
                hash_head = insert_knuth(s, window, s->strstart);
            }

            current_match = find_best_match(s, hash_head);
        }
        curr_match_len = current_match.match_length;

        if (curr_match_len >= WANT_MIN_MATCH && s->lookahead > (unsigned int)(curr_match_len + WANT_MIN_MATCH )) {
            insert_match(s, window, current_match, max_len);
        }

        /* now, look ahead one */
        if (LIKELY(!early_exit && s->lookahead > MIN_LOOKAHEAD && (uint32_t)(current_match.strstart + curr_match_len) < window_end)) {
            s->strstart = current_match.strstart + curr_match_len;
            uint32_t hash_head = insert_knuth(s, window, s->strstart);

            next_match = find_best_match(s, hash_head);

            uint32_t tmp_cmatch_len_sub = curr_match_len - 1;
            if (tmp_cmatch_len_sub
                     && next_match.match_length >= WANT_MIN_MATCH
                     && tmp_cmatch_len_sub <= next_match.match_start) {
                fizzle_matches(s, window, &current_match, &next_match);
                curr_match_len = current_match.match_length;
            }

            s->strstart = current_match.strstart;
            if (curr_match_len == 0) {
                /* If current match fizzled out, jump to next loop iteration */
                continue;
            }
        } else {
            next_match.match_length = 0;
        }

        /* now emit the current match */
        s->lookahead -= curr_match_len;
        if (LIKELY(curr_match_len == 1)) {
            /* matches shorter than WANT_MIN_MATCH are set to 1, we need to emit these as literals */
            bflush = zng_tr_tally_lit(s, window[current_match.strstart]);
        } else {
            check_match(s, current_match.strstart, current_match.match_start, curr_match_len);
            bflush = zng_tr_tally_dist(s, current_match.strstart - current_match.match_start, curr_match_len - STD_MIN_MATCH);
        }

        /* move the "cursor" forward */
        s->strstart += curr_match_len;

        if (UNLIKELY(bflush))
            FLUSH_BLOCK(s, window, 0);
    }
    s->insert = s->strstart < (STD_MIN_MATCH - 1) ? s->strstart : (STD_MIN_MATCH - 1);
    if (flush == Z_FINISH) {
        FLUSH_BLOCK(s, window, 1);
        return finish_done;
    }
    if (UNLIKELY(s->sym_next))
        FLUSH_BLOCK(s, window, 0);

    return block_done;
}
#endif
