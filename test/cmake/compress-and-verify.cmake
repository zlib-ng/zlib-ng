# compress-and-verify.cmake -- Runs a test against an input file to make sure that the specified
#   targets are able to to compress and then decompress successfully. Optionally verify
#   the results with gzip. Output files are generated with unique names to prevent parallel
#   tests from corrupting one another. Default target arguments are compatible with minigzip.

# Copyright (C) 2021 Nathan Moinvaziri
# Licensed under the Zlib license, see LICENSE.md for details

# that test a specific input file for compression or decompression.

# Required Variables
#   INPUT                   - Input file to test
#   TARGET or               - Command to run for both compress and decompress
#     COMPRESS_TARGET and   - Command to run to compress input file
#     DECOMPRESS_TARGET     - Command to run to decompress output file

# Optional Variables
#   TEST_NAME               - Name of test to use when constructing output file paths
#   COMPRESS_ARGS           - Arguments to pass for compress command (default: -c -k)
#   DECOMPRESS_ARGS         - Arguments to pass to decompress command (default: -d -c)

#   GZIP_VERIFY             - Verify that gzip can decompress the COMPRESS_TARGET output and
#                             verify that DECOMPRESS_TARGET can decompress gzip output of INPUT
#   COMPARE                 - Verify decompressed output is the same as input
#   SUCCESS_EXIT            - List of successful exit codes (default: 0, ie: 0;1)

if(TARGET)
    set(COMPRESS_TARGET ${TARGET})
    set(DECOMPRESS_TARGET ${TARGET})
endif()

if(NOT DEFINED INPUT OR NOT DEFINED COMPRESS_TARGET OR NOT DEFINED DECOMPRESS_TARGET)
    message(FATAL_ERROR "Compress test arguments missing")
endif()

# Set default values
if(NOT DEFINED COMPARE)
    set(COMPARE ON)
endif()
if(NOT DEFINED COMPRESS_ARGS)
    set(COMPRESS_ARGS -c -k)
endif()
if(NOT DEFINED DECOMPRESS_ARGS)
    set(DECOMPRESS_ARGS -d -c)
endif()
if(NOT DEFINED GZIP_VERIFY)
    set(GZIP_VERIFY ON)
endif()
if(NOT DEFINED SUCCESS_EXIT)
    set(SUCCESS_EXIT 0)
endif()

# Use test name from input file name
if(NOT DEFINED TEST_NAME)
    get_filename_component(TEST_NAME "${INPUT}" NAME)
endif()

# Generate unique output path so multiple tests can be executed at the same time
string(RANDOM LENGTH 6 UNIQUE_ID)
string(REPLACE "." "-" TEST_NAME "${TEST_NAME}")
set(OUTPUT_BASE "${CMAKE_CURRENT_BINARY_DIR}/Testing/Temporary/${TEST_NAME}-${UNIQUE_ID}")

# Ensure directory exists for output files
get_filename_component(OUTPUT_DIR "${OUTPUT_BASE}" DIRECTORY)
file(MAKE_DIRECTORY "${OUTPUT_DIR}")

# Cleanup temporary files
macro(cleanup_always)
    file(GLOB TEMP_FILES ${OUTPUT_BASE}*)
    file(REMOVE ${TEMP_FILES})
endmacro()
# Clean up temporary files if not on CI
macro(cleanup)
    if(NOT DEFINED ENV{CI})
        cleanup_always()
    endif()
endmacro()

# Show differences between two files
macro(diff src1 src2)
    find_program(XXD xxd)
    if(XXD)
        find_program(DIFF diff)
        if(DIFF)
            set(XXD_COMMAND ${XXD} ${src1} ${src1}.hex)
            execute_process(COMMAND ${XXD_COMMAND})
            set(XXD_COMMAND ${XXD} ${src2} ${src2}.hex)
            execute_process(COMMAND ${XXD_COMMAND})

            set(DIFF_COMMAND ${DIFF} -u ${src1}.hex ${src2}.hex)
            execute_process(COMMAND ${DIFF_COMMAND}
                OUTPUT_FILE ${src2}.diff)

            file(READ ${src2}.diff DIFF_OUTPUT)
            message(STATUS "Diff:\n${DIFF_OUTPUT}")

            if(NOT DEFINED ENV{CI})
                file(REMOVE ${src1}.hex ${src2}.hex ${src2}.diff)
            endif()
        endif()
    endif()
