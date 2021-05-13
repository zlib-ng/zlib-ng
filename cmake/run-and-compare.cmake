# run-and-compare.cmake -- Runs a command and compares its output to an expected value

# Copyright (C) 2021 Nathan Moinvaziri
# Licensed under the Zlib license, see LICENSE.md for details

# Required Variables
#   COMMAND             - Command to run
#   OUTPUT              - Standard output
#   COMPARE             - String to compare output against

# Optional Variables
#   INPUT               - Standard input
#   IGNORE_LINE_ENDINGS - Ignore line endings when comparing output

if(NOT DEFINED OUTPUT OR NOT DEFINED COMPARE OR NOT DEFINED COMMAND)
    message(FATAL_ERROR "Run and compare arguments missing")
endif()

if(INPUT)
    # Run command with stdin input and redirect stdout to output
    execute_process(COMMAND ${CMAKE_COMMAND}
        "-DCOMMAND=${COMMAND}"
        -DINPUT=${INPUT}
        -DOUTPUT=${OUTPUT}
        "-DSUCCESS_EXIT=${SUCCESS_EXIT}"
        -P ${CMAKE_CURRENT_LIST_DIR}/run-and-redirect.cmake
        RESULT_VARIABLE CMD_RESULT)
else()
    # Run command and redirect stdout to output
    execute_process(COMMAND ${CMAKE_COMMAND}
        "-DCOMMAND=${COMMAND}"
        -DOUTPUT=${OUTPUT}
        "-DSUCCESS_EXIT=${SUCCESS_EXIT}"
        -P ${CMAKE_CURRENT_LIST_DIR}/run-and-redirect.cmake
        RESULT_VARIABLE CMD_RESULT)
endif()

if(CMD_RESULT)
    message(FATAL_ERROR "Run before compare failed: ${CMD_RESULT}")
endif()

# Use configure_file to normalize line-endings
if(IGNORE_LINE_ENDINGS)
    configure_file(${COMPARE} ${COMPARE}.cmp NEWLINE_STYLE LF)
    set(COMPARE ${COMPARE}.cmp)
    configure_file(${OUTPUT} ${OUTPUT}.cmp NEWLINE_STYLE LF)
    set(OUTPUT ${OUTPUT}.cmp)
endif()

# Compare that output is equal to specified file
execute_process(COMMAND ${CMAKE_COMMAND}
    -E compare_files ${COMPARE} ${OUTPUT}
    RESULT_VARIABLE CMD_RESULT)

# Delete temporary files used to normalize line-endings
if(IGNORE_LINE_ENDINGS)
    file(REMOVE ${COMPARE} ${OUTPUT})
endif()

if(CMD_RESULT)
    message(FATAL_ERROR "Run compare failed: ${CMD_RESULT}")
endif()