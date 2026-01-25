# Export configurable variables for the try_compile() command.
set(CMAKE_TRY_COMPILE_PLATFORM_VARIABLES MCST_LCC_PREFIX QEMU_CPU)

set(CMAKE_SYSTEM_NAME Linux)
set(CMAKE_SYSTEM_PROCESSOR e2k)
set(CMAKE_SYSTEM_VERSION 1)

set(CMAKE_C_COMPILER_TARGET e2k-linux-gnu)
set(CMAKE_CXX_COMPILER_TARGET e2k-linux-gnu)

set(CMAKE_CROSSCOMPILING TRUE)

# Require explicit toolchain prefix
if(NOT MCST_LCC_PREFIX)
    message(FATAL_ERROR "MCST_LCC_PREFIX must be set to the LCC install prefix (e.g. /opt/mcst/lcc-<ver>)")
endif()
if(NOT EXISTS "${MCST_LCC_PREFIX}/bin")
    message(FATAL_ERROR "MCST_LCC_PREFIX does not contain a bin directory: ${MCST_LCC_PREFIX}")
endif()
if(NOT EXISTS "${MCST_LCC_PREFIX}/fs")
    message(FATAL_ERROR "MCST_LCC_PREFIX does not contain a fs directory: ${MCST_LCC_PREFIX}")
endif()

unset(QEMU_CPU_ARG)
if(QEMU_CPU)
    set(QEMU_CPU_ARG -cpu "${QEMU_CPU}")
endif()
set(CMAKE_CROSSCOMPILING_EMULATOR qemu-e2k-static ${QEMU_CPU_ARG} -L "${MCST_LCC_PREFIX}/fs/")

set(CMAKE_FIND_ROOT_PATH_MODE_PROGRAM NEVER)
set(CMAKE_FIND_ROOT_PATH_MODE_LIBRARY ONLY)
set(CMAKE_FIND_ROOT_PATH_MODE_INCLUDE ONLY)
set(CMAKE_FIND_ROOT_PATH_MODE_PACKAGE ONLY)

find_program(C_COMPILER_FULL_PATH NAMES lcc PATHS "${MCST_LCC_PREFIX}/bin" NO_DEFAULT_PATH)
if(NOT C_COMPILER_FULL_PATH)
    message(FATAL_ERROR "Cross-compiler for ${CMAKE_C_COMPILER_TARGET} not found")
endif()
set(CMAKE_C_COMPILER ${C_COMPILER_FULL_PATH})

find_program(CXX_COMPILER_FULL_PATH NAMES l++ PATHS "${MCST_LCC_PREFIX}/bin" NO_DEFAULT_PATH)
if(CXX_COMPILER_FULL_PATH)
    set(CMAKE_CXX_COMPILER ${CXX_COMPILER_FULL_PATH})
endif()
