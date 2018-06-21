#ifndef _ZBUILD_H
#define _ZBUILD_H

/* This has to be first include that defines any types */
#if defined(_MSC_VER)
#  include <windows.h>
   typedef SSIZE_T ssize_t;
#  define __thread __declspec(thread)
#endif

#if defined(ZLIB_COMPAT)
#  define PREFIX(x) x
#  define PREFIX2(x) ZLIB_ ## x
#  define PREFIX3(x) z_ ## x
#  define zVersion zlibVersion
#  define z_size_t unsigned long
#else
#  define PREFIX(x) zng_ ## x
#  define PREFIX2(x) ZLIBNG_ ## x
#  define PREFIX3(x) zng_ ## x
#  define zVersion zlibng_version
#  define z_size_t size_t
#endif

#endif
