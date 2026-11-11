# test-issues.cmake -- Tests targeting specific GitHub issues

add_test(NAME GH-751
    COMMAND ${CMAKE_COMMAND}
    "-DTARGET=${MINIGZIP_COMMAND}"
    -DTEST_NAME=GH-751-test-txt
    -DINPUT=${CMAKE_CURRENT_SOURCE_DIR}/GH-751/test.txt
    -P ${CMAKE_CURRENT_SOURCE_DIR}/cmake/compress-and-verify.cmake)
