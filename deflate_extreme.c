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
#include "trees.h"

/* A match candidate: either a single literal (match_length == 1) or a match of
 * length >= STD_MIN_MATCH starting at strstart that matches history data at
 * match_start. */
struct match {
    uint32_t match_start;
    uint32_t match_length;
    uint32_t strstart;
    /* The other same-key candidate measured by the near re-check (0 = none):
     * offered to the decision loop as an explicit alternative alongside the
     * primary, which keeps the cut gate's view of cand[0] unchanged. */
    uint32_t alt_len;
    uint32_t alt_dist;
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
 * Zero-frequency symbols are priced one past the deepest fitted code, so a
 * never-seen token only wins when every seen alternative costs more. With a
 * single distinct symbol the tree degenerates and it gets length 1 (deflate needs at least one bit).
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
    } else {
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

    /* Unseen symbols are not free: price them one past the deepest fitted
     * code so a never-seen token only wins when every seen alternative is
     * even more expensive. */
    /* Find the deepest fitted code. */
    int max_len = 0;
    for (int i = 0; i < nsym; i++)
        if (out_len[i] > max_len)
            max_len = out_len[i];
    const uint8_t unseen_len = (uint8_t)(max_len + 1 > 15 ? 15 : max_len + 1);
    /* Price each unseen symbol one past that depth. */
    for (int i = 0; i < nsym; i++)
        if (freq[i] == 0)
            out_len[i] = unseen_len;
}

/* Estimated bit cost of a match token with the given length (>= STD_MIN_MATCH)
 * and distance (>= 1), using the running Huffman length estimates. Layout
 * mirrors trees.c: length code lc lives at LITERALS+1+lc of the lit/length
 * tree, distance code dc at index dc of the distance tree. */
Z_FORCEINLINE static uint32_t match_cost(const deflate_state *s, uint32_t len, uint32_t dist) {
    const uint32_t lcode = zng_length_code[len - STD_MIN_MATCH];
    const uint32_t dcode = d_code(dist - 1);
    return s->ext_ml_len[lcode] + s->ext_md_len[dcode];
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

/* Maximum source distance for a shortened C0 frontier option. A shorter
 * match from far away only adds a token and dilutes every block's Huffman
 * tree (measured: +38KB on DNA-like data), while short close matches save
 * distance extra bits without meaningful token overhead. The longest-match
 * option itself is unaffected by the cap. */
#define EXTREME_FRONTIER_MAX_DIST 72u
/* Under a discard floor (Z_FILTERED) the walk is re-seeded from more distant
 * chains, so recorded frontier distances run farther; on text the 65-72 band
 * then regresses (+4.5KB on big_text), so the tighter cap applies there. */
#define EXTREME_FRONTIER_MAX_DIST_FLOORED 64u

/* Priced block splitting: snapshot the open block's symbol frequencies every
 * SPLIT_SLOT_TOKENS committed sequences and, at flush time, re-fit the
 * Huffman trees for each candidate half; when splitting the block at a
 * snapshot point encodes it cheaper than keeping it whole (the second block
 * header included), emit the left half as its own block. The right half stays
 * open and is re-evaluated on later flushes, so deeper splits emerge over
 * successive blocks like zopfli's recursive splitter. SPLIT_SLOT_TOKENS
 * lives in deflate.h because deflateResetKeep re-arms the first snapshot.
 */
/* Minimum tokens per side of a split: below this the tree refit cannot pay
 * for the second block header (measured ~600-780 bits on this suite). */
#define SPLIT_MIN_SIDE_TOKENS 1024u
/* Required savings for a split to fire: the cost model is an estimate, so a
 * marginal split can lose by a few bytes; only take clear wins. */
#define SPLIT_MARGIN_BITS 64u

/* Re-phase short C0 matches like deflate_slow's lazy truncation: a strictly
 * longer match one or two positions ahead is taken with literal gap-fill.
 * Only applied while the current match is this short, so long-match data
 * keeps the fully priced DP. */
#define REPHASE_MAX_LEN 8

/* Tokens between cost-model rebuilds, and between halvings of the running
 * symbol counts: the decay keeps the sample large (low-variance tree) while
 * letting recent content dominate, so the estimator tracks drift without
 * the oscillation a per-block reset would cause. */
#define EXTREME_REBUILD_EVERY 1024u
#define EXTREME_DECAY_EVERY 262144u

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

/* Compose the per-code match token costs (symbol length + extra bits, one
 * entry per length/distance code) from the current fitted lengths, so
 * match_cost needs two table loads and one add. */
Z_FORCEINLINE static void extreme_model_compose(deflate_state *s) {
    for (int i = 0; i < 29; i++)
        s->ext_ml_len[i] = (uint8_t)(s->ext_ll_len[LITERALS + 1 + i] + xtra_len[i]);
    for (int i = 0; i < 30; i++)
        s->ext_md_len[i] = (uint8_t)(s->ext_d_len[i] + xtra_dist[i]);
}

/* Reset the running cost model to fixed-Huffman priors. Called on a fresh
 * stream (or after a level switch) when no statistics exist yet. */
Z_FORCEINLINE static void extreme_model_reset(deflate_state *s) {
    /* Zero the running symbol counts and rebuild counters. */
    for (int i = 0; i < 316; i++)
        s->ext_freq[i] = 0;
    s->ext_since_build = 0;
    s->ext_distinct_lit = 0;
    s->ext_rebuilds = 0;
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
    extreme_model_compose(s);
}

Z_FORCEINLINE static uint32_t longest_match_extreme(deflate_state *const s, uint32_t cur_match,
                                                    uint16_t *sublen);

/* Search the hash chain headed by hash_head for the best match at s->strstart.
 * Returns a candidate with match_length == 1 when no usable match is found.
 * Callers searching ahead must set s->strstart to the probe position first,
 * since longest_match derives its bounds from it.
 *
 * The chain search returns the longest match. Extreme additionally re-checks
 * the closest same-key candidate (hash_head itself, the LIFO chain head) when
 * the longest sits far away: distance extra bits dominate token cost, so a
 * shorter but much closer match can score better under the same tail-credit
 * rule the outer DP uses. The cheaper of the pair stays the primary; the other is kept in alt_len/alt_dist so the
 * decision loop can still pick it.
 *
 * sublen_out receives the length->distance frontier recorded during the walk
 * (NULL when the caller does not need it; a local scratch is used instead). */
Z_FORCEINLINE static struct match find_best_match(deflate_state *s, uint32_t hash_head, int32_t max_dist,
                                                  uint32_t tail_credit, uint16_t *sublen_out) {
    struct match res;
    int32_t dist;
    uint16_t sublen_scratch[STD_MAX_MATCH + 1];
    uint16_t *const sublen = (sublen_out != NULL) ? sublen_out : sublen_scratch;

    /* The caller may raise s->prev_length to a discard floor (deflate_slow
     * does this for Z_FILTERED); longest_match then returns that floor with a
     * garbage start when nothing longer exists, so treat any result at or
     * below it as no match. */
    const uint32_t match_floor = s->prev_length;

    /* Initialize the result to be a 1 byte literal */
    res.match_start = 0;
    res.match_length = 1;
    res.strstart = s->strstart;
    res.alt_len = 0;
    res.alt_dist = 0;

    dist = (int32_t)s->strstart - (int32_t)hash_head;
    if (UNLIKELY(hash_head == 0)) {
        /* The NIL sentinel hides matches to window index 0: a chain whose
         * first insert was position 0 reads as empty, so longest_match is
         * never reached for them. Compare source 0 directly when it is
         * within reach; the helper rejects non-matches cheaply. */
        const uint32_t len0 = match_window_zero(s, s->strstart, max_dist);
        if (len0 >= STD_MIN_MATCH && len0 > match_floor) {
            res.match_start = 0;
            res.match_length = len0;
            for (uint32_t j = match_floor + 1; j <= len0; j++)
                sublen[j] = (uint16_t)s->strstart;
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
         * under a foreign key. longest_match_extreme replicates that walk and
         * additionally records the length->distance frontier in sublen. */
        uint32_t match_len = longest_match_extreme(s, hash_head, sublen);
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
                /* Keep the cheaper one as the primary (the cut gate and the
                 * best_k == 0 advance see exactly what they always did) but
                 * remember the other: dropping it here truncated every
                 * frontier option longer than a swapped-in near match. */
                if (near_score < far_score) {
                    res.alt_len = res.match_length;
                    res.alt_dist = far_dist;
                    res.match_start = hash_head;
                    res.match_length = near_len;
                } else {
                    res.alt_len = near_len;
                    res.alt_dist = (uint32_t)dist;
                }
            }
        }
    }
    return res;
}

/* Re-run extreme's candidate walk to build a length->distance frontier.
 *
 * Walks the same chain as the roll variant of longest_match (same limit,
 * chain cap, entry offset re-seed, post-match offset search) and records
 * for every length k that a new record reaches the distance of the
 * candidate that first reached it: sublen[k] is the closest-walk-order
 * source matching at least k bytes. The decision loop offers these as
 * alternative (length, distance) options at the cursor -- short close
 * matches and long far matches instead of only the single longest one,
 * which is what photo-optimized parsers emit.
 *
 * The walk is a faithful replication of the roll variant of
 * longest_match, so it replaces that search entirely; the only addition is
 * the frontier recording (every recorded pair came from a measured LCP at a
 * valid source). Returns the longest length found, like longest_match. */
