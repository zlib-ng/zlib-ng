/* zlib-ng-compat.h -- header aliasing the zlib API onto the native zlib-ng API
 * For conditions of distribution and use, see copyright notice in LICENSE.md
 *
 * This is a stepping stone for applications that want to compile against zlib-ng
 * without deep code changes. Moving an application from zlib to zlib-ng takes
 * exactly two changes:
 *
 *   1. include <zlib-ng-compat.h> instead of <zlib.h>,
 *   2. link to libz-ng.so or libz-ng.a, instead of zlib.so or libz.a.
 *
 * The calls, structs, constants and field names then stay exactly as the
 * application already wrote them for zlib, so the porting work can happen later,
 * one file at a time, or not at all; drop this header once the application uses
 * the zng_ prefixed API directly. This file can also allow an application to
 * support both zlib API and zlib-ng API using compile-time selection.
 *
 * It is a best effort aid and not a compatibility guarantee: it does not handle
 * differences in data types, although in most cases this is fine since int ==
 * int32_t, etc, so expect type conversion compiler warnings, and it does not
 * declare anything that the library was configured without. Include this header
 * instead of zlib.h, never both.
 */

#ifndef ZLIB_NG_COMPAT_H_
#define ZLIB_NG_COMPAT_H_

/* Ensure zlib-ng-compat.h is included instead of zlib.h */
#if defined(ZLIB_H_) || defined(ZLIB_H)
#  error zlib-ng-compat.h is included, do not include zlib.h as well.
#endif

#include <zlib-ng.h>

/* zlibVersion maps to zlibng_version(), which returns ZLIBNG_VERSION, so take
 * ZLIB_VERSION from that same macro and the documented "strcmp(zlibVersion(),
 * ZLIB_VERSION)" consistency check passes instead of always failing. The numeric
 * values stay in zlib form, matching the numbers zlib-ng's own zlib-compat
 * header uses today, so gates such as "#if ZLIB_VERNUM >= 0x1280" behave as
 * they do there. */
#define ZLIB_VERSION ZLIBNG_VERSION
#define ZLIB_VERNUM 0x132f
#define ZLIB_VER_MAJOR 1
#define ZLIB_VER_MINOR 3
#define ZLIB_VER_REVISION 2
#define ZLIB_VER_SUBREVISION 15    /* 15=fork (0xf), as in zlib.h.in */

/* Type aliases */
#define z_stream_s              zng_stream_s
#define z_stream                zng_stream
#define gz_header_s             zng_gz_header_s
#define gz_header               zng_gz_header

typedef zng_stream              *z_streamp;
typedef zng_gz_header           *gz_headerp;

typedef uint32_t z_crc_t;
typedef size_t   z_size_t;

#define ZEXTERN                 Z_EXTERN
#define ZEXPORT                 Z_EXPORT
#define ZEXPORTVA               Z_EXPORT

/* Function aliases */
#define zlib_version            zlibng_version()
#define zlibVersion             zlibng_version
#define zlibCompileFlags        zng_zlibCompileFlags

#define deflateInit             zng_deflateInit
#define deflate                 zng_deflate
#define deflateEnd              zng_deflateEnd

#define inflateInit             zng_inflateInit
#define inflate                 zng_inflate
#define inflateEnd              zng_inflateEnd

#define deflateInit2            zng_deflateInit2
#define deflateSetDictionary    zng_deflateSetDictionary
#define deflateGetDictionary    zng_deflateGetDictionary
#define deflateCopy             zng_deflateCopy
#define deflateReset            zng_deflateReset
#define deflateParams           zng_deflateParams
#define deflateTune             zng_deflateTune
#define deflateBound            zng_deflateBound
#define deflatePending          zng_deflatePending
#define deflatePrime            zng_deflatePrime
#define deflateSetHeader        zng_deflateSetHeader

#define inflateInit2            zng_inflateInit2
#define inflateSetDictionary    zng_inflateSetDictionary
#define inflateGetDictionary    zng_inflateGetDictionary
#define inflateSync             zng_inflateSync
#define inflateCopy             zng_inflateCopy
#define inflateReset            zng_inflateReset
#define inflateReset2           zng_inflateReset2
#define inflatePrime            zng_inflatePrime
#define inflateMark             zng_inflateMark
#define inflateGetHeader        zng_inflateGetHeader

#define inflateBackInit         zng_inflateBackInit
#define inflateBack             zng_inflateBack
#define inflateBackEnd          zng_inflateBackEnd
#define deflateUsed             zng_deflateUsed

#define compress                zng_compress
#define compress2               zng_compress2
#define compressBound           zng_compressBound
#define uncompress              zng_uncompress
#define uncompress2             zng_uncompress2

#define gzopen64                zng_gzopen
#define gzseek64                zng_gzseek
#define gztell64                zng_gztell
#define gzoffset64              zng_gzoffset
#define adler32_combine64       zng_adler32_combine
#define crc32_combine64         zng_crc32_combine
#define crc32_combine_gen64     zng_crc32_combine_gen

#define gzopen                  zng_gzopen
#define gzdopen                 zng_gzdopen
#define gzbuffer                zng_gzbuffer
#define gzsetparams             zng_gzsetparams
#define gzread                  zng_gzread
#define gzfread                 zng_gzfread
#define gzwrite                 zng_gzwrite
#define gzfwrite                zng_gzfwrite
#define gzprintf                zng_gzprintf
#define gzputs                  zng_gzputs
#define gzgets                  zng_gzgets
#define gzputc                  zng_gzputc
#define gzungetc                zng_gzungetc
#define gzflush                 zng_gzflush
#define gzseek                  zng_gzseek
#define gzrewind                zng_gzrewind
#define gztell                  zng_gztell
#define gzoffset                zng_gzoffset
#define gzeof                   zng_gzeof
#define gzdirect                zng_gzdirect
#define gzclose                 zng_gzclose
#define gzclose_r               zng_gzclose_r
#define gzclose_w               zng_gzclose_w
#define gzerror                 zng_gzerror
#define gzclearerr              zng_gzclearerr

#define adler32                 zng_adler32
#define adler32_z               zng_adler32_z
#define adler32_combine         zng_adler32_combine
#define crc32                   zng_crc32
#define crc32_z                 zng_crc32_z
#define crc32_combine           zng_crc32_combine
#define crc32_combine_gen       zng_crc32_combine_gen
#define crc32_combine_op        zng_crc32_combine_op

#define gzgetc                  zng_gzgetc
#define gzgetc_                 zng_gzgetc

#define zError                  zng_zError
#define inflateSyncPoint        zng_inflateSyncPoint
#define get_crc_table           zng_get_crc_table
#define inflateUndermine        zng_inflateUndermine
#define inflateValidate         zng_inflateValidate
#define inflateCodesUsed        zng_inflateCodesUsed
#define inflateResetKeep        zng_inflateResetKeep
#define deflateResetKeep        zng_deflateResetKeep

#define gzopen_w                zng_gzopen_w
#define gzvprintf               zng_gzvprintf

#endif /* ZLIB_NG_COMPAT_H_ */
