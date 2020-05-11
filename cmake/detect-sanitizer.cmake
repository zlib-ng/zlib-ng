# detect-sanitizer.cmake -- Detect supported compiler sanitizer flags
# Licensed under the Zlib license, see LICENSE.md for details

macro(check_sanitizer_support known_checks supported_checks)
    set(available_checks "")

    # Build list of supported sanitizer flags by incrementally trying compilation with
    # known sanitizer checks

    foreach(check ${known_checks})
        if(available_checks STREQUAL "")
            set(compile_checks "${check}")
        else()
            set(compile_checks "${available_checks},${check}")
        endif()

        set(CMAKE_REQUIRED_FLAGS "-fsanitize=${compile_checks}")

        check_c_source_compiles("int main() { return 0; }" HAS_SANITIZER_${check}
            FAIL_REGEX "not supported|unrecognized command|unknown option")

        set(CMAKE_REQUIRED_FLAGS)

        if(HAS_SANITIZER_${check})
            set(available_checks ${compile_checks})
        endif()
    endforeach()

    set(${supported_checks} ${available_checks})
endmacro()

macro(add_memory_sanitizer_check)
    check_sanitizer_support("memory" supported_checks)
    if(NOT ${supported_checks} STREQUAL "")
        message(STATUS "Memory sanitizer is enabled")
        set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} -fsanitize=${supported_checks}")
    else()
        message(STATUS "Memory sanitizer is not supported")
    endif()
endmacro()

macro(add_sanitizer_checks)
    set(known_checks
        address
        array-bounds
        bool
        bounds
        builtin
        enum
        float-divide-by-zero
        function
        integer-divide-by-zero
        null
        nonnull-attribute
        object-size
        pointer-compare             # Depends on 'address'
        pointer-overflow
        pointer-subtract            # Depends on 'address'
        return
        returns-nonnull-attribute
        shift
        shift-base
        shift-exponent
        signed-integer-overflow
        undefined
        unsigned-integer-overflow
        vla-bound
        vptr
        )

    # Only check for leak sanitizer if not cross-compiling due to qemu crash
    if(NOT CMAKE_CROSSCOMPILING_EMULATOR)
        list(APPEND known_checks leak)
    endif()
    # Only check for alignment sanitizer flag if unaligned access is not supported
    if(NOT UNALIGNED_OK)
        list(APPEND known_checks alignment)
    endif()

    check_sanitizer_support("${known_checks}" supported_checks)

    if(NOT ${supported_checks} STREQUAL "")
        message(STATUS "Supported sanitizers checks are enabled: ${supported_checks}")
        set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} -fsanitize=${supported_checks}")

        # Group sanitizer flag -fsanitize=undefined will automatically add alignment, even if
        # it is not in our sanitize flag list, so we need to explicitly disable alignment sanitizing.
        if(UNALIGNED_OK)
            set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} -fno-sanitize=alignment")
        endif()
    else()
        message(STATUS "Sanitizer checks are not supported")
    endif()
endmacro()