endmacro()

# Compress input file
if(NOT EXISTS ${INPUT})
    message(FATAL_ERROR "Cannot find compress input: ${INPUT}")
endif()

set(COMPRESS_COMMAND ${COMPRESS_TARGET} ${COMPRESS_ARGS})

message(STATUS "Compress ${COMPRESS_COMMAND}")
message(STATUS "  Input: ${INPUT}")
message(STATUS "  Output: ${OUTPUT_BASE}.gz")

execute_process(COMMAND ${CMAKE_COMMAND}
    "-DCOMMAND=${COMPRESS_COMMAND}"
    -DINPUT=${INPUT}
    -DOUTPUT=${OUTPUT_BASE}.gz
    "-DSUCCESS_EXIT=${SUCCESS_EXIT}"
    -P ${CMAKE_CURRENT_LIST_DIR}/run-and-redirect.cmake
    RESULT_VARIABLE CMD_RESULT)

if(CMD_RESULT)
    cleanup()
    message(FATAL_ERROR "Compress failed: ${CMD_RESULT}")
endif()

# Decompress output
if(NOT EXISTS ${OUTPUT_BASE}.gz)
    cleanup()
    message(FATAL_ERROR "Cannot find decompress input: ${OUTPUT_BASE}.gz")
endif()

set(DECOMPRESS_COMMAND ${DECOMPRESS_TARGET} ${DECOMPRESS_ARGS})

message(STATUS "Decompress ${DECOMPRESS_COMMAND}")
message(STATUS "  Input: ${OUTPUT_BASE}.gz")
message(STATUS "  Output: ${OUTPUT_BASE}")

execute_process(COMMAND ${CMAKE_COMMAND}
    "-DCOMMAND=${DECOMPRESS_COMMAND}"
    -DINPUT=${OUTPUT_BASE}.gz
    -DOUTPUT=${OUTPUT_BASE}
    "-DSUCCESS_EXIT=${SUCCESS_EXIT}"
    -P ${CMAKE_CURRENT_LIST_DIR}/run-and-redirect.cmake
    RESULT_VARIABLE CMD_RESULT)

if(CMD_RESULT)
    cleanup()
    message(FATAL_ERROR "Decompress failed: ${CMD_RESULT}")
endif()

if(COMPARE)
    # Compare decompressed output with original input file
    execute_process(COMMAND ${CMAKE_COMMAND}
        -E compare_files ${INPUT} ${OUTPUT_BASE}
        RESULT_VARIABLE CMD_RESULT)

    if(CMD_RESULT)
        diff(${INPUT} ${OUTPUT_BASE})
        cleanup()
        message(FATAL_ERROR "Compare decompress failed: ${CMD_RESULT}")
    endif()
endif()

