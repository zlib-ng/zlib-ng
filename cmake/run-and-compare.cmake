if(NOT OUTPUT OR NOT COMPARE OR NOT COMMAND)
    message(FATAL_ERROR "Run and compare arguments missing")
endif()

if(INPUT)
    # Run command with stdin input and redirect stdout to output
    execute_process(COMMAND ${CMAKE_COMMAND}
        "-DCOMMAND=${COMMAND}"
        -DINPUT=${INPUT}
        -DOUTPUT=${OUTPUT}
        "-DSUCCESS_EXIT=${SUCCESS_EXIT}"
        -P ${CMAKE_CURRENT_SOURCE_DIR}/cmake/run-and-redirect.cmake)
else()
    # Run command and redirect stdout to output
    execute_process(COMMAND ${CMAKE_COMMAND}
        "-DCOMMAND=${COMMAND}"
        -DOUTPUT=${OUTPUT}
        "-DSUCCESS_EXIT=${SUCCESS_EXIT}"
        -P ${CMAKE_CURRENT_SOURCE_DIR}/cmake/run-and-redirect.cmake)
endif()

# Compare that output is equal to specified file
execute_process(COMMAND ${CMAKE_COMMAND}
        -E compare_files ${COMPARE} ${OUTPUT})