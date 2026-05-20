# check-compiler-flags.cmake -- Check/validate compiler flags
# Licensed under the Zlib license, see LICENSE.md for details

# Extract machine flags from multi-flag command line argument.
function(extract_machine_flags out_var flags_str)
    string(REGEX MATCHALL
        "-(m(arch|cpu|tune|fpu|float-abi|fp16-format|abi)|q(arch|tune)|tp|target)=[^ \t]+|/arch:[^ \t]+"
        matches "${flags_str}")
    string(REPLACE "\"" "" matches "${matches}")
    string(REPLACE "'"  "" matches "${matches}")

    list(SORT matches)
    set(${out_var} "${matches}" PARENT_SCOPE)
endfunction()

# Error on asymmetric C vs C++ machine flags.
function(check_machine_flag_symmetry)
    extract_machine_flags(m_flag_c   "${CMAKE_C_FLAGS}")
    extract_machine_flags(m_flag_cxx "${CMAKE_CXX_FLAGS}")

    if(NOT "${m_flag_c}" STREQUAL "${m_flag_cxx}")
        message(FATAL_ERROR
            "C and C++ machine flags differ:\n"
            "    CMAKE_C_FLAGS:   ${m_flag_c}\n"
            "    CMAKE_CXX_FLAGS: ${m_flag_cxx}\n"
            "Pass the same machine flags to both, or to neither.")
    endif()
endfunction()
