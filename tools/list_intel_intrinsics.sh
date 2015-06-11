#!/bin/sh

fgrep _mm_ "$@" | sed -e 's/.*\(_mm_[^\(]*\)(.*/\1/g' | sort -u
