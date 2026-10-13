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

/* Extra-bit tables, mirroring trees.h (RFC1951 C.4). */
static const uint32_t xtra_len[29] = {
    0,0,0,0,0,0,0,0,1,1,1,1,2,2,2,2,3,3,3,3,4,4,4,4,5,5,5,5,0
};
static const uint32_t xtra_dist[30] = {
    0,0,0,0,1,1,2,2,3,3,4,4,5,5,6,6,7,7,8,8,9,9,10,10,11,11,12,12,13,13
};

/* Push node onto a min-heap ordered by weight[]. */
Z_FORCEINLINE static void heap_push(const uint32_t *weight, int node, int *heap, int *heap_sz) {
    heap[*heap_sz] = node;
    int child = (*heap_sz)++;
    /* Sift up until the parent's weight is no larger. */
    while (child > 0) {
        const int parent = (child - 1) / 2;
        if (weight[heap[parent]] <= weight[heap[child]])
            return;
        const int tmp = heap[parent];
        heap[parent] = heap[child];
        heap[child] = tmp;
        child = parent;
    }
}

/* Pop the minimum-weight node off the heap. */
Z_FORCEINLINE static int heap_pop(const uint32_t *weight, int *heap, int *heap_sz) {
    const int root = heap[0];
    heap[0] = heap[--(*heap_sz)];
    int child = 0;
    /* Sift down until the node is no heavier than both children. */
    for (;;) {
        const int left = 2 * child + 1;
        const int right = left + 1;
        int lowest = child;
        if (left < *heap_sz && weight[heap[left]] < weight[heap[lowest]])
            lowest = left;
        if (right < *heap_sz && weight[heap[right]] < weight[heap[lowest]])
            lowest = right;
        if (lowest == child)
            return root;
        const int tmp = heap[lowest];
        heap[lowest] = heap[child];
        heap[child] = tmp;
        child = lowest;
    }
}

/* Compute Huffman code lengths for nsym symbols from freq[] into out_len[].
 * Symbols with zero frequency get length 0. With a single distinct symbol the
 * tree degenerates and it gets length 1 (deflate needs at least one bit).
 * Depths above the deflate maximum of 15 are clamped, as the real builder does; beyond that the plain merge only
 * approximates build_tree's length-limited tree. */
static void huff_lengths(const uint32_t *freq, int nsym, uint8_t *out_len) {
    int nleaf = 0;
    /* Count the distinct (nonzero-frequency) symbols. */
    for (int i = 0; i < nsym; i++)
        if (freq[i] > 0)
            nleaf++;
    if (nleaf == 0)
        return; /* keep the previous lengths */

    /* Clear the previous lengths. */
    for (int i = 0; i < nsym; i++)
        out_len[i] = 0;
    if (nleaf == 1) {
        /* The single distinct symbol takes a 1-bit code. */
        for (int i = 0; i < nsym; i++)
            if (freq[i] > 0)
                out_len[i] = 1;
        return;
    }

    /* Nodes 0..nleaf-1 are the leaves (in symbol order), the rest internal.
     * nsym <= 286, so total <= 571. */
    const int total = 2 * nleaf - 1;
    uint32_t weight[571];
    int ch_a[571], ch_b[571];
    int leaf_sym[286];
    int heap[571];
    int heap_sz = 0;

    /* Push the leaves (nonzero-frequency symbols) in symbol order. */
    for (int i = 0, nseen = 0; i < nsym; i++) {
        if (freq[i] == 0)
            continue;
        const int node = nseen++;
        weight[node] = freq[i];
        leaf_sym[node] = i;
        ch_a[node] = -1;
        heap_push(weight, node, heap, &heap_sz);
    }

    /* Merge the two lightest nodes until one root remains. */
    for (int node = nleaf; node < total; node++) {
        const int a = heap_pop(weight, heap, &heap_sz);
        const int b = heap_pop(weight, heap, &heap_sz);
        weight[node] = weight[a] + weight[b];
        ch_a[node] = a;
        ch_b[node] = b;
        heap_push(weight, node, heap, &heap_sz);
    }

    /* Walk the root iteratively and record each leaf's depth. */
    int stack_node[571], stack_depth[571];
    int nstack = 0;
    stack_node[nstack] = heap[0];
    stack_depth[nstack] = 0;
    nstack++;
    while (nstack > 0) {
        nstack--;
        const int node = stack_node[nstack];
        const int depth = stack_depth[nstack];
        if (ch_a[node] < 0) {
            out_len[leaf_sym[node]] = (uint8_t)(depth > 15 ? 15 : depth);
        } else {
            stack_node[nstack] = ch_a[node];
            stack_depth[nstack] = depth + 1;
            nstack++;
            stack_node[nstack] = ch_b[node];
            stack_depth[nstack] = depth + 1;
            nstack++;
        }
    }
}

