/* deflate_extreme.c -- The deflate_extreme deflate strategy
 *
 * Copyright (C) 2026 Hans Kristian Rosbach
 *
 * For conditions of distribution and use, see copyright notice in zlib.h
 */

#include "zbuild.h"
#include "deflate.h"
#include "deflate_p.h"
#include "functable.h"
#include "insert_string_p.h"

/* A match candidate: either a single literal (match_length == 1) or a match of
 * length >= STD_MIN_MATCH starting at strstart that matches history data at
 * match_start. */
struct match {
    uint32_t match_start;
    uint32_t match_length;
    uint32_t strstart;
};

/* Fixed-Huffman extra-bit tables, mirroring trees.h (RFC1951 C.4). Length
 * symbols 257-285 carry 7 bits up to symbol 279 and 8 bits above; all
 * distance codes carry 5 bits. */
static const uint32_t xtra_len[29] = {
    0,0,0,0,0,0,0,0,1,1,1,1,2,2,2,2,3,3,3,3,4,4,4,4,5,5,5,5,0
};
static const uint32_t xtra_dist[30] = {
    0,0,0,0,1,1,2,2,3,3,4,4,5,5,6,6,7,7,8,8,9,9,10,10,11,11,12,12,13,13
};

/* Bit cost of a match token with the given length (>= STD_MIN_MATCH) and
 * distance (>= 1) under the fixed Huffman code. */
Z_FORCEINLINE static uint32_t match_cost(uint32_t len, uint32_t dist) {
    const uint32_t lcode = zng_length_code[len - STD_MIN_MATCH];
    return (lcode >= 23 ? 8u : 7u) + xtra_len[lcode] + 5u + xtra_dist[d_code(dist - 1)];
}

/* Bit cost of a literal byte under the fixed Huffman code. */
Z_FORCEINLINE static uint32_t lit_cost(uint32_t c) {
    return c < 144 ? 8u : 9u;
}

/* Imputed bit cost of one input byte left to be encoded by later decisions.
 * Used to compare candidate sequences of different advance: a sequence that
 * advances farther is credited with this many bits per skipped byte, so a
 * good match beats "emit literals and re-decide inside the match region"
 * (which would otherwise win on explicit bit cost alone), while a
 * long-distance 3-byte match (~25 bits) still loses to its three literals. */
#define TAIL_BITS_PER_BYTE 5u

/* Compute the rolling hash key of window[p..p+2] directly, without touching
 * s->ins_h. Valid because UPDATE_HASH_ROLL shifts a 15-bit state by 5 each
 * step: after three folds the seed has shifted out entirely, so the key of
 * position p is always the fresh fold of its first three bytes regardless of
 * where ins_h was last primed. */
Z_FORCEINLINE static uint32_t roll_lookup(const unsigned char *window, uint32_t p) {
    uint32_t h = window[p];
    UPDATE_HASH_ROLL(h, window[p + 1]);
    UPDATE_HASH_ROLL(h, window[p + 2]);
    return h;
}

/* Search the hash chain headed by hash_head for the best match at s->strstart.
 * Returns a candidate with match_length == 1 when no usable match is found.
 * Callers searching ahead must set s->strstart to the probe position first,
 * since longest_match derives its bounds from it. */
Z_FORCEINLINE static struct match find_best_match(deflate_state *s, uint32_t hash_head, int32_t max_dist) {
    struct match res;
    int32_t dist;

    /* Initialize the result to be a 1 byte literal */
    res.match_start = 0;
    res.match_length = 1;
    res.strstart = s->strstart;

    dist = (int32_t)s->strstart - (int32_t)hash_head;
    if (dist <= (int32_t)max_dist && dist > 0 && hash_head != 0) {
        /* To simplify the code, we prevent matches with the string
         * of window index 0 (in particular we have to avoid a match
         * of the string with itself at the start of the input file).
         */
        /* The hash table is rolling (insert_roll), so only the roll variant
         * of longest_match may search it: its offset re-seeds hash scan
         * substrings with the same 3-byte rolling key. The base and knuth
         * variants would either miss those candidates or look up chains
         * under a foreign key. */
        uint32_t match_len = FUNCTABLE_CALL(longest_match_slow_roll)(s, hash_head);
        res.match_start = s->match_start;
        if (UNLIKELY(match_len < STD_MIN_MATCH))
            return res;
        if (UNLIKELY(res.match_start >= res.strstart)) {
            /* this can happen due to some restarts */
            return res;
        }
        res.match_length = match_len;
    }
    return res;
}

