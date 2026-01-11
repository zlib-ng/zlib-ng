# detect-coverage.cmake -- Detect supported compiler coverage flags
# Licensed under the Zlib license, see LICENSE.md for details

# Attempt to enable gcov-style code coverage
macro(add_code_coverage)
    # Check for --coverage flag support
    set(CMAKE_REQUIRED_LINK_OPTIONS -coverage)
    check_c_compiler_flag("--coverage" HAVE_COVERAGE)
    set(CMAKE_REQUIRED_LINK_OPTIONS)
    if(HAVE_COVERAGE)
        # Check for --coverage -fcondition-coverage flag support
        set(CMAKE_REQUIRED_LINK_OPTIONS -coverage -fcondition-coverage)
        check_c_compiler_flag("--coverage -fcondition-coverage -Wno-coverage-too-many-conditions" HAVE_CONDITION_COVERAGE)
        set(CMAKE_REQUIRED_LINK_OPTIONS)

        if(HAVE_CONDITION_COVERAGE)
            # Both --coverage and -fcondition-coverage supported
            add_link_options(-coverage -fcondition-coverage)
            add_compile_options(--coverage -fcondition-coverage -Wno-coverage-too-many-conditions)
            message(STATUS "Code coverage enabled using: --coverage -fcondition-coverage")
        else()
            # Only --coverage supported.
            add_link_options(-coverage)
            add_compile_options(--coverage)
            message(STATUS "Code coverage enabled using: --coverage")
        endif()
    else()
        # Some versions of GCC don't support --coverage shorthand
        set(CMAKE_REQUIRED_LINK_OPTIONS -lgcov -fprofile-arcs)
        check_c_compiler_flag("-ftest-coverage -fprofile-arcs" HAVE_TEST_COVERAGE)
        set(CMAKE_REQUIRED_LINK_OPTIONS)

        if(HAVE_TEST_COVERAGE)
            add_link_options(-lgcov -fprofile-arcs)
            add_compile_options(-ftest-coverage -fprofile-arcs)
            message(STATUS "Code coverage enabled using: -ftest-coverage -fprofile-arcs")
        else()
            # Failed to enable coverage, this is fatal to avoid silent failures in CI
            message(FATAL_ERROR "WITH_CODE_COVERAGE requested, but unable to turn on code coverage with compiler/linker")
            set(WITH_CODE_COVERAGE OFF)
        endif()
    endif()

    # Set optimization level to zero for code coverage builds
    if (WITH_CODE_COVERAGE)
        # Use CMake compiler flag variables due to add_compile_options failure on Windows GCC
        set(CMAKE_C_FLAGS "-O0 ${CMAKE_C_FLAGS}")
        set(CMAKE_CXX_FLAGS "-O0 ${CMAKE_CXX_FLAGS}")
    endif()
endmacro()