Z_FORCEINLINE static uint32_t longest_match_extreme(deflate_state *const s, uint32_t cur_match,
                                                    uint16_t *sublen) {
    const Pos *const prev = s->prev;
    const Pos *const head = s->head;
    const unsigned char *const window = s->window;
    const unsigned wmask = W_MASK(s);
    const unsigned int strstart = s->strstart;
    uint32_t chain_length = s->max_chain_length;
    const uint32_t nice_match = (uint32_t)s->nice_match;
    uint32_t best_len, offset;
    const uint32_t lookahead = s->lookahead;
    uint32_t match_offset = 0;
    const unsigned char *scan;
    const unsigned char *mbase_start;
    const unsigned char *mbase_end;
    uint64_t scan_start;
    uint64_t scan_end;
    uint32_t limit, limit_base;

    memset(sublen, 0, (STD_MAX_MATCH + 1) * sizeof(uint16_t));

    best_len = s->prev_length ? s->prev_length : STD_MIN_MATCH - 1;
    if (UNLIKELY(best_len >= lookahead))
        return lookahead;

    offset = best_len >= sizeof(uint64_t) ? best_len - 7 : 0;

    scan = window + strstart;
    scan_start = zng_memread_8(scan);
    scan_end = zng_memread_8(scan + offset);
    mbase_start = window;
    mbase_end = mbase_start + offset;

    if (UNLIKELY(best_len >= s->good_match))
        chain_length >>= 2;

    limit = strstart > MAX_DIST(s) ? strstart - MAX_DIST(s) : 0;
    limit_base = limit;
    if (best_len >= STD_MIN_MATCH) {
        /* Entry offset re-seed, replicated from the template: under a
         * discard floor start from the most distant same-substring chain. */
        uint32_t hash;
        uint32_t pos;

        hash = update_hash_roll(0, scan[1]);
        hash = update_hash_roll(hash, scan[2]);
        for (uint32_t i = 3; i <= best_len; i++) {
            hash = update_hash_roll(hash, scan[i]);
            pos = head[hash];
            if (UNLIKELY(pos < cur_match)) {
                match_offset = i - 2;
                cur_match = pos;
            }
        }
        limit = limit_base + match_offset;
        if (UNLIKELY(cur_match <= limit))
            return best_len;
        mbase_start -= match_offset;
        mbase_end -= match_offset;
    }

    /* Walk the chain newest-first until the limit or budget runs out. */
    for (;;) {
        if (UNLIKELY(cur_match >= strstart))
            break; /* at/after the cursor: not usable source */

        uint32_t len;
        if (best_len < sizeof(uint64_t)) {
            uint64_t cand_start = zng_memread_8(mbase_start + cur_match);
            if (scan_start != cand_start) {
                uint64_t first_mask = zng_first_bytes_mask64(best_len + 1);
                uint64_t diff = scan_start ^ cand_start;
                /* A candidate beats best_len only when its first best_len+1
                 * bytes match; the masked test rejects without running ctz. */
                if (UNLIKELY((diff & first_mask) == 0)) {
                    len = zng_first_diff_byte64(diff);
                    goto short_match_accept;
                }
                if (--chain_length == 0 || (cur_match = prev[cur_match & wmask]) <= limit)
                    return best_len;
                cand_start = zng_memread_8(mbase_start + cur_match);
                if (scan_start != cand_start) {
                    for (;;) {
                        diff = scan_start ^ cand_start;
                        if (UNLIKELY((diff & first_mask) == 0)) {
                            len = zng_first_diff_byte64(diff);
                            goto short_match_accept;
                        }
                        if (--chain_length == 0 || (cur_match = prev[cur_match & wmask]) <= limit)
                            return best_len;
                        cand_start = zng_memread_8(mbase_start + cur_match);
                        if (scan_start == cand_start)
                            break;
                    }
                }
            }
            /* All 8 bytes match, fallthrough to compare256 for the tail. */
        } else {
            /* Pre-filter on start and end sentinels before compare256. */
            for (;;) {
                if (zng_memcmp_8(mbase_end + cur_match, &scan_end) == 0 &&
                    zng_memcmp_8(mbase_start + cur_match, &scan_start) == 0) {
                    break;
                }
                if (--chain_length && (cur_match = prev[cur_match & wmask]) > limit)
                    continue;
                return best_len;
            }
        }

        len = FUNCTABLE_CALL(compare256)(scan + 2, mbase_start + cur_match + 2) + 2;

        if (len > best_len)
short_match_accept:
        {
            const uint32_t match_start = cur_match - match_offset;
            s->match_start = match_start;

            /* Record the frontier before any early return below. */
            for (uint32_t j = best_len + 1; j <= len && j <= STD_MAX_MATCH; j++)
                sublen[j] = (uint16_t)(strstart - match_start);

            if (UNLIKELY(len >= lookahead))
                return lookahead;
            if (UNLIKELY(len >= nice_match))
                return len;

            best_len = len;
            offset = best_len >= sizeof(uint64_t) ? best_len - 7 : 0;
            scan_end = zng_memread_8(scan + offset);

            /* Look for a better string offset, replicated from the template:
             * re-seed the walk at a more distant chain inside the match so
             * candidates aligned to an internal position can extend it. */
            if (UNLIKELY(len > STD_MIN_MATCH && match_start + len < strstart)) {
                const unsigned char *scan_endstr;
                uint32_t hash;
                uint32_t pos, next_pos;

                /* Go back to offset 0 */
                cur_match -= match_offset;
                match_offset = 0;
                next_pos = cur_match;

                for (uint32_t i = 0; i <= len - STD_MIN_MATCH; i++) {
                    pos = prev[(cur_match + i) & wmask];
                    if (UNLIKELY(pos < next_pos)) {
                        if (UNLIKELY(pos <= limit_base + i))
                            return best_len;
                        next_pos = pos;
                        match_offset = i;
                    }
                }
                cur_match = next_pos;

                scan_endstr = scan + len - (STD_MIN_MATCH - 1);
                hash = update_hash_roll(0, scan_endstr[0]);
                hash = update_hash_roll(hash, scan_endstr[1]);
                hash = update_hash_roll(hash, scan_endstr[2]);
                pos = head[hash];
                if (UNLIKELY(pos < cur_match)) {
                    match_offset = len - (STD_MIN_MATCH - 1);
                    if (pos <= limit_base + match_offset)
                        return best_len;
                    cur_match = pos;
                }

                limit = limit_base + match_offset;
                mbase_start = window - match_offset;
                mbase_end = mbase_start + offset;
                continue;
            }
            mbase_end = mbase_start + offset;
        }
        if (--chain_length && (cur_match = prev[cur_match & wmask]) > limit)
            continue;
        return best_len;
    }
    return best_len;
}

/* Boolean chain probe for the cut gate: answers whether a usable match
 * exists at some position without measuring its length or distance. It runs
 * the same candidate walk longest_match would (same limit, same chain cap,
 * same entry offset re-seed under a discard floor) but exits on the first
 * candidate reaching the acceptance threshold -- typically after one or two
 * comparisons instead of a full 32k-link walk. The verdict stays in lockstep
 * with find_best_match: usable iff some candidate's common prefix exceeds
 * max(match_floor, STD_MIN_MATCH - 1), and only while enough bytes are
 * actually available (the gate clamps longer results to literals). The
 * threshold is at most 6 here (floors are 0 or 5), so one masked 8-byte read
 * decides every candidate. */
