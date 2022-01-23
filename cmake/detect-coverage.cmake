# detect-coverage.cmake -- Detect supported compiler coverage flags
# Licensed under the Zlib license, see LICENSE.md for details

macro(add_code_coverage)
    if(CMAKE_VERSION VERSION_LESS 3.14)
        message(FATAL_ERROR "CMake 3.14 or later is required for code coverage")
    endif()

    # Check for -coverage flag support for Clang/GCC
    set(CMAKE_REQUIRED_LINK_OPTIONS -coverage)
    check_c_compiler_flag(-coverage HAVE_COVERAGE)
    set(CMAKE_REQUIRED_LINK_OPTIONS)

    if(HAVE_COVERAGE)
        add_compile_options(-O0 -coverage)
        add_link_options(-coverage)
    else()
        # Some versions of GCC don't support -coverage shorthand
        set(CMAKE_REQUIRED_LINK_OPTIONS -lgcov -fprofile-arcs)
        check_c_compiler_flag("-ftest-coverage -fprofile-arcs -fprofile-values" HAVE_TEST_COVERAGE)
        set(CMAKE_REQUIRED_LINK_OPTIONS)

        if(HAVE_TEST_COVERAGE)
            add_compile_options(-O0 -ftest-coverage -fprofile-arcs -fprofile-values)
            add_link_options(-lgcov -fprofile-arcs)
        else()
            message(WARNING "Compiler does not support code coverage")
        endif()
    endif()
endmacro()
