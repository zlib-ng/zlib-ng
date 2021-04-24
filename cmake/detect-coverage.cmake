# detect-coverage.cmake -- Detect supported compiler coverage flags
# Licensed under the Zlib license, see LICENSE.md for details

macro(add_code_coverage)
    # Check for -coverage flag support for Clang/GCC
    if(CMAKE_VERSION VERSION_LESS 3.14)
        set(CMAKE_REQUIRED_LIBRARIES -lgcov)
    else()
        set(CMAKE_REQUIRED_LINK_OPTIONS -coverage)
    endif()
    check_c_compiler_flag(-coverage HAVE_COVERAGE)
    set(CMAKE_REQUIRED_LIBRARIES)
    set(CMAKE_REQUIRED_LINK_OPTIONS)

    if(HAVE_COVERAGE)
        set(CMAKE_C_FLAGS "-O0 ${CMAKE_C_FLAGS} -coverage")
        set(CMAKE_EXE_LINKER_FLAGS "${CMAKE_EXE_LINKER_FLAGS} -coverage")
        set(CMAKE_SHARED_LINKER_FLAGS "${CMAKE_SHARED_LINKER_FLAGS} -coverage")
    else()
        # Some versions of GCC don't support -coverage shorthand
        if(CMAKE_VERSION VERSION_LESS 3.14)
            set(CMAKE_REQUIRED_LIBRARIES -lgcov)
        else()
            set(CMAKE_REQUIRED_LINK_OPTIONS -lgcov -fprofile-arcs)
        endif()
        check_c_compiler_flag("-ftest-coverage -fprofile-arcs -fprofile-values" HAVE_TEST_COVERAGE)
        set(CMAKE_REQUIRED_LIBRARIES)
        set(CMAKE_REQUIRED_LINK_OPTIONS)

        if(HAVE_TEST_COVERAGE)
            set(CMAKE_C_FLAGS "-O0 ${CMAKE_C_FLAGS} -ftest-coverage -fprofile-arcs -fprofile-values")
            set(CMAKE_EXE_LINKER_FLAGS "${CMAKE_EXE_LINKER_FLAGS} -lgcov -fprofile-arcs")
            set(CMAKE_SHARED_LINKER_FLAGS "${CMAKE_SHARED_LINKER_FLAGS} -lgcov -fprofile-arcs")
        else()
            message(WARNING "Compiler does not support code coverage")
        endif()
    endif()
endmacro()