Z_FORCEINLINE static unsigned match_exists_at(deflate_state *s, const unsigned char *window, uint32_t probe_pos,
        int32_t max_dist, uint32_t match_floor, uint32_t avail_bytes) {
    const Pos *prev = s->prev;
    const uint32_t hash_head = s->head[roll_lookup(window, probe_pos)];

    if (UNLIKELY(hash_head == 0)) {
        /* The NIL sentinel hides matches to window index 0; compare source 0
         * directly exactly like find_best_match does. */
        const uint32_t len0 = match_window_zero(s, probe_pos, max_dist);
        return (len0 > match_floor && len0 >= STD_MIN_MATCH && avail_bytes >= STD_MIN_MATCH) ? 1u : 0u;
    }

    const int32_t dist = (int32_t)probe_pos - (int32_t)hash_head;
    if (dist <= max_dist && dist > 0) {
        uint32_t cur_match = hash_head;
        uint32_t limit = probe_pos > MAX_DIST(s) ? probe_pos - MAX_DIST(s) : 0u;
        uint32_t chain_length = s->max_chain_length;

        /* Acceptance threshold: one past the entry best_len longest_match
         * would start from (the discard floor, or 2 below it). */
        const uint32_t entry_floor = match_floor > STD_MIN_MATCH - 1u ? match_floor : STD_MIN_MATCH - 1u;
        const uint64_t hit_mask = zng_first_bytes_mask64(entry_floor + 1u);

        if (avail_bytes < entry_floor + 1u)
            return 0u; /* the clamp would demote any hit to a literal */

        /* Do not waste too much time if we already have a good match, like
         * longest_match's entry check. */
        if (UNLIKELY(entry_floor >= s->good_match))
            chain_length >>= 2;

        /* Entry offset re-seed, replicated from longest_match: under a
         * discard floor it starts from the most distant same-substring chain
         * and shifts the limit by the same offset. */
        uint32_t match_offset = 0u;
        if (UNLIKELY(entry_floor >= STD_MIN_MATCH)) {
            const unsigned char *scan = window + probe_pos;
            uint32_t hash = update_hash_roll(0, scan[1]);
            hash = update_hash_roll(hash, scan[2]);
            for (uint32_t sub_len = 3u; sub_len <= entry_floor; sub_len++) {
                hash = update_hash_roll(hash, scan[sub_len]);
                const uint32_t pos = s->head[hash];
                if (UNLIKELY(pos < cur_match)) {
                    match_offset = sub_len - 2u;
                    cur_match = pos;
                }
            }
            limit += match_offset;
            if (cur_match <= limit)
                return 0u; /* longest_match would return the floor here */
        }

        const uint64_t scan_start = zng_memread_8(window + probe_pos);
        /* Walk the chain until a prefix hit, the limit, or the budget runs out. */
        for (;;) {
            if (UNLIKELY(cur_match >= probe_pos))
                return 0u; /* at/after the probe position: not usable source */
            const uint64_t cand_start = zng_memread_8(window + cur_match - match_offset);
            if (UNLIKELY((scan_start ^ cand_start) & hit_mask) == 0)
                return 1u;
            if (--chain_length == 0 || (cur_match = prev[cur_match & W_MASK(s)]) <= limit)
                return 0u;
        }
    }
    return 0u;
}

/* ===========================================================================
 * Priced adaptive block splitting.
 *
 * The open block's symbol frequencies are snapshotted every
 * SPLIT_SLOT_TOKENS committed sequences. At flush time each snapshot point is
 * priced as a split: both halves are costed with freshly fitted trees (dynamic,
 * fixed and stored all considered, exactly like zng_tr_flush_block) plus the
 * dynamic header bits counted with send_tree's run rules. The cheapest split
 * wins; the left half is flushed as its own block and the right half stays
 * open, to be re-evaluated on later flushes.
 */

/* Bit cost of encoding the given per-symbol frequencies as one fixed-Huffman block, header included.
 * The fixed lengths are RFC1951's: literals 0-143 at 8 bits, 144-255 at 9, length codes 7/8, distances 5. */
static uint32_t split_fixed_bits(const uint16_t *lit_freq, const uint16_t *dist_freq) {
    uint32_t bits = 3u;   /* bfinal + btype */

    /* Literal symbols: RFC fixed lengths, 8 bits below 144, 9 above. */
    for (int i = 0; i < 144; i++)
        bits += (uint32_t)lit_freq[i] * 8u;
    for (int i = 144; i < LITERALS; i++)
        bits += (uint32_t)lit_freq[i] * 9u;
    /* End-of-block symbol (freq slot LITERALS): 7-bit fixed code; derived
     * snapshots of a flushed left half carry exactly one. */
    bits += (uint32_t)lit_freq[LITERALS] * 7u;
    /* Length codes: lc 0-22 at 7 bits, lc 23-28 at 8 (per static_ltree), plus
     * xtra_len extra bits. */
    for (int i = LITERALS + 1; i < L_CODES; i++) {
        const int len_code = i - LITERALS - 1;
        bits += (uint32_t)lit_freq[i] * ((len_code <= 22 ? 7u : 8u) + xtra_len[len_code]);
    }
    /* Distance codes: 5-bit fixed symbol plus extra bits. */
    for (int i = 0; i < D_CODES; i++)
        bits += (uint32_t)dist_freq[i] * (5u + xtra_dist[i]);

    return bits;
}

/* Cheapest single-block encoding of this half: dynamic, fixed or stored,
 * mirroring zng_tr_flush_block's selection (stored is bytes*8 + 4 header
 * bytes, as that function compares it). */
static uint32_t split_half_bits(deflate_state *s, const uint16_t *lit_freq,
                                const uint16_t *dist_freq, uint32_t bytes) {
    /* Exact dynamic cost via the real tree builders (same merge order and
     * forced dummy leaves as the emitted block), against fixed and stored. */
    uint32_t bits = zng_tr_exact_block_bits(s, lit_freq, dist_freq);

    const uint32_t fixed = split_fixed_bits(lit_freq, dist_freq);
    if (fixed < bits)
        bits = fixed;
    const uint32_t stored = bytes * 8u + 32u;
    if (stored < bits)
        bits = stored;

    return bits;
}

/* Drop the open-block bookkeeping after a full flush. */
static void blk_reset(deflate_state *s) {
    s->blk_tokens = 0;
    s->blk_bytes = 0;
    s->blk_next_snap = SPLIT_SLOT_TOKENS;
    s->blk_slots = 0;
    s->blk_deferred_tok = 0;
}

/* A snapshot slot is a split candidate when both halves hold at least
 * SPLIT_MIN_SIDE_TOKENS tokens (below that the refit cannot pay for the
 * second block header) and neither half would need a stored fallback wider
 * than MAX_STORED. */
static int blk_slot_viable(const deflate_state *s, uint8_t slot) {
    const uint32_t tok = s->blk_snap_tok[slot];
    if (tok < SPLIT_MIN_SIDE_TOKENS || s->blk_tokens - tok < SPLIT_MIN_SIDE_TOKENS)
        return 0;
    const uint32_t right_bytes = s->blk_bytes - s->blk_snap_bytes[slot];
    if (s->blk_snap_bytes[slot] > MAX_STORED || right_bytes > MAX_STORED)
        return 0;
    return 1;
}

/* Token count at which the open block should be split, or 0 when no split
 * beats encoding the whole block as one. */
static uint32_t blk_find_split(deflate_state *s) {
    /* Self-heal stale bookkeeping: any flush that bypassed the split sites
     * (e.g. FLUSH_BLOCK returning need_more with an empty output buffer)
     * already consumed the open block, so the snapshot chain no longer maps
     * onto sym_buf and must be dropped. */
#ifdef LIT_MEM
    if (UNLIKELY(s->sym_next != s->blk_tokens)) {
        blk_reset(s);
        return 0;
    }
#else
    if (UNLIKELY(s->sym_next != 3u * s->blk_tokens)) {
        blk_reset(s);
        return 0;
    }
#endif
    if (s->blk_slots == 0 || s->blk_tokens < SPLIT_MIN_SIDE_TOKENS * 2)
        return 0;

    /* The open block's running frequencies (END symbol included). */
    uint16_t tot_lit[L_CODES];
    uint16_t tot_dist[D_CODES];
    for (int i = 0; i < L_CODES; i++)
        tot_lit[i] = s->dyn_ltree[i].Freq;
    for (int i = 0; i < D_CODES; i++)
        tot_dist[i] = s->dyn_dtree[i].Freq;

    /* Pre-scan for a viable slot before paying for the baseline tree build:
     * a flush with none skips the exact pricing entirely. */
    {
        int viable = 0;
        for (uint8_t slot = 0; slot < s->blk_slots; slot++)
            if (blk_slot_viable(s, slot)) {
                viable = 1;
                break;
            }
        if (!viable)
            return 0;
    }

    uint32_t best_bits = split_half_bits(s, tot_lit, tot_dist, s->blk_bytes);
    uint32_t best_tok = 0;

    for (uint8_t slot = 0; slot < s->blk_slots; slot++) {
        if (!blk_slot_viable(s, slot))
            continue;
        const uint32_t tok = s->blk_snap_tok[slot];

        /* Right half = totals minus this snapshot, plus its own END symbol. */
        uint16_t right_lit[L_CODES];
        uint16_t right_dist[D_CODES];
        for (int i = 0; i < L_CODES; i++)
            right_lit[i] = (uint16_t)(tot_lit[i] - s->blk_snap_l[slot][i]);
        for (int i = 0; i < D_CODES; i++)
            right_dist[i] = (uint16_t)(tot_dist[i] - s->blk_snap_d[slot][i]);
        right_lit[END_BLOCK] = 1;

        const uint32_t bits = split_half_bits(s, s->blk_snap_l[slot], s->blk_snap_d[slot], s->blk_snap_bytes[slot])
                             + split_half_bits(s, right_lit, right_dist, s->blk_bytes - s->blk_snap_bytes[slot]);
        if (bits + SPLIT_MARGIN_BITS < best_bits) {
            best_bits = bits;
            best_tok = tok;
        }
    }

    return best_tok;
}

/* The slot holding a snapshot token count (snapshots are strictly
 * increasing, so the first match is the only one). Callers pass a token
 * count blk_find_split read from this array, so a miss is impossible; the
 * bound turns any future invariant break into an assert instead of a
 * runaway scan. */
static uint8_t blk_slot_of(const deflate_state *s, uint32_t tok) {
    for (uint8_t slot = 0; slot < s->blk_slots; slot++)
        if (s->blk_snap_tok[slot] == tok)
            return slot;
    Assert(0, "blk_slot_of: token not found in snapshots");
    return 0;
}

