# zlib-ng Utilities

This directory contains utility programs and development tools for zlib-ng. These utilities help with compression testing and generating of internal tables.

`minigzip` is in addition to being used for testing also popularly used as a simpler but faster alternative to `gzip`.

## Files

- `minigzip.c`: A lightweight implementation of the standard gzip compression and decompression tool.
- `minideflate.c`: A basic demonstration program for testing raw deflate compression and decompression streams.
- `makefixed.c`: Generates the static Huffman codes table used for fixed deflate blocks (inffixed_tbl.h).
- `maketrees.c`: Generates the static tree tables and compile-time structures used for Huffman tree construction (trees_tbl.h).
- `makecrct.c`: Generates the precomputed CRC-32 and CRC-32-Fold lookup tables used to accelerate checksum calculations (crc32_braid_tbl.h).
