zlib-ng - zlib for the next generation systems

Maintained by Hans Kristian Rosbach
          aka Dead2 (zlib-ng àt circlestorm dót org)


Fork Motivation and History
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
=====

To build zlib-ng for your platform, use the cross-platform makefile generator cmake.

```
cmake . 
cmake --build . --config Release
```

Build Options
-------------

|Name|Description|Default Value|
|:-|:-|-|
|WITH_GZFILEOP|Compile with support for gzFile related functions|OFF|
|ZLIB_COMPAT|Compile with zlib compatible API|OFF|
|WITH_MSAN|Build with memory sanitizer|OFF|
|WITH_OPTIM|Build with optimisations|ON|
|WITH_NEW_STRATEGIES|Use new strategies|ON|
|WITH_NATIVE_INSTRUCTIONS|Instruct the compiler to use the full instruction set on this host (gcc/clang -march=native)|OFF|
|WITH_ACLE|Build with ACLE CRC|ON|
|WITH_NEON|Build with NEON intrinsics|ON|
|WITH_DFLTCC_DEFLATE|Use DEFLATE COMPRESSION CALL instruction for compression on IBM Z|OFF|
|WITH_DFLTCC_INFLATE|Use DEFLATE COMPRESSION CALL instruction for decompression on IBM Z|OFF|
|ZLIB_ENABLE_TESTS|Build test binaries|ON|
|WITH_SANITIZERS|Build with address sanitizer and all supported sanitizers other than memory sanitizer|OFF|
|WITH_FUZZERS|Build test/fuzz|OFF|
|WITH_CVES|Build test/CVEs|OFF|

Install
-------

To install the binaries for distribution, use the cmake install command:

```
cmake --build . --target install
```

On linux distros, an alternative way to use zlib-ng instead of zlib
for specific programs exist, use LD_PRELOAD.
If the program is dynamically linked with zlib, then zlib-ng can take
its place without risking system-wide instability. Ex:
LD_PRELOAD=/opt/zlib-ng/libz.so.1.2.11.zlib-ng /usr/bin/program

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


Build Status
------------

Travis CI: [![build status](https://api.travis-ci.org/zlib-ng/zlib-ng.svg)](https://travis-ci.org/zlib-ng/zlib-ng/)
Buildkite: [![Build status](https://badge.buildkite.com/7bb1ef84356d3baee26202706cc053ee1de871c0c712b65d26.svg?branch=develop)](https://buildkite.com/circlestorm-productions/zlib-ng)
