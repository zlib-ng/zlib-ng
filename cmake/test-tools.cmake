# test-tools.cmake -- Tests targeting tool coverage

# Test --help and invalid parameters for our tools
set(TEST_COMMAND ${MINIGZIP_COMMAND} "--help")
add_test(NAME minigzip-help
    COMMAND ${CMAKE_COMMAND}
    "-DCOMMAND=${TEST_COMMAND}"
    -P ${CMAKE_CURRENT_SOURCE_DIR}/cmake/run-and-redirect.cmake)

set(TEST_COMMAND ${MINIGZIP_COMMAND} "--invalid")
add_test(NAME minigzip-invalid
    COMMAND ${CMAKE_COMMAND}
    "-DCOMMAND=${TEST_COMMAND}"
    -DSUCCESS_EXIT=64
    -P ${CMAKE_CURRENT_SOURCE_DIR}/cmake/run-and-redirect.cmake)

set(TEST_COMMAND ${MINIDEFLATE_COMMAND} "--help")
add_test(NAME minideflate-help
    COMMAND ${CMAKE_COMMAND}
     "-DCOMMAND=${TEST_COMMAND}"
     -P ${CMAKE_CURRENT_SOURCE_DIR}/cmake/run-and-redirect.cmake)

set(TEST_COMMAND ${MINIDEFLATE_COMMAND} "--invalid")
add_test(NAME minideflate-invalid
    COMMAND ${CMAKE_COMMAND}
    "-DCOMMAND=${TEST_COMMAND}"
    -DSUCCESS_EXIT=64
    -P ${CMAKE_CURRENT_SOURCE_DIR}/cmake/run-and-redirect.cmake)

set(TEST_COMMAND ${SWITCHLEVELS_COMMAND} "--help")
add_test(NAME switchlevels-help
    COMMAND ${CMAKE_COMMAND}
     "-DCOMMAND=${TEST_COMMAND}"
     -P ${CMAKE_CURRENT_SOURCE_DIR}/cmake/run-and-redirect.cmake)
