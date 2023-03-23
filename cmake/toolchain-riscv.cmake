# Copyright (c) 2022 SiFive, Inc. -- Proprietary and Confidential All Rights
# Reserved.
#
# NOTICE: All information contained herein is, and remains the property of
# SiFive, Inc. The intellectual and technical concepts contained herein are
# proprietary to SiFive, Inc. and may be covered by U.S. and Foreign Patents,
# patents in process, and are protected by trade secret or copyright law.
#
# This work may not be copied, modified, re-published, uploaded, executed, or
# distributed in any way, in any medium, whether in whole or in part, without
# prior written permission from SiFive, Inc.
#
# The copyright notice above does not evidence any actual or intended
# publication or disclosure of this source code, which includes information
# that is confidential and/or proprietary, and is a trade secret, of SiFive,
# Inc.
#===----------------------------------------------------------------------===//

set(CMAKE_SYSTEM_PROCESSOR riscv64)

if(CMAKE_HOST_SYSTEM_NAME STREQUAL Linux)
  set(HOST_TAG linux)
elseif(CMAKE_HOST_SYSTEM_NAME STREQUAL Darwin)
  set(HOST_TAG darwin)
endif()

# Don't search host side's library for find_*().
set(CMAKE_FIND_USE_CMAKE_SYSTEM_PATH FALSE)

set(CMAKE_SYSTEM_NAME "Linux")

set(RISCV_LINUX_TOOLCHAIN_PATH_PREFIX "" CACHE STRING "RISCV toolchain path")

if(RISCV_LINUX_TOOLCHAIN_PATH_PREFIX STREQUAL "")
  set(RISCV_TOOLCHAIN_ROOT "${CMAKE_SOURCE_DIR}/prebuilt/toolchain/clang/${HOST_TAG}/riscv_linux" CACHE PATH "RISC-V compiler path")
else()
  set(RISCV_TOOLCHAIN_ROOT $ENV{RISCV_LINUX_TOOLCHAIN_PATH_PREFIX})
endif()


if(DEFINED ENV{CC} AND DEFINED ENV{CXX} AND DEFINED ENV{AR} AND DEFINED ENV{RANLIB})
  set(USE_ENV_COMPILER ON)
else()
  set(USE_ENV_COMPILER OFF)
endif()

if(NOT ${USE_ENV_COMPILER})
# If prebuilt toolchain does not exist, use the toolchain in path.
  set(RISCV_TOOLCHAIN_NAME_PREFIX riscv64-unknown-linux-gnu)
  if (NOT EXISTS ${RISCV_TOOLCHAIN_ROOT})
    set(RISCV_TOOLCHAIN_PREFIX "${RISCV_TOOLCHAIN_NAME_PREFIX}")
  else()
    set(RISCV_TOOLCHAIN_PREFIX "${RISCV_TOOLCHAIN_ROOT}/bin/${RISCV_TOOLCHAIN_NAME_PREFIX}")
  endif()

  set(CMAKE_C_COMPILER "${RISCV_TOOLCHAIN_PREFIX}-clang")
  set(CMAKE_CXX_COMPILER "${RISCV_TOOLCHAIN_PREFIX}-clang++")
# Some binutils tools haven't had the *-llvm-* version.
  set(CMAKE_AR "${RISCV_TOOLCHAIN_PREFIX}-llvm-ar")
  set(CMAKE_RANLIB "${RISCV_TOOLCHAIN_PREFIX}-llvm-ranlib")
endif()

# riscv platfrom setting
set(XLEN "64" CACHE STRING "xlen")
set(VLEN "512" CACHE STRING "vlen")

set(COMPILE_CMODEL "medany" CACHE STRING "CMODEL")
set(COMPILE_SPEC "" CACHE STRING "SPEC")

# compile options
add_compile_options(-mcmodel=${COMPILE_CMODEL})
if(COMPILE_SPEC)
  add_compile_options(--specs=${COMPILE_SPEC}.specs)
endif()

set(CPU_TARGET "sifive-x280n" CACHE STRING "SiFive default CPU target.")
add_compile_options(-mcpu=${CPU_TARGET} -mabi=lp64d)
add_compile_options(-msifive-recode=neon)
# Use fused multiply-add operation.
add_compile_options(-ffp-contract=fast)
add_compile_options(-fdata-sections -ffunction-sections)
add_compile_options(-fdiagnostics-color=always)
# FIXME: (Alex) We would remove this option when the toolchain update. Use it temporarily.
# We have problematic stack unwinding for c++ exception.
add_compile_options(-fno-omit-frame-pointer)
# FIXME: (Jerry) We hits some bugs for auto-vectorization. Turn off this temporarily.
# add_compile_options(-mllvm --riscv-v-vector-bits-min=512)

# FIXME: we have lots of warning message for `--source` option (display source code intermixed with disassembly).
set(OBJDUMP_OPTION --mcpu=${CPU_TARGET} --disassemble --demangle --all-headers --wide --line-numbers)

if(CPU_TARGET STREQUAL "sifive-x280n")
  option(RVV_SUPPORT "RVV Support" ON)
else()
  option(RVV_SUPPORT "RVV Support" OFF)
endif()

set(RISCV_C_COMPILE_FLAGS "" CACHE STRING "The riscv specific c compile flags.")
set(CMAKE_C_FLAGS_INIT "${RISCV_C_COMPILE_FLAGS}")
message(STATUS "CMAKE_C_FLAGS_INIT:${CMAKE_C_FLAGS_INIT}")

set(RISCV_CXX_COMPILE_FLAGS "" CACHE STRING "The riscv specific cxx compile flags.")
set(CMAKE_CXX_FLAGS_INIT "${RISCV_CXX_COMPILE_FLAGS}")
message(STATUS "CMAKE_CXX_FLAGS_INIT:${CMAKE_CXX_FLAGS_INIT}")

set(RISCV_COMPILE_DEFINITIONS "" CACHE STRING "The riscv specific compile defs.")
string (REPLACE " " ";" RISCV_COMPILE_DEFINITIONS_LIST "${RISCV_COMPILE_DEFINITIONS}")
add_compile_definitions(${RISCV_COMPILE_DEFINITIONS_LIST})
message(STATUS "RISCV_COMPILE_DEFINITIONS:${RISCV_COMPILE_DEFINITIONS}")

# link options
set(RISCV_LINKER_FLAGS "" CACHE STRING "The riscv specific link options.")
set(RISCV_DEFAULT_LINKER_FLAGS "-mcpu=${CPU_TARGET} -mabi=lp64d -Wl,--gc-sections")
set(RISCV_ALL_LINKER_FLAGS "${RISCV_LINKER_FLAGS} ${RISCV_DEFAULT_LINKER_FLAGS}")
string(STRIP "${RISCV_ALL_LINKER_FLAGS}" RISCV_ALL_LINKER_FLAGS)
set(CMAKE_EXE_LINKER_FLAGS_INIT "${RISCV_ALL_LINKER_FLAGS}")
message(STATUS "RISCV_LINKER_FLAGS:${RISCV_LINKER_FLAGS}")
message(STATUS "CMAKE_EXE_LINKER_FLAGS_INIT:${CMAKE_EXE_LINKER_FLAGS_INIT}")
