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