/* Flush the open block's first tok sequences as their own block and re-seed
 * all state for the remainder, which stays open. */
static void blk_do_split(deflate_state *s, unsigned char *window, uint32_t tok) {
    /* The token count came from a snapshot; find its slot. */
    const uint8_t slot = blk_slot_of(s, tok);
    const uint32_t left_bytes = s->blk_snap_bytes[slot];

    /* Defensive: the invariant in blk_find_split guarantees tok covers real
     * tokens; refuse rather than memmove past the buffer if it ever fails. */
#ifdef LIT_MEM
    if (UNLIKELY(tok > s->sym_next))
        return;
#else
    if (UNLIKELY(3u * (unsigned int)tok > s->sym_next))
        return;
#endif

    /* Swap in the left half's frequencies and flush it as a full block. */
    uint16_t tot_lit[L_CODES];
    uint16_t tot_dist[D_CODES];
    for (int i = 0; i < L_CODES; i++) {
        tot_lit[i] = s->dyn_ltree[i].Freq;
        s->dyn_ltree[i].Freq = s->blk_snap_l[slot][i];
    }
    for (int i = 0; i < D_CODES; i++) {
        tot_dist[i] = s->dyn_dtree[i].Freq;
        s->dyn_dtree[i].Freq = s->blk_snap_d[slot][i];
    }
    const unsigned int old_next = s->sym_next;
#ifdef LIT_MEM
    s->sym_next = (unsigned int)tok;
#else
    s->sym_next = 3u * (unsigned int)tok;
#endif
    /* Mirror FLUSH_BLOCK_ONLY: a slid-out block start has no window bytes. */
    zng_tr_flush_block(s, (s->block_start >= 0 ? &window[(unsigned)s->block_start] : NULL), left_bytes, 0);
    PREFIX(flush_pending)(s->strm);

    /* Shift the remaining tokens to the front of the symbol buffers. */
#ifdef LIT_MEM
    memmove(s->d_buf, s->d_buf + tok, (old_next - tok) * sizeof(uint16_t));
    memmove(s->l_buf, s->l_buf + tok, old_next - tok);
    s->sym_next = old_next - tok;
#else
    {
        const unsigned int keep = old_next - 3u * (unsigned int)tok;
        /* sym_buf shares the pending_buf allocation (it starts lit_bufsize +
         * PENDING_BUF_PAD bytes in). If the flush above drained only part of
         * the block because the output buffer is small, undrained output still
         * occupies [pending_out .. pending_out+pending) in that shared region,
         * and moving the right half down would clobber it. Defer the physical
         * move until zng_deflate re-enters with pending drained: record the
         * split point and restore the full extent so the open block keeps its
         * true size in the meantime. */
        const uint32_t live_off = (uint32_t)(s->pending_out - s->pending_buf);
        if (live_off + s->pending <= (uint32_t)(s->sym_buf - s->pending_buf)
            || live_off >= (uint32_t)(s->sym_buf - s->pending_buf) + keep) {
            memmove(s->sym_buf, s->sym_buf + 3u * (unsigned int)tok, keep);
            s->sym_next = keep;
            s->blk_deferred_tok = 0;
        } else {
            s->sym_next = old_next;
            s->blk_deferred_tok = (uint32_t)tok;
        }
    }
#endif

    /* Re-seed the right half's frequencies (it gets a fresh END symbol). */
    for (int i = 0; i < L_CODES; i++)
        s->dyn_ltree[i].Freq = (uint16_t)(tot_lit[i] - s->blk_snap_l[slot][i]);
    for (int i = 0; i < D_CODES; i++)
        s->dyn_dtree[i].Freq = (uint16_t)(tot_dist[i] - s->blk_snap_d[slot][i]);
    s->dyn_ltree[END_BLOCK].Freq = 1;

    /* Advance the open-block bookkeeping and re-base the later snapshots. */
    s->block_start += (int)left_bytes;
    s->blk_tokens -= tok;
    s->blk_bytes -= left_bytes;
    s->blk_next_snap -= tok;
    /* Slots below the split point describe tokens that were just flushed and
     * must be dropped: shift the rebased later snapshots down to slot 0 so
     * every surviving snapshot stays on the right half's baseline. */
    for (uint8_t k = slot + 1; k < s->blk_slots; k++) {
        const uint8_t dst = (uint8_t)(k - slot - 1u);
        for (int i = 0; i < L_CODES; i++)
            s->blk_snap_l[dst][i] = (uint16_t)(s->blk_snap_l[k][i] - s->blk_snap_l[slot][i]);
        for (int i = 0; i < D_CODES; i++)
            s->blk_snap_d[dst][i] = (uint16_t)(s->blk_snap_d[k][i] - s->blk_snap_d[slot][i]);
        /* Every snapshot keeps its own END symbol (init_block's count). */
        s->blk_snap_l[dst][END_BLOCK] = 1;
        s->blk_snap_tok[dst] = s->blk_snap_tok[k] - tok;
        s->blk_snap_bytes[dst] = s->blk_snap_bytes[k] - left_bytes;
    }
    s->blk_slots = (uint8_t)(s->blk_slots - slot - 1u);
    /* If the slot cap stalled snapshotting, the rebased threshold can lag far
     * behind the current token count and re-firing would take one snapshot per
     * commit until it catches up; push it a full interval ahead instead. */
    if (UNLIKELY(s->blk_next_snap < s->blk_tokens))
        s->blk_next_snap = s->blk_tokens + SPLIT_SLOT_TOKENS;
}

/* ===========================================================================
 * Region re-optimization (zopfli GetBestLengths, streaming-scoped).
 *
 * Right before an open block is flushed, re-parse its whole byte span with a
 * full shortest-path DP under one or two pricing models and splice the result
 * in when it beats the pass-1 tokens on exact emitted size. Matches are
 * re-probed against the live hash table, which by then also contains the
 * region's own positions: candidates at or after the probe position were
 * inserted after that position was parsed (they did not exist in its pass-1
 * view) and are skipped, leaving exactly the candidate set pass 1 saw.
 */
#define REOPT_MIN_BYTES   2048u
/* Scratch cap; larger open blocks are left to the priced splitter. */
#define REOPT_MAX_BYTES   40960u
/* Required exact-size win over the pass-1 tokens before splicing. */
#define REOPT_MARGIN_BITS 8u

/* Token cost under explicit composed per-code tables (symbol length plus
 * extra bits, one entry per length/distance code). */
Z_FORCEINLINE static uint32_t reopt_match_cost(const uint8_t *ml, const uint8_t *md,
                                               uint32_t len, uint32_t dist) {
    const uint32_t lcode = zng_length_code[len - STD_MIN_MATCH];
    const uint32_t dcode = d_code(dist - 1);
    return ml[lcode] + md[dcode];
}

Z_FORCEINLINE static uint32_t reopt_lit_cost(const uint8_t *ll, uint32_t c) {
    return ll[c];
}

/* Bounded match length between two window positions, never reading past
 * max_len bytes (unlike compare256, which always reads STD_MAX_MATCH). */
static Z_FORCEINLINE uint32_t region_cmp(const unsigned char *a, const unsigned char *b,
                                         uint32_t max_len) {
    uint32_t len = 0;
    while (len < max_len && a[len] == b[len])
        len++;
    return len;
}

/* Best match at an already-parsed region position: a simplified slow_roll walk over candidates strictly before probe_pos,
 * recording the closest distance that reached each length in sublen[] like longest_match_extreme. It omits the entry
 * offset re-seed (there is no discard floor here) and compares byte-wise up to max_len instead of via compare256.
 * Returns the best length (0 when none) and sets *out_dist to its distance. */
static uint32_t region_probe(const deflate_state *s, const unsigned char *window,
                             uint32_t probe_pos, uint32_t max_len,
                             uint16_t *sublen, uint32_t *out_dist) {
    if (UNLIKELY(max_len < STD_MIN_MATCH))
        return 0;

    const Pos *const prev = s->prev;
    const Pos *const head = s->head;
    const unsigned wmask = W_MASK(s);
    uint32_t chain_length = s->max_chain_length;
    uint32_t best_len = STD_MIN_MATCH - 1u; /* no discard floor for region probes */
    uint32_t best_dist = 0;
    const unsigned char *const scan = window + probe_pos;
    const uint32_t limit = probe_pos > MAX_DIST(s) ? probe_pos - MAX_DIST(s) : 0u;

    memset(sublen, 0, (STD_MAX_MATCH + 1) * sizeof(uint16_t));

    uint32_t cur_match = head[roll_lookup(window, probe_pos)];
    const uint64_t scan_start = max_len >= sizeof(uint64_t) ? zng_memread_8(scan) : 0;
    /* A candidate must beat best_len on its first best_len+1 bytes. */
    const uint64_t hit_mask = zng_first_bytes_mask64(best_len + 1u);

    while (chain_length > 0u) {
        /* Future positions share this key but did not exist when probe_pos
         * was parsed; skip them without breaking (older valid entries sit
         * further down the chain). Skips consume budget to bound the walk. */
        if (UNLIKELY(cur_match >= probe_pos)) {
            chain_length--;
            cur_match = prev[cur_match & wmask];
            continue;
        }
        if (cur_match <= limit)
            break;
        chain_length--;

        uint32_t len = 0;
        if (max_len >= sizeof(uint64_t)) {
            const uint64_t diff = scan_start ^ zng_memread_8(window + cur_match);
            if ((diff & hit_mask) == 0u)
                len = region_cmp(scan, window + cur_match, max_len);
        } else {
            len = region_cmp(scan, window + cur_match, max_len);
        }

        if (len > best_len) {
            /* Record the closest source that reached each newly covered length. */
            for (uint32_t j = best_len + 1u; j <= len; j++)
                sublen[j] = (uint16_t)(probe_pos - cur_match);
            best_len = len;
            best_dist = probe_pos - cur_match;
            /* region_cmp is capped at max_len, so once the whole span is
             * covered no later candidate can change sublen[] or the result:
             * stop walking. */
            if (best_len >= max_len)
                break;
        }

        cur_match = prev[cur_match & wmask];
    }

    if (best_len < STD_MIN_MATCH)
        return 0;
    *out_dist = best_dist;
    return best_len;
}