/* Estimated bit cost of a match token with the given length (>= STD_MIN_MATCH)
 * and distance (>= 1), using the running Huffman length estimates. Layout
 * mirrors trees.c: length code lc lives at LITERALS+1+lc of the lit/length
 * tree, distance code dc at index dc of the distance tree. */
Z_FORCEINLINE static uint32_t match_cost(const deflate_state *s, uint32_t len, uint32_t dist) {
    const uint32_t lcode = zng_length_code[len - STD_MIN_MATCH];
    const uint32_t dcode = d_code(dist - 1);
    return s->ext_ll_len[LITERALS + 1 + lcode] + xtra_len[lcode]
         + s->ext_d_len[dcode] + xtra_dist[dcode];
}

/* Estimated bit cost of a literal byte. */
Z_FORCEINLINE static uint32_t lit_cost(const deflate_state *s, uint32_t c) {
    return s->ext_ll_len[c];
}

/* Number of positions ahead of the cursor searched as candidate match start
 * points (the candidate at the cursor itself is always included). Each extra
 * position costs one full chain search per iteration. */
#define EXTREME_PROBE_DEPTH 2

/* Upper bound on the imputed bit value of one input byte left to be encoded
 * by later decisions. A candidate sequence is credited with min(this, mode
 * literal cost - 1) bits per advanced byte, so a good match beats "emit
 * literals and re-decide inside the match region" (which would otherwise win
 * on explicit bit cost alone), while a long-distance 3-byte match still loses
 * to its three literals. Capping at the mode literal cost keeps deep probes
 * from winning on credit alone when literals are cheap (RLE-able data). */
#define TAIL_BITS_PER_BYTE 3u

/* Regime gate threshold; see the decision loop below. */
#define LIT_GATE 6

/* Re-phase short C0 matches like deflate_slow's lazy truncation: a strictly
 * longer match one or two positions ahead is taken with literal gap-fill.
 * Only applied while the current match is this short, so long-match data
 * keeps the fully priced DP. */
#define REPHASE_MAX_LEN 8

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
 * since longest_match derives its bounds from it.
 *
 * The chain search returns the longest match. Extreme additionally re-checks
 * the closest same-key candidate (hash_head itself, the LIFO chain head) when
 * the longest sits far away: distance extra bits dominate token cost, so a
 * shorter but much closer match can score better under the same tail-credit
 * rule the outer DP uses. */
