# Neither Windows nor macOS support ISO C11 `aligned_alloc`.
#  https://social.msdn.microsoft.com/Forums/vstudio/4617cd56-4c8d-4717-a96b-348ad3b2f149
#  https://developer.apple.com/forums/thread/81413
# macOS supports POSIX.1-2001 `posix_memalign`.
#  https://opensource.apple.com/source/Libc/Libc-825.25/gen/posix_memalign.3.auto.html
# Android supports `aligned_alloc` from API 28.
#  https://github.com/aosp-mirror/platform_bionic/commit/cae21a9b53a10f0cba79bf6783c4a5af16228fed
# Android supports `posix_memalign` from API 17 (but is available on API 16 via
# libandroid_support.a, which is linked by default when targeting API 16).
#  https://github.com/aosp-mirror/platform_bionic/commit/85aad909560508410101c18c6ecc6633df39c596
#  https://github.com/aosp-mirror/platform_bionic/commit/e219cefc173bf93b8ff710431784e5de30ffab8f
if(WIN32)
    return()
elseif(APPLE)
    add_definitions(-D MAL_IMPL=1)
elseif(ANDROID)
    # TODO: if API level >= 28, MAL_IMPL=2
    # TODO: elif API level >= 17, MAL_IMPL=1
    # TODO: else, MAL_IMPL=0
elseif(NOT CMAKE_C_STANDARD EQUAL 99)
    return()
endif()

include(CMakePushCheckState)

cmake_push_check_state(RESET)
set(CMAKE_REQUIRED_DEFINITIONS -D _POSIX_C_SOURCE=200112L)
check_function_exists(posix_memalign HAVE_POSIX_MEMALIGN)
if(HAVE_POSIX_MEMALIGN)
    add_definitions(-D _POSIX_C_SOURCE=200112L)
endif()
cmake_pop_check_state()
