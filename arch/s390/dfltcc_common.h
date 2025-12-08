#ifndef DFLTCC_COMMON_H
#define DFLTCC_COMMON_H

#include "zutil.h"

/*
   Runtime Tuning parameters.
*/

/*
  control usage of DFLTCC
  
  env variable: DFLTCC
  0 or unset: use DFLTCC algorithm (default)
  1: disable DFLTCC
 */
extern int env_dfltcc_disabled;

/*
  control usage of DFLTCC_CMPR 
  set this to 1 if you need reproducible builds 

  env variable: SOURCE_DATE_EPOCH
  0 or unset: use DFLTCC_CMPR
  1: disable DFLTCC_CMPR
 */
extern int env_dfltcc_source_date_epoch;

/*
  disable DFLTCC for specific compression levels

  env variable: DFLTCC_LEVEL_MASK
  valid range: 0-10 / 0x0-0xA
  default: 0x2 - disable for compresion level 0 to 2
 */
extern unsigned long env_dfltcc_level_mask;

/*
  New block each X bytes

  env variable: DFLTCC_BLOCK_SIZE
  valid range: > 262144 / 0x40000 (256K)
  default: 1048576 / 0x100000 (1M)
 */
extern unsigned long env_dfltcc_block_size;

/*
  New block after total_in > X

  env variable: DFLTCC_FIRST_FHT_BLOCK_SIZE
  default: 4096 / 0x1000
 */
extern unsigned long env_dfltcc_block_threshold;

/*
  New block only if avail_in >= X

  env variable: DFLTCC_DHT_MIN_SAMPLE_SIZE
  default: 4096 / 0x1000
*/
extern unsigned long env_dfltcc_dht_threshold;

/*
  default value for DFLTCC_RIBM register

  env variable: DFLTCC_RIBM
  default: 0
  valid range: 0-255 / 0x0-0xFF
*/
extern unsigned long env_dfltcc_ribm;

/*
   Parameter Block for Query Available Functions.
 */
struct dfltcc_qaf_param {
    char fns[16];
    char reserved1[8];
    char fmts[2];
    char reserved2[6];
} ALIGNED_(8);

/*
   Parameter Block for Generate Dynamic-Huffman Table, Compress and Expand.
 */
struct dfltcc_param_v0 {
    uint16_t pbvn;                     /* Parameter-Block-Version Number */
    uint8_t mvn;                       /* Model-Version Number */
    uint8_t ribm;                      /* Reserved for IBM use */
    uint32_t reserved32 : 31;
    uint32_t cf : 1;                   /* Continuation Flag */
    uint8_t reserved64[8];
    uint32_t nt : 1;                   /* New Task */
    uint32_t reserved129 : 1;
    uint32_t cvt : 1;                  /* Check Value Type */
    uint32_t reserved131 : 1;
    uint32_t htt : 1;                  /* Huffman-Table Type */
    uint32_t bcf : 1;                  /* Block-Continuation Flag */
    uint32_t bcc : 1;                  /* Block Closing Control */
    uint32_t bhf : 1;                  /* Block Header Final */
    uint32_t reserved136 : 1;
    uint32_t reserved137 : 1;
    uint32_t dhtgc : 1;                /* DHT Generation Control */
    uint32_t reserved139 : 5;
    uint32_t reserved144 : 5;
    uint32_t sbb : 3;                  /* Sub-Byte Boundary */
    uint8_t oesc;                      /* Operation-Ending-Supplemental Code */
    uint32_t reserved160 : 12;
    uint32_t ifs : 4;                  /* Incomplete-Function Status */
    uint16_t ifl;                      /* Incomplete-Function Length */
    uint8_t reserved192[8];
    uint8_t reserved256[8];
    uint8_t reserved320[4];
    uint16_t hl;                       /* History Length */
    uint32_t reserved368 : 1;
    uint16_t ho : 15;                  /* History Offset */
    uint32_t cv;                       /* Check Value */
    uint32_t eobs : 15;                /* End-of-block Symbol */
    uint32_t reserved431: 1;
    uint8_t eobl : 4;                  /* End-of-block Length */
    uint32_t reserved436 : 12;
    uint32_t reserved448 : 4;
    uint16_t cdhtl : 12;               /* Compressed-Dynamic-Huffman Table
                                          Length */
    uint8_t reserved464[6];
    uint8_t cdht[288];                 /* Compressed-Dynamic-Huffman Table */
    uint8_t reserved[24];
    uint8_t ribm2[8];                  /* Reserved for IBM use */
    uint8_t csb[1152];                 /* Continuation-State Buffer */
} ALIGNED_(8);

/*
   Extension of inflate_state and deflate_state.
 */
struct dfltcc_state {
    struct dfltcc_param_v0 param;      /* Parameter block. */
    struct dfltcc_qaf_param af;        /* Available functions. */
    char msg[64];                      /* Buffer for strm->msg */
};

typedef struct {
    struct dfltcc_state common;
    uint16_t level_mask;               /* Levels on which to use DFLTCC */
    uint32_t block_size;               /* New block each X bytes */
    size_t block_threshold;            /* New block after total_in > X */
    uint32_t dht_threshold;            /* New block only if avail_in >= X */
} arch_deflate_state;

typedef struct {
    struct dfltcc_state common;
} arch_inflate_state;

/*
   History buffer size.
 */
#define HB_BITS 15
#define HB_SIZE (1 << HB_BITS)

/*
   Sizes of deflate block parts.
 */
#define DFLTCC_BLOCK_HEADER_BITS 3
#define DFLTCC_HLITS_COUNT_BITS 5
#define DFLTCC_HDISTS_COUNT_BITS 5
#define DFLTCC_HCLENS_COUNT_BITS 4
#define DFLTCC_MAX_HCLENS 19
#define DFLTCC_HCLEN_BITS 3
#define DFLTCC_MAX_HLITS 286
#define DFLTCC_MAX_HDISTS 30
#define DFLTCC_MAX_HLIT_HDIST_BITS 7
#define DFLTCC_MAX_SYMBOL_BITS 16
#define DFLTCC_MAX_EOBS_BITS 15
#define DFLTCC_MAX_PADDING_BITS 7

#define DEFLATE_BOUND_COMPLEN(source_len) \
    ((DFLTCC_BLOCK_HEADER_BITS + \
      DFLTCC_HLITS_COUNT_BITS + \
      DFLTCC_HDISTS_COUNT_BITS + \
      DFLTCC_HCLENS_COUNT_BITS + \
      DFLTCC_MAX_HCLENS * DFLTCC_HCLEN_BITS + \
      (DFLTCC_MAX_HLITS + DFLTCC_MAX_HDISTS) * DFLTCC_MAX_HLIT_HDIST_BITS + \
      (source_len) * DFLTCC_MAX_SYMBOL_BITS + \
      DFLTCC_MAX_EOBS_BITS + \
      DFLTCC_MAX_PADDING_BITS) >> 3)

#endif
