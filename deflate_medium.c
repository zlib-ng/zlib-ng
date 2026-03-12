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

static int emit_match(deflate_state *s, struct match match) {
    int bflush = 0;
    uint32_t match_len = match.match_length;

    /* None of the below functions care about s->lookahead, so decrement it early */
    s->lookahead -= match_len;

    /* matches that are not long enough we need to emit as literals */
    if (match_len < WANT_MIN_MATCH) {
        while (match_len) {
            bflush += zng_tr_tally_lit(s, s->window[match.strstart]);
            match_len--;
            match.strstart++;
        }
        return bflush;
    }

    check_match(s, match.strstart, match.match_start, match_len);

    bflush += zng_tr_tally_dist(s, match.strstart - match.match_start, match_len - STD_MIN_MATCH);
    return bflush;
}

/* insert_match assumes: s->lookahead > match.match_length + WANT_MIN_MATCH */
static void insert_match(deflate_state *s, struct match match) {
    uint32_t match_len = match.match_length;
    uint32_t strstart = match.strstart;

    /* matches that are not long enough we need to emit as literals */
    if (LIKELY(match_len < WANT_MIN_MATCH)) {
        strstart++;
        match_len--;
        if (UNLIKELY(match_len > 0)) {
            if (strstart >= match.orgstart) {
                if (strstart + match_len - 1 >= match.orgstart) {
                    insert_string(s, strstart, match_len);
                } else {
                    insert_string(s, strstart, match.orgstart - strstart + 1);
                }
            }
        }
        return;
    }

    /* Insert new strings in the hash table only if the match length
     * is not too large. This saves time but degrades compression.
     */
    if (match_len <= 16 * s->max_insert_length && s->lookahead >= WANT_MIN_MATCH) {
        match_len--; /* string at strstart already in table */
        strstart++;

        if (LIKELY(strstart >= match.orgstart)) {
            if (LIKELY(strstart + match_len - 1 >= match.orgstart)) {
                insert_string(s, strstart, match_len);
            } else {
                insert_string(s, strstart, match.orgstart - strstart + 1);
            }
        } else if (match.orgstart < strstart + match_len) {
            insert_string(s, match.orgstart, strstart + match_len - match.orgstart);
        }
    } else {
        strstart += match_len;
        quick_insert_string(s, strstart + 2 - STD_MIN_MATCH);

        /* If lookahead < WANT_MIN_MATCH, ins_h is garbage, but it does not
         * matter since it will be recomputed at next deflate call.
         */
    }
}

Z_FORCEINLINE static struct match find_best_match(deflate_state *s, uint32_t hash_head) {
    struct match m;
    int64_t dist;

    m.strstart = (uint16_t)s->strstart;
    m.orgstart = m.strstart;

    dist = (int64_t)s->strstart - hash_head;
    if (dist <= MAX_DIST(s) && dist > 0 && hash_head != 0) {
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

/* fizzle_matches assumes:
 * - current->match_length > 1
 * - (current->match_length - 1) <= next->match_start
 * - (current->match_length - 1) <= next->strstart
 */
static void fizzle_matches(deflate_state *s, struct match *current, struct match *next) {
    unsigned char *window = s->window;
    unsigned char *match, *orig;
    struct match c, n;
    int changed = 0;
    Pos limit;

    match = window - current->match_length + 1 + next->match_start;
    orig  = window - current->match_length + 1 + next->strstart;

    /* quick exit check.. if this fails then don't bother with anything else */
    if (LIKELY(*match != *orig))
        return;

    c = *current;
    n = *next;

    /* step one: try to move the "next" match to the left as much as possible */
    limit = next->strstart > MAX_DIST(s) ? next->strstart - (Pos)MAX_DIST(s) : 0;

    match = window + n.match_start - 1;
    orig = window + n.strstart - 1;

    while (*match == *orig) {
        if (UNLIKELY(c.match_length < 1))
            break;
        if (UNLIKELY(n.strstart <= limit))
            break;
        if (UNLIKELY(n.match_length >= 256))
            break;
        if (UNLIKELY(n.match_start <= 1))
            break;

        n.strstart--;
        n.match_start--;
        n.match_length++;
        c.match_length--;
        match--;
        orig--;
        changed++;
    }

    if (changed && c.match_length <= 1 && n.match_length != 2) {
        n.orgstart++;
        *current = c;
        *next = n;
    } else {
        return;
    }
}

Z_INTERNAL block_state deflate_medium(deflate_state *s, int flush) {
    /* Align the first struct to start on a new cacheline, this allows us to fit both structs in one cacheline */
    ALIGNED_(16) struct match current_match = {0};
                 struct match next_match = {0};

    /* For levels below 5, don't check the next position for a better match */
    int early_exit = s->level < 5;

    for (;;) {
        uint32_t hash_head = 0;    /* head of the hash chain */
        int bflush = 0;       /* set if current block must be flushed */

        /* Make sure that we always have enough lookahead, except
         * at the end of the input file. We need STD_MAX_MATCH bytes
         * for the next match, plus WANT_MIN_MATCH bytes to insert the
         * string following the next current_match.
         */
        if (s->lookahead < MIN_LOOKAHEAD) {
            PREFIX(fill_window)(s);
            if (s->lookahead < MIN_LOOKAHEAD && flush == Z_NO_FLUSH) {
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
            hash_head = 0;
            if (s->lookahead >= WANT_MIN_MATCH) {
                hash_head = quick_insert_string(s, s->strstart);
            }

            current_match = find_best_match(s, hash_head);
        }

        if (LIKELY(s->lookahead > (unsigned int)(current_match.match_length + WANT_MIN_MATCH)))
            insert_match(s, current_match);

        /* now, look ahead one */
        if (LIKELY(!early_exit && s->lookahead > MIN_LOOKAHEAD && (uint32_t)(current_match.strstart + current_match.match_length) < (s->window_size - MIN_LOOKAHEAD))) {
            s->strstart = current_match.strstart + current_match.match_length;
            hash_head = quick_insert_string(s, s->strstart);

            next_match = find_best_match(s, hash_head);

            uint32_t tmp_cmatch_len_sub = current_match.match_length - 1;
            if (tmp_cmatch_len_sub
                     && next_match.match_length >= WANT_MIN_MATCH
                     && tmp_cmatch_len_sub <= next_match.match_start
                     && tmp_cmatch_len_sub <= next_match.strstart) {
                fizzle_matches(s, &current_match, &next_match);
            }

            s->strstart = current_match.strstart;
        } else {
            next_match.match_length = 0;
        }

        /* now emit the current match */
        bflush = emit_match(s, current_match);

        /* move the "cursor" forward */
        s->strstart += current_match.match_length;

        if (UNLIKELY(bflush))
            FLUSH_BLOCK(s, 0);
    }
    s->insert = s->strstart < (STD_MIN_MATCH - 1) ? s->strstart : (STD_MIN_MATCH - 1);
    if (flush == Z_FINISH) {
        FLUSH_BLOCK(s, 1);
        return finish_done;
    }
    if (UNLIKELY(s->sym_next))
        FLUSH_BLOCK(s, 0);

    return block_done;
}
#endif
