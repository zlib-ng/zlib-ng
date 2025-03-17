# test-detect.cmake - Tests automatic data type detection

# Test compress and verify test against data file using extra args
macro(test_minigzip name path)
    # Construct compression arguments for minigzip
    set(compress_args -k -c)
    foreach(extra_arg IN ITEMS "${ARGN}")
        list(APPEND compress_args ${extra_arg})
    endforeach()

    # Create unique friendly string for test
    string(REPLACE ";" "" arg_list "${ARGN}")
    string(REPLACE " " "" arg_list "${arg_list}")
    string(REPLACE "-" "" arg_list "${arg_list}")

    set(test_id minigzip-${name}-${arg_list})

    if(NOT TEST ${test_id})
        add_test(NAME ${test_id}
            COMMAND ${CMAKE_COMMAND}
            "-DTARGET=${MINIGZIP_COMMAND}"
            "-DCOMPRESS_ARGS=${compress_args}"
            "-DDECOMPRESS_ARGS=-d;-c"
            -DINPUT=${CMAKE_CURRENT_SOURCE_DIR}/${path}
            -DTEST_NAME=${test_id}
            -P ${CMAKE_CURRENT_SOURCE_DIR}/cmake/compress-and-verify.cmake)
    endif()
endmacro()

# Tests to verify with automatic data type detection arg
test_minigzip("detect-text" "data/lcet10.txt" -A)
test_minigzip("detect-binary" "data/paper-100k.pdf" -A)
