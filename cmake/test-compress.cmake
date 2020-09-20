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

# Generate unique output path so multiple tests can be executed at the same time
if(NOT OUTPUT)
    # Output name based on input and unique id
    string(RANDOM UNIQUE_ID)
    set(OUTPUT ${INPUT}-${UNIQUE_ID})
else()
    # Output name appends unique id in case multiple tests with same output name
    string(RANDOM LENGTH 6 UNIQUE_ID)
    set(OUTPUT ${OUTPUT}-${UNIQUE_ID})
endif()
string(REPLACE ".gz" "" OUTPUT "${OUTPUT}")

macro(cleanup)
    # Cleanup temporary mingizip files
    file(REMOVE ${OUTPUT}.gz ${OUTPUT}.out)
    # Cleanup temporary gzip files
    file(REMOVE ${OUTPUT}.gzip.gz ${OUTPUT}.gzip.out)
endmacro()

# Compress input file
if(NOT EXISTS ${INPUT})
    message(FATAL_ERROR "Cannot find compress input: ${INPUT}")
endif()

set(COMPRESS_COMMAND ${COMPRESS_TARGET} ${COMPRESS_ARGS})

execute_process(COMMAND ${CMAKE_COMMAND}
    "-DCOMMAND=${COMPRESS_COMMAND}"
    -DINPUT=${INPUT}
    -DOUTPUT=${OUTPUT}.gz
    "-DSUCCESS_EXIT=${SUCCESS_EXIT}"
    -P ${CMAKE_CURRENT_LIST_DIR}/run-and-redirect.cmake
    RESULT_VARIABLE CMD_RESULT)

if(CMD_RESULT)
    cleanup()
    message(FATAL_ERROR "Compress failed: ${CMD_RESULT}")
endif()

# Decompress output
if(NOT EXISTS ${OUTPUT}.gz)
    cleanup()
    message(FATAL_ERROR "Cannot find decompress input: ${OUTPUT}.gz")
endif()

set(DECOMPRESS_COMMAND ${DECOMPRESS_TARGET} ${DECOMPRESS_ARGS})

execute_process(COMMAND ${CMAKE_COMMAND}
    "-DCOMMAND=${DECOMPRESS_COMMAND}"
    -DINPUT=${OUTPUT}.gz
    -DOUTPUT=${OUTPUT}.out
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
        -E compare_files ${INPUT} ${OUTPUT}.out
        RESULT_VARIABLE CMD_RESULT)

    if(CMD_RESULT)
        cleanup()
        message(FATAL_ERROR "Compare minigzip decompress failed: ${CMD_RESULT}")
    endif()
endif()

if(GZIP_VERIFY AND NOT "${COMPRESS_ARGS}" MATCHES "-T")
    # Transparent writing does not use gzip format
    find_program(GZIP gzip)
    if(GZIP)
        if(NOT EXISTS ${OUTPUT}.gz)
            cleanup()
            message(FATAL_ERROR "Cannot find gzip decompress input: ${OUTPUT}.gz")
        endif()

        # Check gzip can decompress our compressed output
        set(GZ_DECOMPRESS_COMMAND ${GZIP} --decompress)

        execute_process(COMMAND ${CMAKE_COMMAND}
            "-DCOMMAND=${GZ_DECOMPRESS_COMMAND}"
            -DINPUT=${OUTPUT}.gz
            -DOUTPUT=${OUTPUT}.gzip.out
            "-DSUCCESS_EXIT=${SUCCESS_EXIT}"
            -P ${CMAKE_CURRENT_LIST_DIR}/run-and-redirect.cmake
            RESULT_VARIABLE CMD_RESULT)

        if(CMD_RESULT)
            cleanup()
            message(FATAL_ERROR "Gzip decompress failed: ${CMD_RESULT}")
        endif()

        # Compare gzip output with original input file
        execute_process(COMMAND ${CMAKE_COMMAND}
            -E compare_files ${INPUT} ${OUTPUT}.gzip.out
            RESULT_VARIABLE CMD_RESULT)

        if(CMD_RESULT)
            cleanup()
            message(FATAL_ERROR "Compare gzip decompress failed: ${CMD_RESULT}")
        endif()

        if(NOT EXISTS ${OUTPUT}.gz)
            cleanup()
            message(FATAL_ERROR "Cannot find gzip compress input: ${INPUT}")
        endif()

        # Compress input file with gzip
        set(GZ_COMPRESS_COMMAND ${GZIP} --stdout)

        execute_process(COMMAND ${CMAKE_COMMAND}
            "-DCOMMAND=${GZ_COMPRESS_COMMAND}"
            -DINPUT=${INPUT}
            -DOUTPUT=${OUTPUT}.gzip.gz
            "-DSUCCESS_EXIT=${SUCCESS_EXIT}"
            -P ${CMAKE_CURRENT_LIST_DIR}/run-and-redirect.cmake
            RESULT_VARIABLE CMD_RESULT)

        if(CMD_RESULT)
            cleanup()
            message(FATAL_ERROR "Gzip compress failed: ${CMD_RESULT}")
        endif()

        if(NOT EXISTS ${OUTPUT}.gz)
            cleanup()
            message(FATAL_ERROR "Cannot find minigzip decompress input: ${OUTPUT}.gzip.gz")
        endif()

        # Check minigzip can decompress gzip compressed output
        execute_process(COMMAND ${CMAKE_COMMAND}
            "-DCOMMAND=${DECOMPRESS_COMMAND}"
            -DINPUT=${OUTPUT}.gzip.gz
            -DOUTPUT=${OUTPUT}.gzip.out
            "-DSUCCESS_EXIT=${SUCCESS_EXIT}"
            -P ${CMAKE_CURRENT_LIST_DIR}/run-and-redirect.cmake
            RESULT_VARIABLE CMD_RESULT)

        if(CMD_RESULT)
            cleanup()
            message(FATAL_ERROR "Minigzip decompress gzip failed: ${CMD_RESULT}")
        endif()

        if(COMPARE)
            # Compare original input file with gzip decompressed output
            execute_process(COMMAND ${CMAKE_COMMAND}
                -E compare_files ${INPUT} ${OUTPUT}.gzip.out
                RESULT_VARIABLE CMD_RESULT)

            if(CMD_RESULT)
                cleanup()
                message(FATAL_ERROR "Compare minigzip decompress gzip failed: ${CMD_RESULT}")
            endif()
        endif()
    endif()
endif()

cleanup()