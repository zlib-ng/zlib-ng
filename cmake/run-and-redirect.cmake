if(WIN32)
    set(DEVNULL NUL)
else()
    set(DEVNULL /dev/null)
endif()
execute_process(COMMAND ${COMMAND}
    RESULT_VARIABLE CMD_RESULT
    INPUT_FILE ${INPUT}
    OUTPUT_FILE ${DEVNULL})
if(SUCCESS_EXIT)
    list(FIND SUCCESS_EXIT ${CMD_RESULT} _INDEX)
    if (${_INDEX} GREATER -1)
        set(CMD_RESULT 0)
    endif()
endif()
if(CMD_RESULT)
    message(FATAL_ERROR "${COMMAND} failed: ${CMD_RESULT}")
endif()
