if(NOT OUTPUT)
    if(WIN32)
        set(OUTPUT NUL)
    else()
        set(OUTPUT /dev/null)
    endif()
endif()
if(INPUT)
    execute_process(COMMAND ${COMMAND}
        RESULT_VARIABLE CMD_RESULT
        INPUT_FILE ${INPUT}
        OUTPUT_FILE ${OUTPUT})
else()
    execute_process(COMMAND ${COMMAND}
        RESULT_VARIABLE CMD_RESULT
        OUTPUT_FILE ${OUTPUT})
endif()
if(SUCCESS_EXIT)
    list(FIND SUCCESS_EXIT ${CMD_RESULT} _INDEX)
    if (${_INDEX} GREATER -1)
        set(CMD_RESULT 0)
    endif()
endif()
if(CMD_RESULT)
    message(FATAL_ERROR "${COMMAND} failed: ${CMD_RESULT}")
endif()
