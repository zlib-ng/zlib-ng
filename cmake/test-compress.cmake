if(TARGET)
    set(COMPRESS_TARGET ${TARGET})
    set(DECOMPRESS_TARGET ${TARGET})
endif()

if(NOT INPUT OR NOT COMPRESS_TARGET OR NOT DECOMPRESS_TARGET)
    message(FATAL_ERROR "Compress test arguments missing")
endif()

# Set default values
if(NOT COMPRESS_ARGS)
    set(COMPRESS_ARGS -c -k)
endif()
if(NOT DECOMPRESS_ARGS)
    set(DECOMPRESS_ARGS -d -c)
endif()
if(NOT GZIP_VERIFY)
    set(GZIP_VERIFY ON)
endif()
if(NOT SUCCESS_EXIT)
    set(SUCCESS_EXIT 0)
endif()

# Generate unique output path so multiple tests can be executed at the same time
if(NOT OUTPUT)
    string(RANDOM UNIQUE_ID)
    set(OUTPUT ${INPUT}-${UNIQUE_ID})
endif()
string(REPLACE ".gz" "" OUTPUT "${OUTPUT}")

# Compress input file
set(COMPRESS_COMMAND ${COMPRESS_TARGET} ${COMPRESS_ARGS})

execute_process(COMMAND ${CMAKE_COMMAND}
    "-DCOMMAND=${COMPRESS_COMMAND}"
    -DINPUT=${INPUT}
    -DOUTPUT=${OUTPUT}.gz
    "-DSUCCESS_EXIT=${SUCCESS_EXIT}"
    -P ${CMAKE_CURRENT_SOURCE_DIR}/cmake/run-and-redirect.cmake)

# Decompress output
set(DECOMPRESS_COMMAND ${DECOMPRESS_TARGET} ${DECOMPRESS_ARGS})

execute_process(COMMAND ${CMAKE_COMMAND}
    "-DCOMMAND=${DECOMPRESS_COMMAND}"
    -DINPUT=${OUTPUT}.gz
    -DOUTPUT=${OUTPUT}.out
    "-DSUCCESS_EXIT=${SUCCESS_EXIT}"
    -P ${CMAKE_CURRENT_SOURCE_DIR}/cmake/run-and-redirect.cmake)

# Compare decompressed output with original input file
execute_process(COMMAND ${CMAKE_COMMAND}
    -E compare_files ${INPUT} ${OUTPUT}.out)

if(GZIP_VERIFY AND NOT "${COMPRESS_ARGS}" MATCHES "-T")
    # Transparent writing does not use gzip format
    find_program(GZIP gzip)
    if(GZIP)
        # Check gzip can decompress our compressed output
        set(GZ_DECOMPRESS_COMMAND ${GZIP} --decompress)

        execute_process(COMMAND ${CMAKE_COMMAND}
            "-DCOMMAND=${GZ_DECOMPRESS_COMMAND}"
            -DINPUT=${OUTPUT}.gz
            -DOUTPUT=${OUTPUT}.gzip.out
            "-DSUCCESS_EXIT=${SUCCESS_EXIT}"
            -P ${CMAKE_CURRENT_SOURCE_DIR}/cmake/run-and-redirect.cmake)

        # Compare gzip output with original input file
        execute_process(COMMAND ${CMAKE_COMMAND}
            -E compare_files ${INPUT} ${OUTPUT}.gzip.out)

        # Compress input file with gzip
        set(GZ_COMPRESS_COMMAND ${GZIP} --stdout)

        execute_process(COMMAND ${CMAKE_COMMAND}
            "-DCOMMAND=${GZ_COMPRESS_COMMAND}"
            -DINPUT=${INPUT}
            -DOUTPUT=${OUTPUT}.gzip.gz
            "-DSUCCESS_EXIT=${SUCCESS_EXIT}"
            -P ${CMAKE_CURRENT_SOURCE_DIR}/cmake/run-and-redirect.cmake)

        # Check minigzip can decompress gzip compressed output
        execute_process(COMMAND ${CMAKE_COMMAND}
            "-DCOMMAND=${DECOMPRESS_COMMAND}"
            -DINPUT=${OUTPUT}.gzip.gz
            -DOUTPUT=${OUTPUT}.gzip.out
            "-DSUCCESS_EXIT=${SUCCESS_EXIT}"
            -P ${CMAKE_CURRENT_SOURCE_DIR}/cmake/run-and-redirect.cmake)

        # Compare original input file with gzip decompressed output
        execute_process(COMMAND ${CMAKE_COMMAND}
            -E compare_files ${INPUT} ${OUTPUT}.gzip.out)

        # Cleanup temporary files
        file(REMOVE ${OUTPUT}.gzip.gz ${OUTPUT}.gzip.out)
    endif()
endif()

# Cleanup temporary files
file(REMOVE ${OUTPUT}.gz ${OUTPUT}.out)
