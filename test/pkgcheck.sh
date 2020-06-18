#!/bin/sh
# Verify that the various build systems produce identical results on a Unixlike system
set -ex

# If suffix not set to "", default to -ng
suffix=${suffix--ng}

# Use same compiler for make and cmake builds
if test "$CC"x = ""x
then
  if clang --version
  then
    export CC=clang
  elif gcc --version
  then
    export CC=gcc
  fi
fi

# New build system
# Happens to delete top-level zconf.h
# (which itself is a bug, https://github.com/madler/zlib/issues/162 )
# which triggers another bug later in configure,
# https://github.com/madler/zlib/issues/499
rm -rf btmp2 pkgtmp2
mkdir btmp2 pkgtmp2
export DESTDIR=$(pwd)/pkgtmp2
cd btmp2
  cmake -G Ninja ..
  ninja -v
  ninja install
cd ..

# Original build system
rm -rf btmp1 pkgtmp1
mkdir btmp1 pkgtmp1
export DESTDIR=$(pwd)/pkgtmp1
cd btmp1
  case $(uname) in
  Darwin)
    export LDFLAGS="-Wl,-headerpad_max_install_names"
    ;;
  Linux)
    if grep -i fedora /etc/os-release > /dev/null
    then
        # Note: Fedora patches cmake to use -O2 in release, which
        # does not match the -O3 configure sets :-(
        export CFLAGS="-O2 -DNDEBUG"
    fi
    ;;
  esac
  ../configure
  make
  make install
cd ..

if diff --exclude '*.so*' --exclude '*.a' -Nur pkgtmp1 pkgtmp2
then
  echo pkgcheck-cmake-bits-identical PASS
else
  echo pkgcheck-cmake-bits-identical FAIL
  exit 1
fi
