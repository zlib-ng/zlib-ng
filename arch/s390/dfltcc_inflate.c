/* dfltcc_inflate.c - IBM Z DEFLATE CONVERSION CALL decompression support. */

/*
   Use the following commands to build zlib-ng with DFLTCC decompression support:

        $ ./configure --with-dfltcc-inflate
   or

        $ cmake -DWITH_DFLTCC_INFLATE=1 .

   and then

        $ make
*/

#include "zbuild.h"
#include "zutil.h"
#include "inftrees.h"
#include "inflate.h"
#include "dfltcc_inflate.h"
#include "dfltcc_detail.h"

int Z_INTERNAL dfltcc_can_inflate(PREFIX3(streamp) strm) {
    struct inflate_state *state = (struct inflate_state *)strm->state;
    struct dfltcc_state *dfltcc_state = GET_DFLTCC_STATE(state);

    /* Unsupported compression settings */
    if (state->wbits != HB_BITS)
        return 0;

    /* Unsupported hardware */
    return is_bit_set(dfltcc_state->af.fns, DFLTCC_XPND) && is_bit_set(dfltcc_state->af.fmts, DFLTCC_FMT0);
}

static inline dfltcc_cc dfltcc_xpnd(PREFIX3(streamp) strm) {
    struct inflate_state *state = (struct inflate_state *)strm->state;
    struct dfltcc_param_v0 *param = &GET_DFLTCC_STATE(state)->param;
    size_t avail_in = strm->avail_in;
    size_t avail_out = strm->avail_out;
    dfltcc_cc cc;

    cc = dfltcc(DFLTCC_XPND | HBT_CIRCULAR,
                param, &strm->next_out, &avail_out,
                &strm->next_in, &avail_in, state->window);
    strm->avail_in = avail_in;
    strm->avail_out = avail_out;
    return cc;
}

dfltcc_inflate_action Z_INTERNAL dfltcc_inflate(PREFIX3(streamp) strm, int flush, int *ret) {
    struct inflate_state *state = (struct inflate_state *)strm->state;
    struct dfltcc_state *dfltcc_state = GET_DFLTCC_STATE(state);
    struct dfltcc_param_v0 *param = &dfltcc_state->param;
    dfltcc_cc cc;

    if (flush == Z_BLOCK || flush == Z_TREES) {
        /* DFLTCC does not support stopping on block boundaries */
        if (dfltcc_inflate_disable(strm)) {
            *ret = Z_STREAM_ERROR;
            return DFLTCC_INFLATE_BREAK;
        } else
            return DFLTCC_INFLATE_SOFTWARE;
    }

    if (state->last) {
        if (state->bits != 0) {
            strm->next_in++;
            strm->avail_in--;
            state->bits = 0;
        }
        state->mode = CHECK;
        return DFLTCC_INFLATE_CONTINUE;
    }

    if (strm->avail_in == 0 && !param->cf)
        return DFLTCC_INFLATE_BREAK;

    if (inflate_ensure_window(state)) {
        state->mode = MEM;
        return DFLTCC_INFLATE_CONTINUE;
    }

    /* Translate stream to parameter block */
    param->cvt = state->flags ? CVT_CRC32 : CVT_ADLER32;
    param->sbb = state->bits;
    param->hl = state->whave; /* Software and hardware history formats match */
    param->ho = (state->wnext - state->whave) & ((1 << HB_BITS) - 1);
    if (param->hl)
        param->nt = 0; /* Honor history for the first block */
    param->cv = state->flags ? ZSWAP32(state->check) : state->check;

    /* Inflate */
    do {
        cc = dfltcc_xpnd(strm);
    } while (cc == DFLTCC_CC_AGAIN);

    /* Translate parameter block to stream */
    strm->msg = oesc_msg(dfltcc_state->msg, param->oesc);
    state->last = cc == DFLTCC_CC_OK;
    state->bits = param->sbb;
    state->whave = param->hl;
    state->wnext = (param->ho + param->hl) & ((1 << HB_BITS) - 1);
    state->check = state->flags ? ZSWAP32(param->cv) : param->cv;
    if (cc == DFLTCC_CC_OP2_CORRUPT && param->oesc != 0) {
        /* Report an error if stream is corrupted */
        state->mode = BAD;
        return DFLTCC_INFLATE_CONTINUE;
    }
    state->mode = TYPEDO;
    /* Break if operands are exhausted, otherwise continue looping */
    return (cc == DFLTCC_CC_OP1_TOO_SHORT || cc == DFLTCC_CC_OP2_TOO_SHORT) ?
        DFLTCC_INFLATE_BREAK : DFLTCC_INFLATE_CONTINUE;
}

int Z_INTERNAL dfltcc_was_inflate_used(PREFIX3(streamp) strm) {
    struct inflate_state *state = (struct inflate_state *)strm->state;
    struct dfltcc_param_v0 *param = &GET_DFLTCC_STATE(state)->param;

    return !param->nt;
}

int Z_INTERNAL dfltcc_inflate_disable(PREFIX3(streamp) strm) {
    struct inflate_state *state = (struct inflate_state *)strm->state;
    struct dfltcc_state *dfltcc_state = GET_DFLTCC_STATE(state);

    if (!dfltcc_can_inflate(strm))
        return 0;
    if (dfltcc_was_inflate_used(strm))
        /* DFLTCC has already decompressed some data. Since there is not
         * enough information to resume decompression in software, the call
         * must fail.
         */
        return 1;
    /* DFLTCC was not used yet - decompress in software */
    memset(&dfltcc_state->af, 0, sizeof(dfltcc_state->af));
    return 0;
}
