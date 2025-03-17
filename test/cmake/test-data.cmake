# test-data.cmake - Tests targeting data files in the data directory

# Test compress and verify test against data file using extra args
macro(parameterized_tester name path)
    set(test_id parameterized-${name})

    if(NOT TEST ${test_id})
        add_test(NAME ${test_id}
            COMMAND ${PARAMETERIZED_TESTER_COMMAND}
            "${CMAKE_CURRENT_SOURCE_DIR}/${path}")
    endif()
endmacro()

# Enumerate all files in data directory to run tests against
file(GLOB_RECURSE TEST_FILE_PATHS
    LIST_DIRECTORIES false
    RELATIVE ${CMAKE_CURRENT_SOURCE_DIR}
    ${CMAKE_CURRENT_SOURCE_DIR}/data/*)

# For all files in the data directory, run tests against them
foreach(test_file_path ${TEST_FILE_PATHS})
    if("${test_file_path}" MATCHES ".gz$" OR "${test_file_path}" MATCHES ".out$" OR
        "${test_file_path}" MATCHES "/.git/" OR "${test_file_path}" MATCHES ".md$")
        continue()
    endif()
    get_filename_component(test_name ${test_file_path} NAME)
    if (test_name STREQUAL "")
        continue()
    endif()
    parameterized_tester(${test_name} ${test_file_path})
endforeach()