/* Shortest-path DP over region bytes [start, end) under two length models at once,
 * sharing one probe per position (the probes depend only on window/chain state,
 * not on the pricing model). Writes each model's winning per-position token into
 * its link pair (len 0 marks a literal whose byte is in dist) and returns 1 when
 * both spans are fully coverable (always, via literals). */
static int region_dp_both(const deflate_state *s, const unsigned char *window, uint32_t start,
                          uint32_t end, const uint8_t *llA, const uint8_t *ddA,
                          const uint8_t *llB, const uint8_t *ddB,
                          uint32_t *bestA, uint32_t *bestB,
                          uint16_t *linkA_len, uint16_t *linkA_dist,
                          uint16_t *linkB_len, uint16_t *linkB_dist) {
    const uint32_t nbytes = end - start;
    static const uint32_t classes[] = { 3u, 4u, 5u, 6u, 7u, 8u, 9u, 10u, 11u, 13u, 15u, 17u, 19u, 23u,
                                         27u, 31u, 35u, 43u, 51u, 59u, 67u, 83u, 99u, 115u, 131u, 163u, 195u,
                                         227u, 258u };
    const uint32_t inf = UINT32_MAX / 4u;

    /* Compose per-code match token costs once per model: code length + extra bits. */
    uint8_t mlA[29];
    uint8_t mdA[30];
    uint8_t mlB[29];
    uint8_t mdB[30];
    for (int i = 0; i < 29; i++) {
        mlA[i] = (uint8_t)(llA[LITERALS + 1 + i] + xtra_len[i]);
        mlB[i] = (uint8_t)(llB[LITERALS + 1 + i] + xtra_len[i]);
    }
    for (int i = 0; i < 30; i++) {
        mdA[i] = (uint8_t)(ddA[i] + xtra_dist[i]);
        mdB[i] = (uint8_t)(ddB[i] + xtra_dist[i]);
    }

    /* Every position unreachable except the span start. */
    for (uint32_t i = 1; i <= nbytes; i++) {
        bestA[i] = inf;
        bestB[i] = inf;
    }
    bestA[0] = 0;
    bestB[0] = 0;

    /* Edges out of each reachable source position, for both models. */
    for (uint32_t q = 0; q < nbytes; q++) {
        const uint32_t baseA = bestA[q];
        const uint32_t baseB = bestB[q];
        if (UNLIKELY(baseA == inf && baseB == inf))
            continue;
        const uint32_t abs_pos = start + q;

        /* The span end is the binding limit: bytes past it belong to later
         * blocks and cannot be covered here. Probe once, price twice. */
        uint32_t max_len = end - abs_pos;
        if (max_len > STD_MAX_MATCH)
            max_len = STD_MAX_MATCH;

        uint16_t sublen[STD_MAX_MATCH + 1];
        uint32_t bdist = 0;
        const uint32_t blen = (max_len >= STD_MIN_MATCH)
                                  ? region_probe(s, window, abs_pos, max_len, sublen, &bdist)
                                  : 0u;

        if (baseA != inf) {
            const uint32_t lit = baseA + reopt_lit_cost(llA, window[abs_pos]);
            if (lit < bestA[q + 1u]) {
                bestA[q + 1u] = lit;
                linkA_len[q + 1u] = 0;
                linkA_dist[q + 1u] = (uint16_t)window[abs_pos];
            }
            for (uint32_t class_i = 0; class_i < sizeof(classes) / sizeof(classes[0]); class_i++) {
                const uint32_t k = classes[class_i];
                if (k > max_len)
                    break; /* classes are sorted ascending */
                if (sublen[k] == 0)
                    continue;
                const uint32_t cost = baseA + reopt_match_cost(mlA, mdA, k, sublen[k]);
                if (cost < bestA[q + k]) {
                    bestA[q + k] = cost;
                    linkA_len[q + k] = (uint16_t)k;
                    linkA_dist[q + k] = sublen[k];
                }
            }
            if (blen > 10u && blen <= max_len) {
                const uint32_t cost = baseA + reopt_match_cost(mlA, mdA, blen, bdist);
                if (cost < bestA[q + blen]) {
                    bestA[q + blen] = cost;
                    linkA_len[q + blen] = (uint16_t)blen;
                    linkA_dist[q + blen] = (uint16_t)bdist;
                }
            }
        }

        if (baseB != inf) {
            const uint32_t lit = baseB + reopt_lit_cost(llB, window[abs_pos]);
            if (lit < bestB[q + 1u]) {
                bestB[q + 1u] = lit;
                linkB_len[q + 1u] = 0;
                linkB_dist[q + 1u] = (uint16_t)window[abs_pos];
            }
            for (uint32_t class_i = 0; class_i < sizeof(classes) / sizeof(classes[0]); class_i++) {
                const uint32_t k = classes[class_i];
                if (k > max_len)
                    break; /* classes are sorted ascending */
                if (sublen[k] == 0)
                    continue;
                const uint32_t cost = baseB + reopt_match_cost(mlB, mdB, k, sublen[k]);
                if (cost < bestB[q + k]) {
                    bestB[q + k] = cost;
                    linkB_len[q + k] = (uint16_t)k;
                    linkB_dist[q + k] = sublen[k];
                }
            }
            if (blen > 10u && blen <= max_len) {
                const uint32_t cost = baseB + reopt_match_cost(mlB, mdB, blen, bdist);
                if (cost < bestB[q + blen]) {
                    bestB[q + blen] = cost;
                    linkB_len[q + blen] = (uint16_t)blen;
                    linkB_dist[q + blen] = (uint16_t)bdist;
                }
            }
        }
    }

    return bestA[nbytes] != inf && bestB[nbytes] != inf;
}

/* Grow the re-optimization scratch to cover a region of this many bytes, freeing any smaller previous block.
 * Returns 0 (leaving s->re_scratch unchanged) when the allocation fails. */
static int reopt_ensure_scratch(deflate_state *s, uint32_t n) {
    if (n <= s->re_cap)
        return 1;
    const uint32_t cap = n > REOPT_MAX_BYTES ? REOPT_MAX_BYTES : n;
    /* Layout: two best arrays (cap+1 u32 each), then four link arrays of cap+1
     * u16 each: the DP writes links indexed by destination position, which
     * reaches n. */
    uint32_t *blk = (uint32_t *)s->strm->zalloc(s->strm->opaque, 1u,
                                                2u * (cap + 1u) * sizeof(uint32_t) + (cap + 1u) * 4u * sizeof(uint16_t));
    if (blk == NULL)
        return 0;
    if (s->re_scratch != NULL)
        s->strm->zfree(s->strm->opaque, s->re_scratch);
    s->re_scratch = blk;
    s->re_cap = cap;
    return 1;
}

/* Tally one DP path's tokens into a fresh histogram: len 0 is a literal at
 * link_dist[p], otherwise a match of length len from that distance. The arrays
 * are zeroed first (END_BLOCK stays the caller's to set). */
static void reopt_tally_path(const uint16_t *link_len, const uint16_t *link_dist,
                             uint32_t nbytes, uint16_t *freq_lit, uint16_t *freq_dist) {
    for (int i = 0; i < L_CODES; i++)
        freq_lit[i] = 0;
    for (int i = 0; i < D_CODES; i++)
        freq_dist[i] = 0;
    /* Walk the path newest first, exactly as region_dp wrote it. */
    for (uint32_t p = nbytes; p > 0u;) {
        const uint32_t len = link_len[p];
        if (len == 0) {
            freq_lit[link_dist[p]]++;
            p--;
        } else {
            freq_lit[LITERALS + 1 + zng_length_code[len - STD_MIN_MATCH]]++;
            freq_dist[d_code(link_dist[p] - 1u)]++;
            p -= len;
        }
    }
}

/* Subtract one pass-1 token from the running model, saturating at zero (a
 * decay since commit may have halved the contribution away). Advances *off by
 * the token's covered bytes. */
