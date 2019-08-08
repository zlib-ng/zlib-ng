This directory contains IBM Z DEFLATE CONVERSION CALL support for
zlib-ng. In order to enable it, the following build commands should be
used:

    $ ./configure --with-dfltcc-deflate --with-dfltcc-inflate
    $ make

or

    $ cmake -DWITH_DFLTCC_DEFLATE=1 -DWITH_DFLTCC_INFLATE=1 .
    $ make

When built like this, zlib-ng would compress in hardware on level 1,
and in software on all other levels. Decompression will always happen
in hardware. In order to enable DFLTCC compression for levels 1-6 (i.e.
to make it used by default) one could add -DDFLTCC_LEVEL_MASK=0x7e to
CFLAGS when building zlib-ng.

Two DFLTCC compression calls produce the same results only when they
both are made on machines of the same generation, and when the
respective buffers have the same offset relative to the start of the
page. Therefore care should be taken when using hardware compression
when reproducible results are desired. In particular, zlib-ng-specific
zng_deflateSetParams call allows setting Z_DEFLATE_REPRODUCIBLE
parameter, which would disable DFLTCC if reproducible results are
required.

DFLTCC does not support every single zlib-ng feature, in particular:

* inflate(Z_BLOCK) and inflate(Z_TREES)
* inflateMark()
* inflatePrime()
* deflateParams() after the first deflate() call

When used, these functions will either switch to software, or, in case
this is not possible, gracefully fail.

All SystemZ-specific code lives in a separate file and is integrated
with the rest of zlib-ng using hook macros, which are explained below.

DFLTCC takes as arguments a parameter block, an input buffer, an output
buffer and a window. ZALLOC_STATE, ZFREE_STATE, ZCOPY_STATE,
ZALLOC_WINDOW and TRY_FREE_WINDOW macros encapsulate allocation details
for the parameter block (which is allocated alongside zlib-ng state)
and the window (which must be page-aligned).

While for inflate software and hardware window formats match, this is
not the case for deflate. Therefore, deflateSetDictionary and
deflateGetDictionary need special handling, which is triggered using
the DEFLATE_SET_DICTIONARY_HOOK and DEFLATE_GET_DICTIONARY_HOOK macros.

deflateResetKeep() and inflateResetKeep() update the DFLTCC parameter
block using DEFLATE_RESET_KEEP_HOOK and INFLATE_RESET_KEEP_HOOK macros.

DEFLATE_PARAMS_HOOK, INFLATE_PRIME_HOOK and INFLATE_MARK_HOOK macros
make the unsupported deflateParams(), inflatePrime() and inflateMark()
calls fail gracefully.

The algorithm implemented in hardware has different compression ratio
than the one implemented in software. DEFLATE_BOUND_ADJUST_COMPLEN and
DEFLATE_NEED_CONSERVATIVE_BOUND macros make deflateBound() return the
correct results for the hardware implementation.

Actual compression and decompression are handled by DEFLATE_HOOK and
INFLATE_TYPEDO_HOOK macros. Since inflation with DFLTCC manages the
window on its own, calling updatewindow() is suppressed using
INFLATE_NEED_UPDATEWINDOW() macro.

In addition to compression, DFLTCC computes CRC-32 and Adler-32
checksums, therefore, whenever it's used, software checksumming is
suppressed using DEFLATE_NEED_CHECKSUM and INFLATE_NEED_CHECKSUM
macros.

While software always produces reproducible compression results, this
is not the case for DFLTCC. Therefore, zlib-ng users are given the
ability to specify whether or not reproducible compression results
are required. While it is always possible to specify this setting
before the compression begins, it is not always possible to do so in
the middle of a deflate stream - the exact conditions for that are
determined by DEFLATE_CAN_SET_REPRODUCIBLE macro.
