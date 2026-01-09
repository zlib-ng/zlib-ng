# fallback-macros.cmake -- CMake fallback macros
# Copyright (C) 2026 Vladislav Shchapov
# Licensed under the Zlib license, see LICENSE.md for details

# Workaround for FetchContent EXCLUDE_FROM_ALL implementation for CMake before 3.28.
# The EXCLUDE_FROM_ALL argument for FetchContent_Declare added in version 3.28.
# Before CMake 3.28, FetchContent_MakeAvailable would add dependencies to the ALL target.
macro(ZNG_FetchContent_MakeAvailable)
    if(CMAKE_VERSION VERSION_LESS 3.28)
        foreach(__zng_contentName IN ITEMS ${ARGV})
            string(TOLOWER ${__zng_contentName} __zng_contentNameLower)
            FetchContent_GetProperties(${__zng_contentName})
            if(NOT ${__zng_contentNameLower}_POPULATED)
                FetchContent_Populate(${__zng_contentName})
                add_subdirectory(${${__zng_contentNameLower}_SOURCE_DIR} ${${__zng_contentNameLower}_BINARY_DIR} EXCLUDE_FROM_ALL)
            endif()
        endforeach()
        unset(__zng_contentName)
        unset(__zng_contentNameLower)
    else()
        FetchContent_MakeAvailable(${ARGV})
    endif()
endmacro()

if(CMAKE_VERSION VERSION_LESS 3.28)
    set(ZNG_FetchContent_Declare_EXCLUDE_FROM_ALL)
else()
    set(ZNG_FetchContent_Declare_EXCLUDE_FROM_ALL EXCLUDE_FROM_ALL)
endif()