static void reopt_subtract_token(deflate_state *s, uint32_t odist, uint32_t len_field, uint32_t *off) {
    if (odist == 0) {
        if (s->ext_freq[len_field] > 0)
            s->ext_freq[len_field]--;
        (*off)++;
    } else {
        const uint32_t full_len = len_field + STD_MIN_MATCH;
        const uint32_t len_slot = LITERALS + 1 + zng_length_code[full_len - STD_MIN_MATCH];
        const uint32_t dist_slot = LITERALS + 29 + 1 + d_code(odist - 1u);
        if (s->ext_freq[len_slot] > 0)
            s->ext_freq[len_slot]--;
        if (s->ext_freq[dist_slot] > 0)
            s->ext_freq[dist_slot]--;
        *off += full_len;
    }
}

/* Re-parse the open block with full-block DPs and splice in the winner when
 * it beats the pass-1 tokens on exact emitted size. No-op outside the size
 * window or when no candidate wins by REOPT_MARGIN_BITS. */
static void region_reoptimize(deflate_state *s, unsigned char *window) {
    if (UNLIKELY(s->block_start < 0))
        return; /* the window slid past the block start: bytes are gone */
    const uint32_t start = (uint32_t)s->block_start;
    const uint32_t end = (uint32_t)s->strstart;
    const uint32_t nbytes = end - start;
    if (UNLIKELY(nbytes < REOPT_MIN_BYTES || nbytes > REOPT_MAX_BYTES))
        return;
    if (UNLIKELY(!reopt_ensure_scratch(s, nbytes)))
        return;

    uint32_t *bestA = s->re_scratch;
    uint32_t *bestB = bestA + (s->re_cap + 1u);
    uint16_t *linkA_len = (uint16_t *)(bestB + (s->re_cap + 1u));
    uint16_t *linkA_dist = linkA_len + (s->re_cap + 1u);
    uint16_t *linkB_len = linkA_dist + (s->re_cap + 1u);
    uint16_t *linkB_dist = linkB_len + (s->re_cap + 1u);

    /* Baseline: exact size of the pass-1 tokens (END symbol included, as in
     * init_block). */
    uint16_t tot_lit[L_CODES];
    uint16_t tot_dist[D_CODES];
    for (int i = 0; i < L_CODES; i++)
        tot_lit[i] = s->dyn_ltree[i].Freq;
    for (int i = 0; i < D_CODES; i++)
        tot_dist[i] = s->dyn_dtree[i].Freq;
    const uint32_t base_bits = split_half_bits(s, tot_lit, tot_dist, nbytes);

    /* Pass B's model: a fit to this block's own frequencies. */
    uint32_t freq_fit_l[L_CODES];
    uint32_t freq_fit_d[D_CODES];
    uint8_t ll_local[L_CODES];
    uint8_t dd_local[D_CODES];
    /* Contiguous copies: huff_lengths wants packed frequencies, not ct_data. */
    for (int i = 0; i < L_CODES; i++)
        freq_fit_l[i] = tot_lit[i];
    for (int i = 0; i < D_CODES; i++)
        freq_fit_d[i] = tot_dist[i];
    huff_lengths(freq_fit_l, L_CODES, ll_local);
    huff_lengths(freq_fit_d, D_CODES, dd_local);

    /* Passes A and B share one probe per position: price with the running
     * model and the local fit in a single interleaved DP. */
    memset(linkA_len, 0, nbytes * sizeof(uint16_t));
    memset(linkB_len, 0, nbytes * sizeof(uint16_t));
    if (UNLIKELY(!region_dp_both(s, window, start, end, s->ext_ll_len, s->ext_d_len,
                                  ll_local, dd_local, bestA, bestB,
                                  linkA_len, linkA_dist, linkB_len, linkB_dist))) {
        return;
    }

    /* Tally each candidate's histogram and price it exactly. */
    uint16_t freqA_lit[L_CODES];
    uint16_t freqA_dist[D_CODES];
    uint16_t freqB_lit[L_CODES];
    uint16_t freqB_dist[D_CODES];
    reopt_tally_path(linkA_len, linkA_dist, nbytes, freqA_lit, freqA_dist);
    reopt_tally_path(linkB_len, linkB_dist, nbytes, freqB_lit, freqB_dist);

    freqA_lit[END_BLOCK] = 1;
    freqB_lit[END_BLOCK] = 1;
    const uint32_t bitsA = split_half_bits(s, freqA_lit, freqA_dist, nbytes);
    const uint32_t bitsB = split_half_bits(s, freqB_lit, freqB_dist, nbytes);
    /* Pick the winner: pass-1 tokens stay unless a candidate wins clearly. */
    int win = 0;
    uint32_t win_bits = base_bits;
    if (bitsA + REOPT_MARGIN_BITS < base_bits) {
        win = 1;
        win_bits = bitsA;
    }
    if (bitsB + REOPT_MARGIN_BITS < base_bits && bitsB < win_bits)
        win = 2;
    if (win == 0)
        return;
    const uint16_t *link_len = win == 1 ? linkA_len : linkB_len;
    const uint16_t *link_dist = win == 1 ? linkA_dist : linkB_dist;

    /* Reconstruct the winning path; bestA[] is free now that both DPs are
     * done, so it doubles as the token store (len | dist << 16). */
    uint32_t *path = bestA;
    uint32_t ntok = 0;
    for (uint32_t p = nbytes; p > 0u;) {
        const uint32_t len = link_len[p];
        path[ntok++] = len | ((uint32_t)link_dist[p] << 16);
        p -= len == 0 ? 1u : len;
    }

    /* The re-tally below rewrites sym_buf from zero; a winner with more
     * tokens than the pass-1 parse holds would run past sym_end and into the
     * state struct that follows pending_buf. Skip such splices. */
#ifdef LIT_MEM
    if (UNLIKELY(ntok > s->sym_end))
        return;
#else
    if (UNLIKELY(3u * ntok > s->sym_end))
        return;
#endif

    /* Subtract the pass-1 tokens from the running model. Saturated: a decay
     * since commit may have halved an old token's contribution away. */
#ifdef LIT_MEM
    for (uint32_t tok_i = 0, off = 0u; off < nbytes;) {
        reopt_subtract_token(s, s->d_buf[tok_i], s->l_buf[tok_i], &off);
        tok_i++;
    }
#else
    for (uint32_t tok_i = 0, off = 0u; off < nbytes;) {
        /* Byte-wise like compress_block: a u32 read would pull the next
         * token's low distance byte into the length field. */
        const uint32_t odist = s->sym_buf[3u * tok_i] + ((uint32_t)s->sym_buf[3u * tok_i + 1u] << 8);
        const uint32_t len_field = s->sym_buf[3u * tok_i + 2u];
        reopt_subtract_token(s, odist, len_field, &off);
        tok_i++;
    }
#endif

    /* Reset the block state like init_block and re-tally the winner tokens (newest first, so each tally lands where pass 1
     * put it), which also rewrites sym_buf in place. */
    for (int i = 0; i < L_CODES; i++)
        s->dyn_ltree[i].Freq = 0;
    for (int i = 0; i < D_CODES; i++)
        s->dyn_dtree[i].Freq = 0;
    for (int i = 0; i < BL_CODES; i++)
        s->bl_tree[i].Freq = 0;
    s->dyn_ltree[END_BLOCK].Freq = 1;
    s->opt_len = s->static_len = 0;
    s->sym_next = 0;

    for (uint32_t i = 0; i < ntok; i++) {
        const uint32_t tok = path[ntok - 1u - i];
        const uint32_t len = tok & 0xffffu;
        const uint32_t dist = tok >> 16;
        if (len == 0) {
            zng_tr_tally_lit(s, (unsigned char)dist);
            s->ext_freq[dist]++;
        } else {
            zng_tr_tally_dist(s, dist, len - STD_MIN_MATCH);
            s->ext_freq[LITERALS + 1 + zng_length_code[len - STD_MIN_MATCH]]++;
            s->ext_freq[LITERALS + 29 + 1 + d_code(dist - 1u)]++;
        }
    }

#ifdef ZLIB_DEBUG
# ifdef LIT_MEM
    Assert(s->sym_next == ntok, "reopt splice token count");
# else
    Assert(s->sym_next == 3u * ntok, "reopt splice token count");
# endif
#endif
}

/* Flush the open block as a full non-final block, trying a priced split
 * first and re-optimizing when no split wins. Returns 1 when the caller must
 * return need_more (pending output undrained or the output buffer is full),
 * 0 when the flush completed and the block state was reset. */