Z_FORCEINLINE static struct match find_best_match(deflate_state *s, uint32_t hash_head, int32_t max_dist,
                                                  uint32_t tail_credit) {
    struct match res;
    int32_t dist;

    /* The caller may raise s->prev_length to a discard floor (deflate_slow
     * does this for Z_FILTERED); longest_match then returns that floor with a
     * garbage start when nothing longer exists, so treat any result at or
     * below it as no match. */
    const uint32_t match_floor = s->prev_length;

    /* Initialize the result to be a 1 byte literal */
    res.match_start = 0;
    res.match_length = 1;
    res.strstart = s->strstart;

    dist = (int32_t)s->strstart - (int32_t)hash_head;
    if (UNLIKELY(hash_head == 0)) {
        /* The NIL sentinel hides matches to window index 0: a chain whose
         * first insert was position 0 reads as empty, so longest_match is
         * never reached for them. Compare source 0 directly when it is
         * within reach; the prefix check below rejects non-matches cheaply. */
        if (s->strstart > 0 && s->strstart <= (uint32_t)max_dist) {
            const uint64_t scan_start = zng_memread_8(s->window + s->strstart);
            const uint64_t diff = scan_start ^ zng_memread_8(s->window);
            uint32_t len0;
            if (diff == 0) {
                len0 = FUNCTABLE_CALL(compare256)(s->window + s->strstart + 2, s->window + 2) + 2;
            } else {
                len0 = zng_first_diff_byte64(diff);
            }
            if (UNLIKELY(len0 > s->lookahead))
                len0 = s->lookahead;
            if (len0 >= STD_MIN_MATCH && len0 > match_floor) {
                res.match_start = 0;
                res.match_length = len0;
            }
        }
        return res;
    }
    if (dist <= (int32_t)max_dist && dist > 0) {
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
        if (UNLIKELY(match_len <= match_floor || match_len < STD_MIN_MATCH))
            return res;
        if (UNLIKELY(res.match_start >= res.strstart)) {
            /* this can happen due to some restarts */
            return res;
        }
        res.match_length = match_len;

        /* A far longest match pays 5-13 distance extra bits; the closest
         * same-key occurrence may score better once its shorter length is
         * credited. Only worth measuring when the winner is far. The read is
         * bounded by the same window padding guarantee longest_match relies
         * on, and the result is clamped like its EOF guard. */
        const uint32_t far_dist = res.strstart - res.match_start;
        if (LIKELY(far_dist > 32)) {
            /* The rolling key is only 15 bits, so hash_head may be a pure
             * collision: verify the prefix exactly like longest_match does.
             * A full 8-byte match extends through compare256; otherwise the
             * common prefix length (if >= STD_MIN_MATCH) is the match. */
            const uint64_t scan_start = zng_memread_8(s->window + s->strstart);
            const uint64_t diff = scan_start ^ zng_memread_8(s->window + hash_head);
            uint32_t near_len;
            if (diff == 0) {
                near_len = FUNCTABLE_CALL(compare256)(s->window + s->strstart + 2, s->window + hash_head + 2) + 2;
            } else {
                near_len = zng_first_diff_byte64(diff);
            }
            if (UNLIKELY(near_len > s->lookahead))
                near_len = s->lookahead;
            /* Respect the discard floor: deflate_slow would never emit a
             * match this short under Z_FILTERED. */
            if (near_len >= STD_MIN_MATCH && near_len > match_floor) {
                const int64_t far_score = (int64_t)match_cost(s, res.match_length, far_dist)
                                        - (int64_t)tail_credit * (int64_t)res.match_length;
                const int64_t near_score = (int64_t)match_cost(s, near_len, (uint32_t)dist)
                                          - (int64_t)tail_credit * (int64_t)near_len;
                if (near_score < far_score) {
                    res.match_start = hash_head;
                    res.match_length = near_len;
                }
            }
        }
    }
    return res;
}

/* deflate_extreme compresses with a lazy two-position lookahead: at each step
 * it searches for matches at the cursor and one/two positions ahead, then
 * emits the minimum-cost sequence among {C0}, {lit, C1}, {lit, lit, C2}, priced with the running
 * dynamic-Huffman cost model. All hash inserts are strictly sequential (the rolling hash state only
 * advances forward), so ahead searches use direct key lookups.
 */