if(GZIP_VERIFY AND NOT "${COMPRESS_ARGS}" MATCHES "-T")
    # Transparent writing does not use gzip format
    find_program(GZIP gzip)
    if(GZIP)
        if(NOT EXISTS ${OUTPUT_BASE}.gz)
            cleanup()
            message(FATAL_ERROR "Cannot find gzip decompress input: ${OUTPUT_BASE}.gz")
        endif()

        # Check gzip can decompress our compressed output
        set(GZ_DECOMPRESS_COMMAND ${GZIP} --decompress)

        message(STATUS "Gzip decompress ${GZ_DECOMPRESS_COMMAND}")
        message(STATUS "  Input: ${OUTPUT_BASE}.gz")
        message(STATUS "  Output: ${OUTPUT_BASE}-ungzip")

        execute_process(COMMAND ${CMAKE_COMMAND}
            "-DCOMMAND=${GZ_DECOMPRESS_COMMAND}"
            -DINPUT=${OUTPUT_BASE}.gz
            -DOUTPUT=${OUTPUT_BASE}-ungzip
            "-DSUCCESS_EXIT=${SUCCESS_EXIT}"
            -P ${CMAKE_CURRENT_LIST_DIR}/run-and-redirect.cmake
            RESULT_VARIABLE CMD_RESULT)

        if(CMD_RESULT)
            cleanup()
            message(FATAL_ERROR "Gzip decompress failed: ${CMD_RESULT}")
        endif()

        # Compare gzip output with original input file
        execute_process(COMMAND ${CMAKE_COMMAND}
            -E compare_files ${INPUT} ${OUTPUT_BASE}-ungzip
            RESULT_VARIABLE CMD_RESULT)

        if(CMD_RESULT)
            diff(${INPUT} ${OUTPUT_BASE}-ungzip)
            cleanup()
            message(FATAL_ERROR "Compare gzip decompress failed: ${CMD_RESULT}")
        endif()

        # Compress input file with gzip
        set(GZ_COMPRESS_COMMAND ${GZIP} --stdout)

        message(STATUS "Gzip compress ${GZ_COMPRESS_COMMAND}")
        message(STATUS "  Input: ${INPUT}")
        message(STATUS "  Output: ${OUTPUT_BASE}-gzip.gz")

        execute_process(COMMAND ${CMAKE_COMMAND}
            "-DCOMMAND=${GZ_COMPRESS_COMMAND}"
            -DINPUT=${INPUT}
            -DOUTPUT=${OUTPUT_BASE}-gzip.gz
            "-DSUCCESS_EXIT=${SUCCESS_EXIT}"
            -P ${CMAKE_CURRENT_LIST_DIR}/run-and-redirect.cmake
            RESULT_VARIABLE CMD_RESULT)

        if(CMD_RESULT)
            cleanup()
            message(FATAL_ERROR "Gzip compress failed: ${CMD_RESULT}")
        endif()

        if(NOT EXISTS ${OUTPUT_BASE}-gzip.gz)
            cleanup()
            message(FATAL_ERROR "Cannot find decompress gzip input: ${OUTPUT_BASE}-gzip.gz")
        endif()

        message(STATUS "Decompress gzip ${DECOMPRESS_COMMAND}")
        message(STATUS "  Input: ${OUTPUT_BASE}-gzip.gz")
        message(STATUS "  Output: ${OUTPUT_BASE}-gzip")

        # Check decompress target can handle gzip compressed output
        execute_process(COMMAND ${CMAKE_COMMAND}
            "-DCOMMAND=${DECOMPRESS_COMMAND}"
            -DINPUT=${OUTPUT_BASE}-gzip.gz
            -DOUTPUT=${OUTPUT_BASE}-gzip
            "-DSUCCESS_EXIT=${SUCCESS_EXIT}"
            -P ${CMAKE_CURRENT_LIST_DIR}/run-and-redirect.cmake
            RESULT_VARIABLE CMD_RESULT)

        if(CMD_RESULT)
            cleanup()
            message(FATAL_ERROR "Decompress gzip failed: ${CMD_RESULT}")
        endif()

        if(COMPARE)
            # Compare original input file with gzip decompressed output
            execute_process(COMMAND ${CMAKE_COMMAND}
                -E compare_files ${INPUT} ${OUTPUT_BASE}-gzip
                RESULT_VARIABLE CMD_RESULT)

            if(CMD_RESULT)
                diff(${INPUT} ${OUTPUT_BASE}-gzip)
                cleanup()
                message(FATAL_ERROR "Compare decompress gzip failed: ${CMD_RESULT}")
            endif()
        endif()
    endif()
endif()

cleanup_always()
