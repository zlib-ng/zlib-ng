/* dfltcc_inflate.c - IBM Z DEFLATE CONVERSION CALL decompression support. */

/*
   Use the following commands to build zlib-ng with DFLTCC decompression support:

        $ ./configure --with-dfltcc-inflate
   or

        $ cmake -DWITH_DFLTCC_INFLATE=1 .

   and then

        $ make
*/

#ifdef S390_DFLTCC_INFLATE

#include "zbuild.h"
#include "zutil.h"
#include "inftrees.h"
#include "inflate.h"
#include "dfltcc_inflate.h"
#include "dfltcc_detail.h"

void Z_INTERNAL PREFIX(dfltcc_reset_inflate_state)(PREFIX3(streamp) strm) {
    struct inflate_state *state = (struct inflate_state *)strm->state;

    dfltcc_reset_state(&state->arch.common);
}

int Z_INTERNAL PREFIX(dfltcc_can_inflate)(PREFIX3(streamp) strm) {
    struct inflate_state *state = (struct inflate_state *)strm->state;
    struct dfltcc_state *dfltcc_state = &state->arch.common;

    if (!PREFIX(dfltcc_enabled))
        return 0;

    /* Unsupported hardware */
    return is_bit_set(dfltcc_state->af.fns, DFLTCC_XPND) && is_bit_set(dfltcc_state->af.fmts, DFLTCC_FMT0);
}

