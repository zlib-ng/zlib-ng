Porting applications to use zlib-ng
===================================

Zlib-ng can be used/compiled in two different modes, that require some
consideration by the application developer.

Changes from zlib affecting native and compat modes
---------------------------------------------------
Zlib-ng is not as conservative with memory allocation as Zlib is.

Where Zlib's inflate will allocate a lower amount of memory depending on
compression level and window size, zlib-ng will always allocate the maximum
amount of memory and possibly leave parts of it unused.
Zlib-ng's deflate will however allocate a lower amount of memory depending
on compression level and window size.

Zlib-ng also allocates one "big" buffer instead of doing multiple smaller
allocations. This is faster, can lead to better cache locality and reduces
space lost to alignment padding.

At the time of writing, by default zlib-ng allocates the following amounts
of memory on a 64-bit system (except on S390x that requires ~4KiB more):
-  Deflate: 350.272 Bytes
-  Inflate:  42.112 Bytes

**Advantages:**
- All memory is allocated during DeflateInit or InflateInit functions,
  leaving the actual deflate/inflate functions free from allocations.
- Zlib-ng can only fail from memory allocation errors during init.
- Time spent doing memory allocation systemcalls is all done during init,
  allowing applications to do prepare this before doing latency-sensitive
  deflate/inflate later.
- Can reduce wasted memory due to buffer alignment padding both by OS and zlib-ng.
- Potentially improved memory locality.

**Disadvantages:**
- Zlib-ng allocates a little more memory than zlib does.

zlib-compat mode
----------------
Zlib-ng can be compiled in zlib-compat mode, suitable for zlib-replacement
in a single application or system-wide.

Please note that zlib-ng in zlib-compat mode tries to maintain both API and
ABI compatibility with the original zlib. Any issues regarding compatibility
can be reported as bugs.

In certain instances you may not be able to simply replace the zlib library/dll
files and expect the application to work. The application may need to be
recompiled against the zlib-ng headers and libs to ensure full compatibility.

It is also possible for the deflate output stream to differ from the original
zlib due to algorithmic differences between the two libraries. Any tests or
applications that depend on the exact length of the deflate stream being a
certain value will need to be updated.

**Advantages:**
- Easy to port to, since it only requires a recompile of the application and
  no changes to the application code.

**Disadvantages:**
- Can conflict with a system-installed zlib, as that can often be linked in
  by another library you are linking into your application. This can cause
  crashes or incorrect output.
- If your application is pre-allocating a memory buffer and you are providing
  deflate/inflate init with your own allocator that allocates from that buffer
  (looking at you nginx), you should be aware that zlib-ng needs to allocate
  more memory than stock zlib needs. The same problem exists with Intel’s and
  Cloudflare’s zlib forks. Doing this is not recommended since it makes it
  very hard to maintain compatibility over time.

**Build Considerations:**
- Compile against the *zlib.h* provided by zlib-ng
- Configuration header is named *zconf.h*
- Static library is *libz.a* on Unix and macOS, or *zlib.lib* on Windows
- Shared library is *libz.so* on Unix, *libz.dylib* on macOS, or *zlib1.dll*
  on Windows
- Type `z_size_t` is *unsigned __int64* on 64-bit Windows, and *unsigned long* on 32-bit Windows, Unix and macOS
- Type `z_uintmax_t` is *unsigned long* in zlib-compat mode, and *size_t* with zlib-ng API

zlib-ng native mode
-------------------
Zlib-ng in native mode is suitable for co-existing with the standard zlib
library, allowing applications to implement support and testing separately.

The zlib-ng native has implemented some modernization and simplifications
in its API, intended to make life easier for application developers.

**Advantages:**
- Does not conflict with other zlib implementations, and can co-exist as a
  system library along with zlib.
- In certain places zlib-ng native uses more appropriate data types, removing
  the need for some workarounds in the API compared to zlib.

**Disadvantages:**
- Requires minor changes to applications to use the prefixed zlib-ng
  function calls and structs. Usually this means a small prefix `zng_` has to be added.

**Build Considerations:**
- Compile against *zlib-ng.h*
- Configuration header is named *zconf-ng.h*
- Static library is *libz-ng.a* on Unix and macOS, or *zlib-ng.lib* on Windows
- Shared library is *libz-ng.so* on Unix, *libz-ng.dylib* on macOS, or
  *zlib-ng2.dll* on Windows
- Type `z_size_t` is *size_t*

zlib-ng-compat.h alias header
-----------------------------
Zlib-ng native mode also installs a header named *zlib-ng-compat.h*, which
aliases the zlib API onto the prefixed zlib-ng functions. It is the middle
ground between zlib-compat mode and porting an application to the `zng_`
prefixed API: the library keeps the zlib-ng names, so nothing can conflict with
a system-installed zlib, while the application code keeps using the zlib names.

Moving an application from zlib to zlib-ng takes two changes:
- include *zlib-ng-compat.h* instead of *zlib.h*,
- link the zlib-ng library instead of zlib.

Calls, structs, constants and field names then stay exactly as they were
written for zlib, so the porting work can be done later, one file at a time,
or be skipped entirely; the header can be dropped whenever the application uses
the zlib-ng API directly. Including this header on one side of an `#ifdef` and
*zlib-ng.h* on the other lets one application support the zlib API and the
zlib-ng API with compile-time selection.

**Advantages:**
- Only the included header name and the library linked change, application
  sources stay mostly untouched, which makes it easy to test zlib-ng early.
- No conflict with a system zlib, unlike zlib-compat mode, since nothing in
  the library or the installed names collides.
- An application can move to zlib-ng now and modernize the code later, or
  support both APIs side by side.

**Disadvantages:**
- It is a best effort aid, not a compatibility guarantee: it does not handle
  type differences, so expect conversion warnings.
- ZLIB_VERSION holds the zlib-ng version string, matching what the version
  function returns, while the numeric macros keep zlib-style values (currently
  ZLIB_VERNUM 0x132f) so version gates behave as they do in zlib-compat mode.

**Build Considerations:**
- Compile against *zlib-ng-compat.h*, installed along with *zlib-ng.h* unless
  zlib-ng is configured with -DWITH_COMPAT_HEADER=OFF. It is not installed by
  zlib-compat mode, which installs the real *zlib.h*.
- Everything else in native mode applies: configuration header is *zconf-ng.h*,
  static library is *libz-ng.a* on Unix and macOS, shared library is *libz-ng.so*
  on Unix, *libz-ng.dylib* on macOS, or *zlib-ng2.dll* on Windows.
- Include this header instead of *zlib.h*, never both; it can be included
  together with *zlib-ng.h* in the same file, which is handy while porting
  one file at a time.
- The gzip file functions follow the WITH_GZFILEOP option of the build, the
  header enables their declarations, but a zlib-ng configured
  -DWITH_GZFILEOP=OFF does not contain them, which then fails at link time.
- Type `z_size_t` is *size_t*, as in native mode, and type `z_uintmax_t` is
  *size_t*, not *unsigned long* as in zlib-compat mode.

zlib-ng compile-time detection
------------------------------

To distinguish zlib-ng from other zlib implementations at compile-time check for the
existence of `ZLIBNG_VERSION` defined in the zlib header.