static int extreme_flush_open_block(deflate_state *s, unsigned char *window) {
    const uint32_t split_tok = blk_find_split(s);
    if (UNLIKELY(split_tok != 0)) {
        blk_do_split(s, window, split_tok);
        if (UNLIKELY(s->pending != 0))
            return 1;
    } else {
        region_reoptimize(s, window);
    }
    FLUSH_BLOCK_ONLY(s, window, 0);
    blk_reset(s);
    return (s->strm->avail_out == 0) ? 1 : 0;
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
        extreme_model_reset(s);
        blk_reset(s);
        /* An open block inherited from another level adopts its span so a
         * split can price the inherited prefix against the new suffix. */
        if (UNLIKELY(s->sym_next != 0)) {
#ifdef LIT_MEM
            s->blk_tokens = s->sym_next;
#else
            s->blk_tokens = s->sym_next / 3u;
#endif
            s->blk_bytes = (uint32_t)(s->strstart - s->block_start);
        }
        s->blk_next_snap = s->blk_tokens + SPLIT_SLOT_TOKENS;
        if (s->blk_tokens >= SPLIT_MIN_SIDE_TOKENS) {
            for (int i = 0; i < L_CODES; i++)
                s->blk_snap_l[0][i] = s->dyn_ltree[i].Freq;
            for (int i = 0; i < D_CODES; i++)
                s->blk_snap_d[0][i] = s->dyn_dtree[i].Freq;
            s->blk_snap_tok[0] = s->blk_tokens;
            s->blk_snap_bytes[0] = s->blk_bytes;
            s->blk_slots = 1;
        }
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
        /* Length->distance frontier for the cursor position, recorded by the
         * C0 search itself: per length, the distance of the candidate that
         * first reached it (advisory -- find_best_match's primary stays the
         * authoritative one). */
        uint16_t sublen[STD_MAX_MATCH + 1];
        s->prev_length = match_floor;
        cand[0] = find_best_match(s, hash_head, max_dist, tail_credit, sublen);

        /* Candidates one..EXTREME_PROBE_DEPTH positions ahead, looked up
         * without inserting so the rolling hash state stays sequential.
         * find_best_match wants the chain head position, so resolve each probe
         * key through s->head. */
        for (int k = 1; k <= EXTREME_PROBE_DEPTH; k++) {
            if (UNLIKELY(s->lookahead < WANT_MIN_MATCH + (uint32_t)k))
                break;
            s->strstart = pos + (uint32_t)k;
            cand[k] = find_best_match(s, s->head[roll_lookup(window, pos + (uint32_t)k)], max_dist, tail_credit, NULL);
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

        /* Unified candidate list: the C0 frontier options at position 0 (or
         * a plain literal when C0 has no match), then the ahead probes. */
        struct seq { uint32_t pos_off, match_len, match_dist; };
        struct seq seqs[32 + EXTREME_PROBE_DEPTH];
        int nseq = 0;
        int nopt = 0;

        /* Alternative (length, distance) options at the cursor from the frontier walk: the individual 3-10 symbols,
         * every zlib-ng length-class edge up to 258 (the edges where the extra-bit count changes), and the longest
         * match itself. Within a class the price is flat in length, so only class edges matter; every option stays
         * legal because a match may always be emitted shorter than its measured maximum. */
        {
            /* The true longest of the two same-key candidates: frontier
             * class points must not be truncated by a swapped-in near match,
             * so filter against the longer of the pair. */
            const uint32_t c0_longest = MAX(cand[0].match_length, cand[0].alt_len);
            if (c0_longest >= STD_MIN_MATCH) {
                static const uint32_t opt_points[] = { 3, 4, 5, 6, 7, 8, 9, 10, 11, 13, 15, 17, 19, 23,
                                                            27, 31, 35, 43, 51, 59, 67, 83, 99, 115, 131, 163, 195,
                                                            227, 258 };
                const uint32_t max_frontier_dist = (match_floor > 0)
                    ? EXTREME_FRONTIER_MAX_DIST_FLOORED : EXTREME_FRONTIER_MAX_DIST;
                for (unsigned p = 0; p < sizeof(opt_points) / sizeof(opt_points[0]); p++) {
                    const uint32_t k = opt_points[p];
                    if (k > match_floor && k < c0_longest && sublen[k] != 0
                        && sublen[k] <= max_frontier_dist) {
                        seqs[nseq].pos_off = 0;
                        seqs[nseq].match_len = k;
                        seqs[nseq].match_dist = sublen[k];
                        nseq++;
                        nopt++;
                    }
                }
                /* The primary keeps the template's authoritative distance;
                 * the other candidate is offered explicitly so its exact
                 * length (possibly between class points) survives. */
                seqs[nseq].pos_off = 0;
                seqs[nseq].match_len = cand[0].match_length;
                seqs[nseq].match_dist = cand[0].strstart - cand[0].match_start;
                nseq++;
                nopt++;
                if (cand[0].alt_len != 0 && cand[0].alt_len < c0_longest) {
                    seqs[nseq].pos_off = 0;
                    seqs[nseq].match_len = cand[0].alt_len;
                    seqs[nseq].match_dist = cand[0].alt_dist;
                    nseq++;
                    nopt++;
                }
            }
        }

        if (nopt == 0) {
            seqs[nseq].pos_off = 0;
            seqs[nseq].match_len = 1;
            seqs[nseq].match_dist = 0;
            nseq++;
        }
        for (int k = 1; k < ncand; k++) {
            seqs[nseq].pos_off = (uint32_t)k;
            seqs[nseq].match_len = cand[k].match_length;
            seqs[nseq].match_dist = cand[k].strstart - cand[k].match_start;
            nseq++;
        }
        Assert(nseq <= 32 + EXTREME_PROBE_DEPTH, "seqs overflow");

        /* Lazy decision: emit the minimum-score sequence, where score is the
         * explicit token bits minus the tail credit per advanced byte (see
         * above). Ties go to the longest advance. */
        int64_t best_score = INT64_MAX;
        uint32_t best_advance = 0;
        int best_seq = 0;
        /* Positions after pos already inserted into the hash table by the
         * cut-and-rematch pre-insert below; the commit-time batch insert must
         * not re-insert them (the rolling hash state is sequential). */
        uint32_t preinserted = 0;
#if LIT_GATE > 0
        /* Regime gate: when the most frequent literal needs more than
         * LIT_GATE bits, literals are expensive and a found C0 option almost
         * always beats the DP alternatives, so the decision keeps only the C0 options. On data
         * dominated by one cheap literal (filter bytes, constant stretches)
         * keep the full two-ply DP, which pays off there. */
        const int nseq_limit = (nopt > 0 && s->ext_mode_lit_cost > LIT_GATE) ? nopt : nseq;
#else
        const int nseq_limit = nseq;
#endif
        for (int q = 0; q < nseq_limit; q++) {
            uint32_t cost = 0;
            /* Literal gap bytes before the candidate match. */
            for (int i = 0; i < (int)seqs[q].pos_off; i++)
                cost += lit_cost(s, window[pos + (uint32_t)i]);
            uint32_t advance;
            if (seqs[q].match_len == 1) {
                cost += lit_cost(s, window[pos + seqs[q].pos_off]);
                advance = seqs[q].pos_off + 1u;
            } else {
                cost += match_cost(s, seqs[q].match_len, seqs[q].match_dist);
                advance = seqs[q].pos_off + seqs[q].match_len;
            }
            const int64_t score = (int64_t)cost - (int64_t)tail_credit * (int64_t)advance;
            if (score < best_score || (score == best_score && advance > best_advance)) {
                best_score = score;
                best_advance = advance;
                best_seq = q;
            }
        }

        /* Map the winner back onto the candidate array: a position-0 win
         * stores its (length, distance) option in cand[0] so the rephase,
         * cut and commit blocks below see the committed match unchanged. */
        int best_k;
        if (nopt > 0 && seqs[best_seq].pos_off == 0) {
            cand[0].match_length = seqs[best_seq].match_len;
            cand[0].match_start = pos - seqs[best_seq].match_dist;
            best_k = 0;
        } else
            best_k = (int)seqs[best_seq].pos_off;

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

        /* Cut-and-rematch (backward re-phase): M(L) followed by two literals
         * can lose to M(L-1) plus a match starting at q = pos+L-1, the byte
         * the cut frees. Three preconditions:
         *
         *  - Cutting must not cross into a length code with more extra bits.
         *    Length 258 is the only top-of-table no-extra-bits case (code 28,
         *    extra 0); cutting it to 257 pays ~5 bits per token, which on
         *    long-match data dwarfs any literal savings. The explicit scores
         *    below price the rest of the boundary cases.
         *  - Both bytes after the match must really be literals in the
         *    un-cut parse, so probe N1 = pos+L and N2 = pos+L+1 exactly as
         *    the next round's C0 search would. That requires the pending
         *    match's covered region to be visible in the hash table first,
         *    so insert [pos+1 .. pos+L-2] now (the gate lookahead >= L+2
         *    keeps every key within valid data).
         *  - q itself is held back from insertion until after the decision:
         *    on a cut, the next iteration's top-of-loop insert_roll(q) is the
         *    sequential continuation; on a keep, insert q here so the next
         *    round's insert_roll(N1) stays sequential.
         *
         * Scoring uses explicit token bits only, no tail credit: the gate
         * makes the keep side exact (b1, b2 are literals), and M' at q is
         * re-found by the next round with the same visible sources, so both
         * sides price what will actually be emitted. A credit term would
         * double-count M''s bytes -- credited here and again when the next
         * round finds them as C0 -- biasing the decision toward cutting. */
        if (best_k == 0 && cand[0].match_length >= STD_MIN_MATCH + 1u
            && s->lookahead >= cand[0].match_length + 2u) {
            const uint32_t cut_len = cand[0].match_length;
            const uint32_t len_code_full = zng_length_code[cut_len - STD_MIN_MATCH];
            const uint32_t len_code_cut  = zng_length_code[cut_len - 1u - STD_MIN_MATCH];
            if (xtra_len[len_code_cut] <= xtra_len[len_code_full] && cut_len - 1u > match_floor) {
                insert_roll_batch(s, window, pos + 1, cut_len - 2u);

                /* Probe N1/N2 as the next round would: a usable match at
                 * either one keeps the full match. The boolean probe exits on
                 * the first hit, so gate failures -- the common case -- cost
                 * one or two comparisons instead of a full chain walk. */
                const unsigned n1_match = match_exists_at(s, window, pos + cut_len, max_dist,
                    match_floor, s->lookahead - cut_len);
                const unsigned n2_match = match_exists_at(s, window, pos + cut_len + 1u, max_dist,
                    match_floor, s->lookahead - cut_len - 1u);

                const uint32_t cut_pos = pos + cut_len - 1u;
                int do_cut = 0;
                if (!n1_match && !n2_match) {
                    /* Both literals: probe q for the freed-byte match. Every
                     * source <= pos+L-2 is inserted, exactly the state the
                     * next round will have when it searches q. */
                    struct match cut_cand;
                    s->prev_length = match_floor;
                    s->strstart = cut_pos;
                    cut_cand = find_best_match(s, s->head[roll_lookup(window, cut_pos)], max_dist, tail_credit, NULL);
                    const uint32_t q_avail = s->lookahead - cut_len + 1u;
                    if (UNLIKELY(cut_cand.match_length > q_avail))
                        cut_cand.match_length = q_avail >= STD_MIN_MATCH ? q_avail : 1u;
                    s->strstart = pos;
                    s->prev_length = 0;

                    if (cut_cand.match_length >= STD_MIN_MATCH && cut_cand.match_length > match_floor) {
                        const uint32_t cand0_dist = pos - cand[0].match_start;
                        const int64_t score_keep = (int64_t)match_cost(s, cut_len, cand0_dist)
                            + lit_cost(s, window[pos + cut_len]) + lit_cost(s, window[pos + cut_len + 1u]);
                        const int64_t score_cut = (int64_t)match_cost(s, cut_len - 1u, cand0_dist)
                            + match_cost(s, cut_cand.match_length, cut_pos - cut_cand.match_start);
                        do_cut = (score_cut < score_keep);
                    }
                }
                if (do_cut) {
                    cand[0].match_length = cut_len - 1u;
                    best_advance = cut_len - 1u;
                    preinserted = cut_len - 2u;
                } else
                    insert_roll(s, window, cut_pos);

                /* In the keep paths q was inserted as well; only the cut path
                 * leaves it pending for the next iteration. */
                if (preinserted == 0)
                    preinserted = cut_len - 1u;
            }
        }

        /* The commit tallies best_k+1 tokens and bflush is only checked after
         * the whole commit, so a multi-token commit starting within that many
         * tokens of sym_end would write past it. Flush the open block first
         * when they would not fit; a single token always fits (sym_next is a
         * multiple of 3 and at most sym_end - 3 here), and the flushed block
         * just ends up to 2 tokens shorter than before. */
#ifdef LIT_MEM
        const uint32_t commit_need = (uint32_t)(best_k + 1);
#else
        const uint32_t commit_need = 3u * (uint32_t)(best_k + 1);
#endif
        if (UNLIKELY(best_k > 0 && s->sym_next + commit_need > s->sym_end)) {
            if (UNLIKELY(extreme_flush_open_block(s, window)))
                return need_more;
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

        /* Open-block bookkeeping for priced splitting: one snapshot of the
         * symbol frequencies every SPLIT_SLOT_TOKENS committed tokens. */
        s->blk_tokens += (uint32_t)(best_k + 1);
        s->blk_bytes += best_advance;
        if (UNLIKELY(s->blk_tokens >= s->blk_next_snap && s->blk_slots < SPLIT_SLOTS)) {
            for (int i = 0; i < L_CODES; i++)
                s->blk_snap_l[s->blk_slots][i] = s->dyn_ltree[i].Freq;
            for (int i = 0; i < D_CODES; i++)
                s->blk_snap_d[s->blk_slots][i] = s->dyn_dtree[i].Freq;
            s->blk_snap_tok[s->blk_slots] = s->blk_tokens;
            s->blk_snap_bytes[s->blk_slots] = s->blk_bytes;
            s->blk_slots++;
            s->blk_next_snap += SPLIT_SLOT_TOKENS;
        }

        /* Refresh the estimated code lengths periodically so the model tracks
         * the per-block trees trees.c actually builds. While the model is cold
         * and the sample spans a wide alphabet (real text, not DNA-like data
         * with five symbols), rebuild faster: noisy small-sample trees hurt
         * narrow-alphabet data but help tracking on broad ones. */
        const uint32_t rebuild_every = (s->ext_rebuilds < 8u && s->ext_distinct_lit >= 32u) ? 128u : EXTREME_REBUILD_EVERY;
        if (UNLIKELY(++s->ext_since_build >= rebuild_every)) {
            s->ext_since_build = 0;
            /* Cost of the most frequent literal: bounds the tail credit (see
             * the decision loop above) and gates the count decay below. */
            uint32_t best_freq = 0;
            uint32_t distinct = 0;
            for (int i = 0; i < LITERALS; i++) {
                if (s->ext_freq[i] != 0)
                    distinct++;
                if (s->ext_freq[i] > best_freq) {
                    best_freq = s->ext_freq[i];
                    s->ext_mode_lit_cost = s->ext_ll_len[i];
                }
            }
            s->ext_distinct_lit = distinct;

            /* Halve the running counts on schedule so recent content dominates
             * the tree; skip while the sample is too small to halve. Uniform
             * scaling leaves the tree shape unchanged, so skipping on flat
             * data costs nothing. */
            if ((s->ext_rebuilds & (EXTREME_DECAY_EVERY / EXTREME_REBUILD_EVERY - 1u)) == 0u && best_freq >= 1024) {
                for (int i = 0; i < 316; i++)
                    s->ext_freq[i] >>= 1;
            }

            huff_lengths(s->ext_freq, LITERALS + 1 + 29, s->ext_ll_len);
            huff_lengths(s->ext_freq + LITERALS + 1 + 29, 30, s->ext_d_len);
            extreme_model_compose(s);
            s->ext_rebuilds++;
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
                if (UNLIKELY(insert_cnt <= preinserted))
                    insert_cnt = 0;
                else
                    insert_cnt -= preinserted;
                if (UNLIKELY(insert_cnt > max_insert - pos))
                    insert_cnt = max_insert - pos;
                if (insert_cnt)
                    insert_roll_batch(s, window, pos + 1u + preinserted, insert_cnt);
            }
        }

        s->lookahead -= best_advance;
        s->strstart += best_advance;

        if (UNLIKELY(bflush)) {
            if (UNLIKELY(extreme_flush_open_block(s, window)))
                return need_more;
        }
    }

    /* Positions still to insert into the hash table when input resumes. */
    s->insert = s->strstart < (STD_MIN_MATCH - 1) ? s->strstart : (STD_MIN_MATCH - 1);
    if (UNLIKELY(flush == Z_FINISH)) {
        const uint32_t split_tok = blk_find_split(s);
        int split_done = 0;
        if (UNLIKELY(split_tok != 0)) {
            /* The final flush cannot defer: once this call returns with
             * finish_started the stream is closed out and no further
             * compression call resumes the right half (zng_deflate skips the
             * compressor when status is FINISH_STATE, avail_in is 0 and the
             * lookahead is empty). Only split when the output buffer can hold
             * the left half's worst case (a stored block) so do_split's flush
             * drains pending fully and both halves emit within this call. A
             * too-tight buffer falls through to a whole-block final flush,
             * whose own finish_started is safe: everything is already in
             * pending with bi_valid at zero. */
            const uint8_t slot = blk_slot_of(s, split_tok);
            if (s->strm->avail_out >= s->blk_snap_bytes[slot] + 5u) {
                blk_do_split(s, window, split_tok);
                Assert(s->pending == 0, "zfin split undrained");
                split_done = 1;
            }
        }
        if (!split_done)
            region_reoptimize(s, window);
        FLUSH_BLOCK(s, window, 1);
        return finish_done;
    }
    /* Final partial block, mirroring deflate_slow's tail: try a priced split, re-optimize when no split wins, then flush. */
    if (UNLIKELY(s->sym_next)) {
        const uint32_t split_tok = blk_find_split(s);
        if (UNLIKELY(split_tok != 0)) {
            blk_do_split(s, window, split_tok);
            if (UNLIKELY(s->pending != 0))
                return need_more;
        } else {
            region_reoptimize(s, window);
        }
        /* Reset the split bookkeeping before any early return: FLUSH_BLOCK's
         * partial-drain exit would otherwise leave stale tokens that the next
         * entry accumulates on (blk_find_split self-heals, but the first new
         * block loses its split opportunities). */
        FLUSH_BLOCK_ONLY(s, window, 0);
        blk_reset(s);
        if (UNLIKELY(s->strm->avail_out == 0))
            return need_more;
    }

    return block_done;
}