static inline dfltcc_cc dfltcc_xpnd(PREFIX3(streamp) strm) {
    struct inflate_state *state = (struct inflate_state *)strm->state;
    struct dfltcc_param_v0 *param = &state->arch.common.param;
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

/* Translate an Operation-Ending-Supplemental Code (OESC) reported by the
   hardware into the message that software inflate() produces for the same
   defect, so that callers see a single set of strings no matter which decoder
   ran. The codes are defined in chapter 26 "DEFLATE CONVERSION CALL" of the
   z/Architecture Principles of Operation (SA22-7832).

   Only codes whose software equivalent is unambiguous are translated; the
   bookkeeping codes (parameter block version, history length, compressed DHT
   cache length), the codes that describe a defect without naming which tree it
   belongs to, and the model-dependent FF code keep the generic text so that
   nothing the hardware reported is lost. Support for codes other than 00 and FF
   is model-dependent, in which case the generic text is what callers get. */
static z_const char *dfltcc_inflate_oesc_msg(char *buf, int oesc) {
    switch (oesc) {
        case 0x11: /* BTYPE equal to 11 binary */
            return "invalid block type";
        case 0x12: /* stored block whose NLEN is not the complement of LEN */
            return "invalid stored block lengths";
        case 0x22: /* HLIT greater than 29 */
        case 0x23: /* HDIST greater than 29 */
            return "too many length or distance symbols";
        case 0x24: /* code-length tree cannot describe a functional Huffman tree */
            return "invalid code lengths set";
        case 0x26: /* repeat used before any code length was defined */
        case 0x2A: /* more code lengths than the DHT declares */
            return "invalid bit length repeat";
        case 0x28: /* no Huffman code assigned to the end-of-block symbol */
            return "invalid code -- missing end-of-block";
        case 0x2B: /* literal/length tree cannot describe a functional tree */
            return "invalid literal/lengths set";
        case 0x2D: /* distance tree cannot describe a functional tree */
            return "invalid distances set";
        case 0x31: /* no code for a literal or length, or reserved fixed length code */
            return "invalid literal/length code";
        case 0x32: /* no code for a distance, or reserved fixed distance code */
            return "invalid distance code";
        case 0x40: /* distance beyond the history available at that point */
            return "invalid distance too far back";
        default:
            return oesc_msg(buf, oesc);
    }
}

dfltcc_inflate_action Z_INTERNAL PREFIX(dfltcc_inflate)(PREFIX3(streamp) strm, int flush, int *ret) {
    struct inflate_state *state = (struct inflate_state *)strm->state;
    struct dfltcc_state *dfltcc_state = &state->arch.common;
    struct dfltcc_param_v0 *param = &dfltcc_state->param;
    dfltcc_cc cc;

    if (flush == Z_BLOCK || flush == Z_TREES) {
        /* DFLTCC does not support stopping on block boundaries */
        if (PREFIX(dfltcc_inflate_disable)(strm)) {
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

    /* if window not in use yet, initialize */
    if (state->wsize == 0)
        state->wsize = 1U << state->wbits;

    /* Translate stream to parameter block */
    param->cvt = ((state->wrap & 4) && state->flags) ? CVT_CRC32 : CVT_ADLER32;
    param->sbb = state->bits;
    if (param->hl)
        param->nt = 0; /* Honor history for the first block */
    if (state->wrap & 4)
        param->cv = state->flags ? ZSWAP32(state->check) : state->check;

    /* Inflate */
    do {
        cc = dfltcc_xpnd(strm);
    } while (cc == DFLTCC_CC_AGAIN);

    /* Translate parameter block to stream. Only report a message when the
       hardware actually flagged an invalid input: assigning unconditionally
       would erase a message set earlier during the same inflate() call, and
       software inflate() likewise leaves strm->msg alone unless it errors. */
    if (param->oesc != 0)
        strm->msg = dfltcc_inflate_oesc_msg(dfltcc_state->msg, param->oesc);
    state->last = cc == DFLTCC_CC_OK;
    state->bits = param->sbb;
    if (state->wrap & 4)
        strm->adler = state->check = state->flags ? ZSWAP32(param->cv) : param->cv;
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

int Z_INTERNAL PREFIX(dfltcc_was_inflate_used)(PREFIX3(streamp) strm) {
    struct inflate_state *state = (struct inflate_state *)strm->state;

    return !state->arch.common.param.nt;
}

/*
   Rotates a circular buffer.
   The implementation is based on https://cplusplus.com/reference/algorithm/rotate/
 */
static void rotate(unsigned char *start, unsigned char *pivot, unsigned char *end) {
    unsigned char *p = pivot;
    unsigned char tmp;

    while (p != start) {
        tmp = *start;
        *start = *p;
        *p = tmp;

        start++;
        p++;

        if (p == end)
            p = pivot;
        else if (start == pivot)
            pivot = p;
    }
}

int Z_INTERNAL PREFIX(dfltcc_inflate_disable)(PREFIX3(streamp) strm) {
    struct inflate_state *state = (struct inflate_state *)strm->state;
    struct dfltcc_state *dfltcc_state = &state->arch.common;
    struct dfltcc_param_v0 *param = &dfltcc_state->param;

    if (!PREFIX(dfltcc_can_inflate)(strm))
        return 0;
    if (PREFIX(dfltcc_was_inflate_used)(strm))
        /* DFLTCC has already decompressed some data. Since there is not
         * enough information to resume decompression in software, the call
         * must fail.
         */
        return 1;
    /* DFLTCC was not used yet - decompress in software */
    memset(&dfltcc_state->af, 0, sizeof(dfltcc_state->af));
    /* Convert the window from the hardware to the software format */
    rotate(state->window, state->window + param->ho, state->window + HB_SIZE);
    state->whave = state->wnext = MIN(param->hl, state->wsize);
    return 0;
}

/*
   Preloading history.
*/
int Z_INTERNAL PREFIX(dfltcc_inflate_set_dictionary)(PREFIX3(streamp) strm,
                                                     const unsigned char *dictionary, uInt dict_length) {
    struct inflate_state *state = (struct inflate_state *)strm->state;
    struct dfltcc_param_v0 *param = &state->arch.common.param;

    /* if window not in use yet, initialize */
    if (state->wsize == 0)
        state->wsize = 1U << state->wbits;

    append_history(param, state->window, dictionary, dict_length);
    state->havedict = 1;
    return Z_OK;
}

int Z_INTERNAL PREFIX(dfltcc_inflate_get_dictionary)(PREFIX3(streamp) strm,
                                                     unsigned char *dictionary, uInt *dict_length) {
    struct inflate_state *state = (struct inflate_state *)strm->state;
    struct dfltcc_param_v0 *param = &state->arch.common.param;

    if (dictionary && state->window)
        get_history(param, state->window, dictionary);
    if (dict_length)
        *dict_length = param->hl;
    return Z_OK;
}

#endif
