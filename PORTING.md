Porting applications to use zlib-ng
-----------------------------------

Zlib-ng can be used/compiled in two different modes, that require some
consideration by the application developer.

zlib-compat mode
----------------
Zlib-ng can be compiled in zlib-compat mode, suitable for zlib-replacement
in a single application or system-wide.

Please note that zlib-ng in zlib-compat mode is API-compatible but not
ABI-compatible, meaning that you cannot simply replace the zlib library/dll
files and expect the application to work. The application will need to be
recompiled against the zlib-ng headers and libs to ensure full compatability.

Zlib-ng can be distinguished from other zlib implementations at compile-time
by checking for the existence of ZLIBNG_VERSION defined in the zlib.h header.

Compile against the *zlib.h* provided by zlib-ng.

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
  more memory than stock zlib needs. The same problem exists with Intels and
  Cloudflares zlib forks. Doing this is not recommended since it makes it
  very hard to maintain compatibility over time.


zlib-ng native mode
-------------------
Zlib-ng in native mode is suitable for co-existing with the standard zlib
library, allowing applications to implement support and testing separately.

The zlib-ng native has implemented some modernization and simplifications
in its API, intended to make life easier for application developers.

Compile against *zlib-ng.h*.

**Advantages:**
- Does not conflict with other zlib implementations, and can co-exist as a
  system library along with zlib.
- In certain places zlib-ng native uses more appropriate data types, removing
  the need for some workarounds in the API compared to zlib.

**Disadvantages:**
- Requires minor changes to applications to use the prefixed zlib-ng
  function calls and structs. Usually this means a small prefix `zng_` has to be added.
