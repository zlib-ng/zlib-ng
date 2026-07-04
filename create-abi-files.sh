#!/bin/sh
[ -f test/abicheck.sh ] || exit
case "$1" in
  ""|--refresh) ;;
  *) echo "Usage: $0 [--refresh]" >&2
     exit 1
     ;;
esac
[ "$1" = "--refresh" ] && git rm -f test/abi/*.abi
#
test/abicheck.sh --refresh-if || exit
test/abicheck.sh --zlib-compat --refresh-if || exit
CFLAGS=-m32 LDFLAGS=-m32 test/abicheck.sh --refresh-if || exit
CFLAGS=-m32 LDFLAGS=-m32 test/abicheck.sh --zlib-compat --refresh-if || exit
CC=aarch64-linux-gnu-gcc CHOST=aarch64-linux-gnu test/abicheck.sh --refresh-if || exit
CC=aarch64-linux-gnu-gcc CHOST=aarch64-linux-gnu test/abicheck.sh --zlib-compat --refresh-if || exit
CC=arm-linux-gnueabi-gcc CHOST=arm-linux-gnueabi test/abicheck.sh --refresh-if || exit
CC=arm-linux-gnueabi-gcc CHOST=arm-linux-gnueabi test/abicheck.sh --zlib-compat --refresh-if || exit
CC=arm-linux-gnueabihf-gcc CHOST=arm-linux-gnueabihf test/abicheck.sh --refresh-if || exit
CC=arm-linux-gnueabihf-gcc CHOST=arm-linux-gnueabihf test/abicheck.sh --zlib-compat --refresh-if || exit
CC=loongarch64-linux-gnu-gcc-14 CHOST=loongarch64-linux-gnu test/abicheck.sh --refresh-if || exit
CC=loongarch64-linux-gnu-gcc-14 CHOST=loongarch64-linux-gnu test/abicheck.sh --zlib-compat --refresh-if || exit
CC=mips-linux-gnu-gcc CHOST=mips-linux-gnu test/abicheck.sh --refresh-if || exit
CC=mips-linux-gnu-gcc CHOST=mips-linux-gnu test/abicheck.sh --zlib-compat --refresh-if || exit
CC=mips64-linux-gnuabi64-gcc CHOST=mips64-linux-gnuabi64 test/abicheck.sh --refresh-if || exit
CC=mips64-linux-gnuabi64-gcc CHOST=mips64-linux-gnuabi64 test/abicheck.sh --zlib-compat --refresh-if || exit
CC=powerpc-linux-gnu-gcc CHOST=powerpc-linux-gnu test/abicheck.sh --refresh-if || exit
CC=powerpc-linux-gnu-gcc CHOST=powerpc-linux-gnu test/abicheck.sh --zlib-compat --refresh-if || exit
CC=powerpc64-linux-gnu-gcc CHOST=powerpc64-linux-gnu test/abicheck.sh --refresh-if || exit
CC=powerpc64-linux-gnu-gcc CHOST=powerpc64-linux-gnu test/abicheck.sh --zlib-compat --refresh-if || exit
CC=powerpc64le-linux-gnu-gcc CHOST=powerpc64le-linux-gnu test/abicheck.sh --refresh-if || exit
CC=powerpc64le-linux-gnu-gcc CHOST=powerpc64le-linux-gnu test/abicheck.sh --zlib-compat --refresh-if || exit
CC=riscv64-linux-gnu-gcc CHOST=riscv64-linux-gnu test/abicheck.sh --refresh-if || exit
CC=riscv64-linux-gnu-gcc CHOST=riscv64-linux-gnu test/abicheck.sh --zlib-compat --refresh-if || exit
CC=s390x-linux-gnu-gcc CHOST=s390x-linux-gnu test/abicheck.sh --refresh-if || exit
CC=s390x-linux-gnu-gcc CHOST=s390x-linux-gnu test/abicheck.sh --zlib-compat --refresh-if || exit
CC=sparc64-linux-gnu-gcc CHOST=sparc64-linux-gnu test/abicheck.sh --refresh-if || exit
CC=sparc64-linux-gnu-gcc CHOST=sparc64-linux-gnu test/abicheck.sh --zlib-compat --refresh-if || exit
#
git add test/abi/*.abi
