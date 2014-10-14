# Makefile for zlib
# Copyright (C) 1995-2013 Jean-loup Gailly, Mark Adler
# For conditions of distribution and use, see copyright notice in zlib.h

all: x86.o x86.lo

x86.o:
	gcc -O3 -I. -c -o $@x86.c

x86.lo:
	-@mkdir objs 2>/dev/null || test -d objs
	gcc -O3 -I. -c -o $@ x86.c
	-@mv objs/$*.o $@

mostlyclean: clean
clean:
	rm -f *.o *.lo *~ \
	rm -rf objs
	rm -f *.gcda *.gcno *.gcov
