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

/* Reset the running cost model to fixed-Huffman priors. Called on a fresh
 * stream (or after a level switch) when no statistics exist yet. */
Z_FORCEINLINE static void extreme_model_reset(deflate_state *s) {
    /* Zero the running symbol counts and rebuild counters. */
    for (int i = 0; i < 316; i++)
        s->ext_freq[i] = 0;
    s->ext_since_build = 0;
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

/* Count the code-length symbols a fitted tree needs in the block header:
 * replicates send_tree's run state machine (including its adaptive repeat
 * thresholds) without emitting bits. len[] must be zero past max_code. */
static void count_tree_runs(const uint8_t *len, int max_code, uint32_t *cl_count, uint32_t *run_bits) {
    int prevlen = -1;
    int nextlen = len[0];
    int count = 0;
    int max_count = 7;
    int min_count = 4;

    if (nextlen == 0)
        max_count = 138, min_count = 3;

    for (int n = 0; n <= max_code; n++) {
        const int curlen = nextlen;

        nextlen = n + 1 <= max_code ? len[n + 1] : 0;
        if (++count < max_count && curlen == nextlen)
            continue;
        else if (count < min_count) {
            cl_count[curlen] += (uint32_t)count;
        }
        else if (curlen != 0) {
            if (curlen != prevlen) {
                cl_count[curlen]++;
                count--;
            }
            cl_count[16]++;              /* REP_3_6 */
            *run_bits += 2u;
        }
        else if (count <= 10) {
            cl_count[17]++;              /* REPZ_3_10 */
            *run_bits += 3u;
        }
        else {
            cl_count[18]++;              /* REPZ_11_138 */
            *run_bits += 7u;
        }

        count = 0;
        prevlen = curlen;
        if (nextlen == 0)
            max_count = 138, min_count = 3;
        else if (curlen == nextlen)
            max_count = 6, min_count = 3;
        else
            max_count = 7, min_count = 4;
    }
}

/* Bits to encode lit_freq/dist_freq as one dynamic block: fitted data bits plus the exact
 * tree-description bits (same trimming and run rules the emitter uses). */
static uint32_t split_dyn_bits(const uint16_t *lit_freq, const uint16_t *dist_freq) {
    uint32_t freq_l[L_CODES];
    uint32_t freq_d[D_CODES];
    uint8_t ll_len[L_CODES];
    uint8_t dd_len[D_CODES];
    int max_l = 0;
    int max_d = 0;

    for (int i = 0; i < L_CODES; i++) {
        freq_l[i] = lit_freq[i];
        if (lit_freq[i])
            max_l = i;
    }
    for (int i = 0; i < D_CODES; i++) {
        freq_d[i] = dist_freq[i];
        if (dist_freq[i])
            max_d = i;
    }

    huff_lengths(freq_l, L_CODES, ll_len);
    huff_lengths(freq_d, D_CODES, dd_len);

    uint32_t bits = 0;
    for (int i = 0; i <= max_l; i++)
        bits += freq_l[i] * (uint32_t)ll_len[i];
    for (int i = 0; i <= max_d; i++)
        bits += freq_d[i] * (uint32_t)dd_len[i];

    /* Tree description: header fields, code-length symbol counts over both
     * trimmed trees, then the fitted meta-tree. */
    uint32_t cl_count[BL_CODES] = { 0 };
    uint32_t run_bits = 0;
    count_tree_runs(ll_len, max_l, cl_count, &run_bits);
    count_tree_runs(dd_len, max_d, cl_count, &run_bits);

    int max_blindex = -1;
    for (int rank = 0; rank < BL_CODES; rank++)
        if (cl_count[bl_order[rank]])
            max_blindex = rank;

    uint8_t bl_len[BL_CODES];
    huff_lengths(cl_count, BL_CODES, bl_len);

    bits += 5u + 5u + 4u + (uint32_t)(max_blindex + 1) * 3u + run_bits;
    for (int rank = 0; rank <= max_blindex; rank++)
        bits += cl_count[bl_order[rank]] * (uint32_t)bl_len[bl_order[rank]];

    return bits;
}

/* Bit cost of encoding the given per-symbol frequencies as one fixed-Huffman block, header included.
 * The fixed lengths are RFC1951's: literals 0-143 at 8 bits, 144-255 at 9, length codes 7/8, distances 5. */
static uint32_t split_fixed_bits(const uint16_t *lit_freq, const uint16_t *dist_freq) {
    uint32_t bits = 3u;   /* bfinal + btype */

    /* Literal symbols: RFC fixed lengths, 8 bits below 144, 9 above. */
    for (int i = 0; i < 144; i++)
        bits += (uint32_t)lit_freq[i] * 8u;
    for (int i = 144; i < LITERALS; i++)
        bits += (uint32_t)lit_freq[i] * 9u;
    /* Length codes: 7 or 8 bits each. */
    for (int i = LITERALS; i < L_CODES; i++)
        bits += (uint32_t)lit_freq[i] * (i <= LITERALS + 23 ? 7u : 8u);
    /* Distance codes: 5-bit fixed symbol. */
    for (int i = 0; i < D_CODES; i++)
        bits += (uint32_t)dist_freq[i] * 5u;

    return bits;
}

/* Cheapest single-block encoding of this half: dynamic, fixed or stored,
 * mirroring zng_tr_flush_block's selection (stored is bytes*8 + 4 header
 * bytes, as that function compares it). */
static uint32_t split_half_bits(const uint16_t *lit_freq, const uint16_t *dist_freq, uint32_t bytes) {
    uint32_t bits = split_dyn_bits(lit_freq, dist_freq);

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

    uint32_t best_bits = split_half_bits(tot_lit, tot_dist, s->blk_bytes);
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

        const uint32_t bits = split_half_bits(s->blk_snap_l[slot], s->blk_snap_d[slot], s->blk_snap_bytes[slot])
                             + split_half_bits(right_lit, right_dist, s->blk_bytes - s->blk_snap_bytes[slot]);
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
         * the per-block trees trees.c actually builds. */
        if (UNLIKELY(++s->ext_since_build >= EXTREME_REBUILD_EVERY)) {
            s->ext_since_build = 0;
            /* Cost of the most frequent literal: bounds the tail credit (see
             * the decision loop above) and gates the count decay below. */
            uint32_t best_freq = 0;
            for (int i = 0; i < LITERALS; i++) {
                if (s->ext_freq[i] > best_freq) {
                    best_freq = s->ext_freq[i];
                    s->ext_mode_lit_cost = s->ext_ll_len[i];
                }
            }

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
            const uint32_t split_tok = blk_find_split(s);
            if (UNLIKELY(split_tok != 0)) {
                blk_do_split(s, window, split_tok);
                /* The left half may not have fully drained: pending_out has
                 * advanced and compressing another block now would write over
                 * undrained output. Defer; the next call drains pending at
                 * entry before resuming compression. */
                if (UNLIKELY(s->pending != 0))
                    return need_more;
            }
            FLUSH_BLOCK(s, window, 0);
            blk_reset(s);
        }
    }

    /* Positions still to insert into the hash table when input resumes. */
    s->insert = s->strstart < (STD_MIN_MATCH - 1) ? s->strstart : (STD_MIN_MATCH - 1);
    if (UNLIKELY(flush == Z_FINISH)) {
        const uint32_t split_tok = blk_find_split(s);
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
            /* The token count came from a snapshot; find its slot. */
            uint8_t slot = 0;
            while (s->blk_snap_tok[slot] != split_tok)
                slot++;
            if (s->strm->avail_out >= s->blk_snap_bytes[slot] + 5u) {
                blk_do_split(s, window, split_tok);
                Assert(s->pending == 0, "zfin split undrained");
            }
        }
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
