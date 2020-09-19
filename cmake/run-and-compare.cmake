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
        -P ${CMAKE_CURRENT_SOURCE_DIR}/cmake/run-and-redirect.cmake
        RESULT_VARIABLE CMD_RESULT)
else()
    # Run command and redirect stdout to output
    execute_process(COMMAND ${CMAKE_COMMAND}
        "-DCOMMAND=${COMMAND}"
        -DOUTPUT=${OUTPUT}
        "-DSUCCESS_EXIT=${SUCCESS_EXIT}"
        -P ${CMAKE_CURRENT_SOURCE_DIR}/cmake/run-and-redirect.cmake
        RESULT_VARIABLE CMD_RESULT)
endif()

if(CMD_RESULT)
    message(FATAL_ERROR "Run before compare failed: ${CMD_RESULT}")
endif()

# Compare that output is equal to specified file
execute_process(COMMAND ${CMAKE_COMMAND}
    -E compare_files ${COMPARE} ${OUTPUT}
    RESULT_VARIABLE CMD_RESULT)

if(CMD_RESULT)
    message(FATAL_ERROR "Run compare failed: ${CMD_RESULT}")
endif()