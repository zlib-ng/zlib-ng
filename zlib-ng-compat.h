/* zlib-ng-compat.h -- zlib function aliasing to zlib-ng functions
 * For conditions of distribution and use, see copyright notice in LICENSE.md
 *
 * This file provides aliases to help with porting efforts from zlib to zlib-ng,
 * it does not handle differences in data types, although in most cases this
 * is fine since int == int32_t, etc. Expect type conversion compiler warnings.
 *
 * The application using this also needs to link to zlib-ng instead of zlib.
 */

#ifndef ZLIB_NG_COMPAT_H_
#define ZLIB_NG_COMPAT_H_

// Ensure zlib-ng-compat.h is included instead of zlib.h
#if defined(ZLIB_H_) || defined(ZLIB_H)
#  error zlib-ng-compat.h is included, do not include zlib.h as well.
#endif

#include <zlib-ng.h>

// Defines to ensure backwards compatibility
#define ZLIB_VERSION "1.3.1.zlib-ng-compat"
#define ZLIB_VERNUM 0x131f
#define ZLIB_VER_MAJOR 1
#define ZLIB_VER_MINOR 3
#define ZLIB_VER_REVISION 1
#define ZLIB_VER_SUBREVISION 15

// Type aliases
#define z_stream_s              zng_stream_s
#define z_stream                zng_stream
#define gz_header_s             zng_gz_header_s
#define gz_header               zng_gz_header

typedef zng_stream              *z_streamp;
typedef zng_gz_header           *gz_headerp;

// Function aliases
#define zlib_version            zlibng_version()
#define zlibVersion             zlibng_version

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

#define zlibCompileFlags        zng_zlibCompileFlags

#define compress                zng_compress
#define compress2               zng_compress2
#define compressBound           zng_compressBound
#define uncompress              zng_uncompress
#define uncompress2             zng_uncompress2

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
