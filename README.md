## zlib-ng
*zlib data compression library for the next generation systems*

Maintained by Hans Kristian Rosbach
          aka Dead2 (zlib-ng àt circlestorm dót org)

|CI|Build Status|
|:-|-|
|Travis|[![build status](https://api.travis-ci.org/zlib-ng/zlib-ng.svg)](https://travis-ci.org/zlib-ng/zlib-ng/)|
|Buildkite|[![Build status](https://badge.buildkite.com/7bb1ef84356d3baee26202706cc053ee1de871c0c712b65d26.svg?branch=develop)](https://buildkite.com/circlestorm-productions/zlib-ng)|


Fork Motivation
---------------------------

The motivation for this fork was due to seeing several 3rd party
contributions containing new optimizations not getting implemented
into the official zlib repository.

Mark Adler has been maintaining zlib for a very long time, and he has
done a great job and hopefully he will continue for a long time yet.
The idea of zlib-ng is not to replace zlib, but to co-exist as a
drop-in replacement with a lower threshold for code change.

zlib has a long history and is incredibly portable, even supporting
lots of systems that predate the Internet. This is great, but it does
complicate further development and maintainability.
The zlib code has numerous workarounds for old compilers that do not
understand ANSI-C or to accommodate systems with limitations such as
operating in a 16-bit environment.

Many of these workarounds are only maintenance burdens, some of them
are pretty huge code-wise. For example, the [v]s[n]printf workaround
code has a whopping 8 different implementations just to cater to
various old compilers. With this many workarounds cluttered throughout
the code, new programmers with an idea/interest for zlib will need
to take some time to figure out why all of these seemingly strange
things are used, and how to work within those confines.

So I decided to make a fork, merge all the Intel optimizations, merge
the Cloudflare optimizations that did not conflict, plus a couple
of other smaller patches. Then I started cleaning out workarounds,
various dead code, all contrib and example code as there is little
point in having those in this fork for various reasons.

A lot of improvements have gone into zlib-ng since its start, and
numerous people have contributed both small and big improvements,
or valuable testing.

Please read LICENSE.md, it is very simple and very liberal.

Build
-----

There are two ways to build zlib-ng:

### Cmake

To build zlib-ng using the cross-platform makefile generator cmake.

```
cmake .
cmake --build . --config Release
```

### Configure

To build zlib-ng using the bash configure script:

```
./configure
make
make test
```

Build Options
-------------

| CMake                    | configure                | Description                                                                                  | Default                          |
|:-------------------------|:-------------------------|:---------------------------------------------------------------------------------------------|----------------------------------|
| ZLIB_COMPAT              | --zlib-compat            | Compile with zlib compatible API                                                             | OFF                              |
| ZLIB_ENABLE_TESTS        |                          | Build test binaries                                                                          | ON                               |
| WITH_GZFILEOP            | --with-gzfileops         | Compile with support for gzFile related functions                                            | OFF                              |
| WITH_MSAN                | --with-msan              | Build with memory sanitizer                                                                  | OFF                              |
| WITH_OPTIM               | --without-optimizations  | Build with optimisations                                                                     | ON                               |
| WITH_NEW_STRATEGIES      | --without-new-strategies | Use new strategies                                                                           | ON                               |
| WITH_NATIVE_INSTRUCTIONS |                          | Instruct the compiler to use the full instruction set on this host (gcc/clang -march=native) | OFF                              |
|                          | --force-sse2             | Assume SSE2 instructions are always available                                                | DISABLED (x86), ENABLED (x86_64) |
| WITH_ACLE                | --without-acle           | Build with ACLE CRC                                                                          | ON                               |
| WITH_NEON                | --without-neon           | Build with NEON intrinsics                                                                   | ON                               |
| WITH_DFLTCC_DEFLATE      | --with-dfltcc-deflate    | Use DEFLATE COMPRESSION CALL instruction for compression on IBM Z                            | OFF                              |
| WITH_DFLTCC_INFLATE      | --with-dfltcc-inflate    | Use DEFLATE COMPRESSION CALL instruction for decompression on IBM Z                          | OFF                              |
| WITH_SANITIZERS          | --with-sanitizers        | Build with address sanitizer and all supported sanitizers other than memory sanitizer        | OFF                              |
| WITH_FUZZERS             | --with-fuzzers           | Build test/fuzz                                                                              | OFF                              |

Install
-------

We do not recommend installing unless you know what you are doing, as this can
override the system default zlib library, and any wrong configuration or
incompatibility of zlib-ng can make the whole system unusable.

On linux distros, an alternative way to use zlib-ng instead of zlib, is through
the use of the _LD_PRELOAD_ environment variable. If the program is dynamically linked
with zlib, then zlib-ng can take its place without risking system-wide instability.

```
LD_PRELOAD=/opt/zlib-ng/libz.so.1.2.11.zlib-ng /usr/bin/program
```

### Cmake

To install zlib-ng system-wide using cmake:

```
cmake --build . --target install
```

### Configure

To install zlib-ng system-wide using the configure script:

```
make install
```

Contributing
------------

Zlib-ng is a young project, and we aim to be open to contributions,
and we would be delighted to receive pull requests on github.
Just remember that any code you submit must be your own and it must
be zlib licensed.
Help with testing and reviewing of pull requests etc is also very
much appreciated.

If you are interested in contributing, please consider joining our
IRC channel #zlib-ng on the Freenode IRC network.


Acknowledgments
----------------

Thanks to Servebolt.com for sponsoring my maintainership of zlib-ng.

Thanks go out to all the people and companies who have taken the time
to contribute code reviews, testing and/or patches. Zlib-ng would not
have been nearly as good without you.

The deflate format used by zlib was defined by Phil Katz.
The deflate and zlib specifications were written by L. Peter Deutsch.

zlib was originally created by Jean-loup Gailly (compression)
and Mark Adler (decompression).

Contents
--------

| Name             | Description                                                    |
|:-----------------|:---------------------------------------------------------------|
| arch/            | Architecture-specific code                                     |
| doc/             | Documentation for formats and algorithms                       |
| test/example.c   | Zlib usages examples for build testing                         |
| test/minigzip.c  | Minimal gzip-like functionality for build testing              |
| test/infcover.c  | inf*.c code coverage for build coverage testing                |
| win32/           | Shared library version resources for Windows                   |
| CMakeLists.txt   | Cmake build script                                             |
| configure        | Bash configure/build script                                    |
| adler32.c        | Compute the Adler-32 checksum of a data stream                 |
| compress.c       | Compress a memory buffer                                       |
| deflate.*        | Compress data using the deflation algorithm                    |
| deflate_fast.c   | Compress data using the fast strategy of deflate algorithm     |
| deflate_medium.c | Compress data using the medium stragety of deflate algorithm   |
| deflate_slow.c   | Compress data using the slow strategy of deflate algorithm     |
| functable.*      | Struct containing function pointers to optimized functions     |
| gzclose.c        | Zlib function for closing gzip files                           |
| gzendian.h       | BYTE_ORDER for endian tests                                    |
| gzguts.h         | Zlib header internal definitions for gz* operations            |
| gzlib.c          | Zlib functions common to reading and writing gzip files        |
| gzread.c         | Zlib functions for reading gzip files                          |
| gzwrite.c        | Zlib functions for writing gzip files                          |
| infback.*        | Inflate using a callback interface                             |
| inflate.*        | Zlib decompression                                             |
| inffast.*        | Fast decoding                                                  |
| inffixed.h       | Table for decoding fixed codes                                 |
| inftrees.h       | Generate Huffman trees for efficient decoding                  |
| memcopy.h        | Inline functions to copy small data chunks                     |
| trees.*          | Output deflated data using Huffman coding                      |
| uncompr.c        | Decompress a memory buffer                                     |
| zconf.h.cmakein  | zconf.h template for cmake                                     |
| zlib.3           | Man page for zlib                                              |
| zlib.3.pdf       | Man page in PDF format                                         |
| zlib.map         | Linux symbol information                                       |
| zlib.pc.in       | zlib.pc template for cmake                                     |