/* deflate_extreme compresses with a lazy two-position lookahead: at each step
 * it searches for matches at the cursor and one/two positions ahead, then
 * emits the minimum fixed-Huffman bit-cost sequence among {C0}, {lit, C1},
 * {lit, lit, C2}. All hash inserts are strictly sequential (the rolling hash
 * state only advances forward), so ahead searches use direct key lookups.
 */
Z_INTERNAL block_state deflate_extreme(deflate_state *s, int flush) {
    unsigned char *window = s->window;
    int32_t max_dist = MAX_DIST(s);

    /* Extreme holds no lazy state between iterations (the decision commits
     * immediately), so clear anything a previous strategy left behind. In
     * particular longest_match uses s->prev_length as a search floor; a stale
     * value from deflate_slow could discard every match after a level switch
     * into this one. */
    s->match_available = 0;
    s->prev_length = 0;

    for (;;) {
        int bflush = 0;       /* set if current block must be flushed */

        /* Make sure that we always have enough lookahead, except
         * at the end of the input file. We need STD_MAX_MATCH bytes
         * for the next match, plus WANT_MIN_MATCH bytes to insert the
         * string following the next match.
         */
        if (UNLIKELY(s->lookahead < MIN_LOOKAHEAD)) {
            PREFIX(fill_window)(s);
            if (UNLIKELY(s->lookahead < MIN_LOOKAHEAD && flush == Z_NO_FLUSH)) {
                return need_more;
            }
            if (UNLIKELY(s->lookahead == 0))
                break; /* flush the current block */
        }

        /* Capture the cursor only after fill_window: a window slide inside it
         * rebases s->strstart by -w_size, and every window access below must
         * use the post-slide coordinate. */
        const uint32_t pos = s->strstart;

        /* Insert the string at pos in the dictionary. fill_window keeps the
         * rolling hash state positioned exactly here, so this is always the
         * next sequential insert (deflate_slow relies on the same invariant).
         */
        uint32_t hash_head = 0;   /* head of the hash chain */
        if (LIKELY(s->lookahead >= WANT_MIN_MATCH))
            hash_head = insert_roll(s, window, pos);

        /* Candidate at pos. */
        struct match cand[3];
        int ncand;
        cand[0] = find_best_match(s, hash_head, max_dist);
        ncand = 1;

        /* Candidates one and two positions ahead, looked up without inserting
         * so the rolling hash state stays sequential. find_best_match wants the
         * chain head position, so resolve the probe key through s->head. */
        if (LIKELY(s->lookahead >= WANT_MIN_MATCH + 1)) {
            s->strstart = pos + 1;
            cand[1] = find_best_match(s, s->head[roll_lookup(window, pos + 1)], max_dist);
            ncand = 2;
        }
        if (LIKELY(s->lookahead >= WANT_MIN_MATCH + 2)) {
            s->strstart = pos + 2;
            cand[2] = find_best_match(s, s->head[roll_lookup(window, pos + 2)], max_dist);
            ncand = 3;
        }
        s->strstart = pos;

        /* longest_match clamps lengths against s->lookahead measured from the
         * original cursor, so a search k positions ahead may overstate its
         * length by up to k near EOF. Cap each probe at the bytes actually
         * available from its position. */
        for (int k = 1; k < ncand; k++) {
            const uint32_t avail = s->lookahead - (uint32_t)k;
            if (UNLIKELY(cand[k].match_length > avail)) {
                cand[k].match_length = avail;
                if (cand[k].match_length < STD_MIN_MATCH) {
                    cand[k].match_length = 1;
                    cand[k].match_start = 0;
                }
            }
        }

        /* Lazy decision: emit the minimum-score sequence, where score is the
         * explicit token bits minus TAIL_BITS_PER_BYTE per advanced byte.
         * Ties go to the longest advance. */
        int64_t best_score = INT64_MAX;
        uint32_t best_advance = 0;
        int best_k = 0;
        for (int k = 0; k < ncand; k++) {
            uint32_t cost = 0;
            for (int i = 0; i < k; i++)
                cost += lit_cost(window[pos + (uint32_t)i]);
            uint32_t advance;
            if (cand[k].match_length == 1) {
                cost += lit_cost(window[pos + (uint32_t)k]);
                advance = (uint32_t)k + 1u;
            } else {
                cost += match_cost(cand[k].match_length, cand[k].strstart - cand[k].match_start);
                advance = (uint32_t)k + cand[k].match_length;
            }
            const int64_t score = (int64_t)cost - (int64_t)TAIL_BITS_PER_BYTE * (int64_t)advance;
            if (score < best_score || (score == best_score && advance > best_advance)) {
                best_score = score;
                best_advance = advance;
                best_k = k;
            }
        }

        /* The commit tallies best_k+1 tokens and bflush is only checked after
         * the whole commit, so a multi-token commit starting within that many
         * tokens of sym_end would write past it. Flush the open block first
         * when they would not fit; a single token always fits (sym_next is a
         * multiple of 3 and at most sym_end - 3 here). */
#ifdef LIT_MEM
        const uint32_t commit_need = (uint32_t)(best_k + 1);
#else
        const uint32_t commit_need = 3u * (uint32_t)(best_k + 1);
#endif
        if (UNLIKELY(best_k > 0 && s->sym_next + commit_need > s->sym_end)) {
            FLUSH_BLOCK(s, window, 0);
        }

        /* Commit the chosen sequence. */
        for (int i = 0; i < best_k; i++)
            bflush |= zng_tr_tally_lit(s, window[pos + (uint32_t)i]);
        if (cand[best_k].match_length == 1) {
            bflush |= zng_tr_tally_lit(s, window[pos + (uint32_t)best_k]);
        } else {
            check_match(s, cand[best_k].strstart, cand[best_k].match_start, cand[best_k].match_length);
            bflush |= zng_tr_tally_dist(s, cand[best_k].strstart - cand[best_k].match_start,
                                        cand[best_k].match_length - STD_MIN_MATCH);
        }

        /* Insert in the hash table all strings up to the end of the emitted
         * region: [pos+1 .. pos+advance-1]. pos is already inserted; probed
         * positions are not. Clamped like deflate_slow so keys never read past
         * valid data; a clamped gap is re-primed by fill_window on the next
         * round, which always follows since advance >= lookahead - 1 then. */
        {
            const uint32_t max_insert = pos + s->lookahead - STD_MIN_MATCH;
            if (max_insert > pos) {
                uint32_t insert_cnt = best_advance - 1u;
                if (UNLIKELY(insert_cnt > max_insert - pos))
                    insert_cnt = max_insert - pos;
                insert_roll_batch(s, window, pos + 1, insert_cnt);
            }
        }

        s->lookahead -= best_advance;
        s->strstart += best_advance;

        if (UNLIKELY(bflush))
            FLUSH_BLOCK(s, window, 0);
    }

    /* Positions still to insert into the hash table when input resumes. */
    s->insert = s->strstart < (STD_MIN_MATCH - 1) ? s->strstart : (STD_MIN_MATCH - 1);
    if (UNLIKELY(flush == Z_FINISH)) {
        FLUSH_BLOCK(s, window, 1);
        return finish_done;
    }
    if (UNLIKELY(s->sym_next))
        FLUSH_BLOCK(s, window, 0);

    return block_done;
}