Z_INTERNAL block_state deflate_extreme(deflate_state *s, int flush) {
    unsigned char *window = s->window;
    int32_t max_dist = MAX_DIST(s);

    /* Matches this long or shorter are discarded, mirroring deflate_slow:
     * Z_FILTERED ignores lengths up to 5. Raised as the search floor via
     * s->prev_length around every candidate lookup in the loop below. */
    const uint32_t match_discard = (s->strategy == Z_FILTERED) ? 5 : 0;
    const uint32_t match_floor = MIN(match_discard, s->good_match - 1);

    /* Extreme holds no lazy state between iterations (the decision commits
     * immediately), so clear anything a previous strategy left behind. In
     * particular longest_match uses s->prev_length as a search floor; a stale
     * value from deflate_slow could discard every match after a level switch
     * into this one. */
    s->match_available = 0;
    s->prev_length = 0;

    /* Initialize the cost model on a fresh stream (total_in == 0, which
     * deflateReset guarantees before any input is read) or on the first entry
     * into this strategy after a level switch. Every field must be set
     * explicitly: unlike classic zlib, zlib-ng's zcalloc does not zero the
     * state, so a reused allocation can carry another stream's model (or
     * garbage in the reserved[19] flag itself). */
    if (UNLIKELY(s->strm->total_in == 0 || s->reserved[19] == 0)) {
        for (int i = 0; i < 316; i++)
            s->ext_freq[i] = 0;
        s->ext_since_build = 0;
        s->ext_mode_lit_cost = 8; /* fixed-table literal cost until measured */
        /* Fixed-Huffman lengths (RFC1951 C.4); running statistics replace
         * them as tokens are tallied. */
        for (int i = 0; i < 144; i++)
            s->ext_ll_len[i] = 8;
        for (int i = 144; i < LITERALS; i++)
            s->ext_ll_len[i] = 9;
        for (int i = 0; i < 23; i++)
            s->ext_ll_len[LITERALS + 1 + i] = 7;
        for (int i = 23; i < 29; i++)
            s->ext_ll_len[LITERALS + 1 + i] = 8;
        for (int i = 0; i < 30; i++)
            s->ext_d_len[i] = 5;
        s->reserved[19] = 1;
    }

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

        /* Imputed bit value of one byte left to later decisions. A candidate
         * is credited with min(TAIL_BITS_PER_BYTE, mode literal cost - 1)
         * bits per advanced byte; the cap keeps deep probes from winning on
         * credit alone when literals are cheap (RLE-able data). */
        const uint32_t tail_credit = s->ext_mode_lit_cost > 1u
            ? MIN(TAIL_BITS_PER_BYTE, s->ext_mode_lit_cost - 1u)
            : 0u;

        /* Candidate at pos. The discard floor is carried in s->prev_length
         * for the duration of all three lookups (longest_match uses it as its
         * search floor) and restored afterwards. */
        struct match cand[EXTREME_PROBE_DEPTH + 1];
        int ncand = 1;
        s->prev_length = match_floor;
        cand[0] = find_best_match(s, hash_head, max_dist, tail_credit);

        /* Candidates one..EXTREME_PROBE_DEPTH positions ahead, looked up
         * without inserting so the rolling hash state stays sequential.
         * find_best_match wants the chain head position, so resolve each probe
         * key through s->head. */
        for (int k = 1; k <= EXTREME_PROBE_DEPTH; k++) {
            if (UNLIKELY(s->lookahead < WANT_MIN_MATCH + (uint32_t)k))
                break;
            s->strstart = pos + (uint32_t)k;
            cand[k] = find_best_match(s, s->head[roll_lookup(window, pos + (uint32_t)k)], max_dist, tail_credit);
            ncand++;
        }
        s->prev_length = 0;
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
         * explicit token bits minus the tail credit per advanced byte (see
         * above). Ties go to the longest advance. */
        int64_t best_score = INT64_MAX;
        uint32_t best_advance = 0;
        int best_k = 0;
#if LIT_GATE > 0
        /* Regime gate: when the most frequent literal needs more than
         * LIT_GATE bits, literals are expensive and a found C0 match almost
         * always beats the DP alternatives, so commit it directly. On data
         * dominated by one cheap literal (filter bytes, constant stretches)
         * keep the full two-ply DP, which pays off there. */
        if (cand[0].match_length >= STD_MIN_MATCH && s->ext_mode_lit_cost > LIT_GATE) {
            best_k = 0;
            best_advance = cand[0].match_length;
        } else
#endif
        for (int k = 0; k < ncand; k++) {
            uint32_t cost = 0;
            for (int i = 0; i < k; i++)
                cost += lit_cost(s, window[pos + (uint32_t)i]);
            uint32_t advance;
            if (cand[k].match_length == 1) {
                cost += lit_cost(s, window[pos + (uint32_t)k]);
                advance = (uint32_t)k + 1u;
            } else {
                cost += match_cost(s, cand[k].match_length, cand[k].strstart - cand[k].match_start);
                advance = (uint32_t)k + cand[k].match_length;
            }
            const int64_t score = (int64_t)cost - (int64_t)tail_credit * (int64_t)advance;
            if (score < best_score || (score == best_score && advance > best_advance)) {
                best_score = score;
                best_advance = advance;
                best_k = k;
            }
        }

        /* Re-phase: if a strictly longer, closer match sits one/two
         * positions ahead of a short C0, take it with literal gap-fill,
         * mirroring deflate_slow's unconditional truncation on a strictly
         * longer next-position match. */
        if (best_k == 0 && cand[0].match_length >= STD_MIN_MATCH
            && cand[0].match_length <= REPHASE_MAX_LEN) {
            for (int k = 1; k < ncand; k++) {
                if (cand[k].match_length <= cand[best_k].match_length)
                    continue;
                const uint32_t dist_k = cand[k].strstart - cand[k].match_start;
                const uint32_t dist_b = cand[best_k].strstart - cand[best_k].match_start;
                if (dist_k < dist_b)
                    best_k = k;
            }
            /* The DP loop set the advance for its own winner; recompute it
             * for the re-phased one (always a strong match here). */
            best_advance = (uint32_t)best_k + cand[best_k].match_length;
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

        /* Commit the chosen sequence, mirroring every tally into the cost
         * model's running symbol counts (same layout trees.c uses). */
        for (int i = 0; i < best_k; i++) {
            bflush |= zng_tr_tally_lit(s, window[pos + (uint32_t)i]);
            s->ext_freq[window[pos + (uint32_t)i]]++;
        }
        if (cand[best_k].match_length == 1) {
            bflush |= zng_tr_tally_lit(s, window[pos + (uint32_t)best_k]);
            s->ext_freq[window[pos + (uint32_t)best_k]]++;
        } else {
            const uint32_t mlen = cand[best_k].match_length;
            const uint32_t mdist = cand[best_k].strstart - cand[best_k].match_start;
            check_match(s, cand[best_k].strstart, cand[best_k].match_start, mlen);
            bflush |= zng_tr_tally_dist(s, mdist, mlen - STD_MIN_MATCH);
            s->ext_freq[LITERALS + 1 + zng_length_code[mlen - STD_MIN_MATCH]]++;
            s->ext_freq[LITERALS + 29 + 1 + d_code(mdist - 1)]++;
        }

        /* Refresh the estimated code lengths periodically so the model tracks
         * the per-block trees trees.c actually builds. */
        if (UNLIKELY(++s->ext_since_build >= 2048)) {
            s->ext_since_build = 0;
            huff_lengths(s->ext_freq, LITERALS + 1 + 29, s->ext_ll_len);
            huff_lengths(s->ext_freq + LITERALS + 1 + 29, 30, s->ext_d_len);
            /* Cost of the most frequent literal: bounds the tail credit (see
             * the decision loop above). */
            uint32_t best_freq = 0;
            for (int i = 0; i < LITERALS; i++) {
                if (s->ext_freq[i] > best_freq) {
                    best_freq = s->ext_freq[i];
                    s->ext_mode_lit_cost = s->ext_ll_len[i];
                }
            }
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
