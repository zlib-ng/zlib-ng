#!/bin/sh
# exportcheck.sh -- Fails if a static library exposes symbols beyond the public API

# Copyright (C) 2026 Nathan Moinvaziri
# Licensed under the Zlib license, see LICENSE.md for details

# Every function or object with default visibility in the archive must be declared with Z_EXTERN
# in the public header or listed in the global section of the linker map. Anything else is
# missing Z_INTERNAL and would leak into the dynamic symbol table of any shared library that
# links the archive.

set -e
TESTDIR="$(cd "$(dirname "$0")"; pwd)"
SRCDIR="$(dirname "$TESTDIR")"

usage() {
    cat <<_EOF_
Usage: $0 [--zlib-compat] [--symbol-prefix=PREFIX] [--allow=SYMBOL] ARCHIVE
Check that every visible symbol in ARCHIVE is declared in the public header or linker map.
Options:
--zlib-compat          - check the zlib-compatible flavor of zlib-ng.
--symbol-prefix=PREFIX - prefix given to exported symbols via ZLIB_SYMBOL_PREFIX.
--allow=SYMBOL         - permit SYMBOL even though it is not declared.
Obeys NM and READELF.
_EOF_
}

compat=false
prefix=""
allowed=""
archive=""
for arg in "$@"; do
    case "$arg" in
    --zlib-compat) compat=true ;;
    --symbol-prefix=*) prefix="${arg#*=}" ;;
    --allow=*) allowed="$allowed
${arg#*=}" ;;
    -h|--help) usage; exit 0 ;;
    -*) echo "Unknown option: $arg"; usage; exit 1 ;;
    *) archive="$arg" ;;
    esac
done
if test -z "$archive"; then
    usage
    exit 1
fi

if $compat; then
    header="$SRCDIR/zlib.h.in"
    mapfile="$SRCDIR/zlib.map.in"
else
    header="$SRCDIR/zlib-ng.h.in"
    mapfile="$SRCDIR/zlib-ng.map.in"
fi

# Defined symbols that are visible outside the archive
case "$(uname -s)" in
Darwin)
    visible=$(${NM:-nm} -m "$archive" \
        | sed -n 's/^[0-9a-f]* ([^)]*) \(\[[^]]*\] \)*\(weak \)\{0,1\}external _\([^ ]*\).*/\3/p' | sort -u)
    ;;
*)
    visible=$(${READELF:-readelf} -sW "$archive" \
        | awk '$4 ~ /^(FUNC|OBJECT|IFUNC)$/ && $5 ~ /^(GLOBAL|WEAK)$/ && $6 == "DEFAULT" && $7 ~ /^[0-9]+$/ { print $8 }' | sort -u)
    ;;
esac
# The archive always defines the public API, so an empty list means the tool output was not parsed
if test -z "$visible"; then
    echo "exportcheck: FAIL: no visible symbols found in $archive"
    exit 1
fi

# Functions declared with Z_EXTERN in the public header, across every #if branch
declared=$(awk -v prefix="$prefix" '
    { text = text $0 "\n" }
    END {
        # Drop comments first so quotes inside them cannot open a string literal
        code = ""
        while ((start = index(text, "/*")) > 0) {
            code = code substr(text, 1, start - 1) " "
            text = substr(text, start + 2)
            stop = index(text, "*/")
            text = stop ? substr(text, stop + 2) : ""
        }
        text = code text
        # Drop string literals so parentheses inside deprecation messages cannot pass for a name
        gsub(/"([^"\\]|\\.)*"/, "", text)
        gsub(/\\\n/, "", text)
        n = split(text, lines, "\n")
        code = ""
        for (i = 1; i <= n; i++)
            if (lines[i] !~ /^[ \t]*#/)
                code = code " " lines[i]
        n = split(code, statements, ";")
        for (i = 1; i <= n; i++) {
            if (statements[i] !~ /Z_EXTERN/)
                continue
            # Z_DEPRECATED(msg) precedes the name and would otherwise be taken for it
            gsub(/Z_DEPRECATED[ \t]*\([ \t]*\)/, "", statements[i])
            if (match(statements[i], /[A-Za-z_][A-Za-z_0-9]*[ \t]*\(/)) {
                name = substr(statements[i], RSTART, RLENGTH)
                sub(/[ \t]*\($/, "", name)
                print prefix name
            }
        }
    }' "$header")
# Symbols declared in the global sections of the linker map
declared="$declared
$(sed "s/@ZLIB_SYMBOL_PREFIX@/$prefix/g" "$mapfile" \
    | awk '/[{]/ { global = 1; next }
           /global:/ { global = 1; next }
           /local:/ { global = 0; next }
           /[}]/ { next }
           global && /;/ && !/\*/ { sub(/^[ \t]+/, ""); sub(/;.*/, ""); print }')
$allowed"

# Declared symbols come first, then a separator, then the visible symbols to check
unexpected=$(printf '%s\n--\n%s\n' "$declared" "$visible" \
    | awk '/^--$/ { checking = 1; next }
           !checking { known[$0] = 1; next }
           !known[$0]')
if test -n "$unexpected"; then
    echo "exportcheck: FAIL: symbols visible in $archive but not declared for export:"
    printf '%s\n' "$unexpected" | sed 's/^/  /'
    exit 1
fi
echo "exportcheck: OK: $(printf '%s\n' "$visible" | grep -c .) visible symbols, all declared for export"
