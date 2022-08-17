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

# Test generated crc32 tables match tables in source directory
add_test(NAME makecrct
    COMMAND ${CMAKE_COMMAND}
    "-DCOMMAND=${MAKECRCT_COMMAND}"
    -DOUTPUT=${PROJECT_BINARY_DIR}/Testing/Temporary/crc32_braid_tbl._h
    -DCOMPARE=${PROJECT_SOURCE_DIR}/crc32_braid_tbl.h
    -DIGNORE_LINE_ENDINGS=ON
    -P ${CMAKE_CURRENT_SOURCE_DIR}/cmake/run-and-compare.cmake)

# Test generated inflate tables match tables in source directory
add_test(NAME makefixed
    COMMAND ${CMAKE_COMMAND}
    "-DCOMMAND=${MAKEFIXED_COMMAND}"
    -DOUTPUT=${PROJECT_BINARY_DIR}/Testing/Temporary/inffixed_tbl._h
    -DCOMPARE=${PROJECT_SOURCE_DIR}/inffixed_tbl.h
    -DIGNORE_LINE_ENDINGS=ON
    -P ${CMAKE_CURRENT_SOURCE_DIR}/cmake/run-and-compare.cmake)

# Test generated tree tables match tables in source directory
add_test(NAME maketrees
    COMMAND ${CMAKE_COMMAND}
    "-DCOMMAND=${MAKETREES_COMMAND}"
    -DOUTPUT=${PROJECT_BINARY_DIR}/Testing/Temporary/trees_tbl._h
    -DCOMPARE=${PROJECT_SOURCE_DIR}/trees_tbl.h
    -DIGNORE_LINE_ENDINGS=ON
    -P ${CMAKE_CURRENT_SOURCE_DIR}/cmake/run-and-compare.cmake)
