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

macro(add_address_sanitizer)
    set(known_checks
        address
        pointer-compare
        pointer-subtract
        )

    check_sanitizer_support("${known_checks}" supported_checks)
    if(NOT ${supported_checks} STREQUAL "")
        message(STATUS "Address sanitizer is enabled: ${supported_checks}")
        set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} -fsanitize=${supported_checks}")
    else()
        message(STATUS "Address sanitizer is not supported")
    endif()

    if(CMAKE_CROSSCOMPILING_EMULATOR)
        # Only check for leak sanitizer if not cross-compiling due to qemu crash
        message(WARNING "Leak sanitizer is not supported when cross compiling")
    else()
        # Leak sanitizer requires address sanitizer
        check_sanitizer_support("leak" supported_checks)
        if(NOT ${supported_checks} STREQUAL "")
            message(STATUS "Leak sanitizer is enabled: ${supported_checks}")
            set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} -fsanitize=${supported_checks}")
        else()
            message(STATUS "Leak sanitizer is not supported")
        endif()
    endif()
endmacro()

macro(add_memory_sanitizer)
    check_sanitizer_support("memory" supported_checks)
    if(NOT ${supported_checks} STREQUAL "")
        message(STATUS "Memory sanitizer is enabled: ${supported_checks}")
        set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} -fsanitize=${supported_checks}")
    else()
        message(STATUS "Memory sanitizer is not supported")
    endif()
endmacro()

macro(add_thread_sanitizer)
    check_sanitizer_support("thread" supported_checks)
    if(NOT ${supported_checks} STREQUAL "")
        message(STATUS "Thread sanitizer is enabled: ${supported_checks}")
        set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} -fsanitize=${supported_checks}")
    else()
        message(STATUS "Thread sanitizer is not supported")
    endif()
endmacro()

macro(add_undefined_sanitizer)
    set(known_checks
        array-bounds
        bool
        bounds
        builtin
        enum
        float-cast-overflow
        float-divide-by-zero
        function
        integer-divide-by-zero
        local-bounds
        null
        nonnull-attribute
        pointer-overflow
        return
        returns-nonnull-attribute
        shift
        shift-base
        shift-exponent
        signed-integer-overflow
        undefined
        unsigned-integer-overflow
        unsigned-shift-base
        vla-bound
        vptr
        )

    # Only check for alignment sanitizer flag if unaligned access is not supported
    if(NOT UNALIGNED_OK)
        list(APPEND known_checks alignment)
    endif()
    # Object size sanitizer has no effect at -O0 and produces compiler warning if enabled
    if(NOT CMAKE_C_FLAGS MATCHES "-O0")
        list(APPEND known_checks object-size)
    endif()

    check_sanitizer_support("${known_checks}" supported_checks)

    if(NOT ${supported_checks} STREQUAL "")
        message(STATUS "Undefined behavior sanitizer is enabled: ${supported_checks}")
        set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} -fsanitize=${supported_checks}")

        # Group sanitizer flag -fsanitize=undefined will automatically add alignment, even if
        # it is not in our sanitize flag list, so we need to explicitly disable alignment sanitizing.
        if(UNALIGNED_OK)
            set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} -fno-sanitize=alignment")
        endif()
    else()
        message(STATUS "UNdefined behavior sanitizer is not supported")
    endif()
endmacro()
