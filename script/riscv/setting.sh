#!/bin/bash

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

# The riscv toolchain settings.

SETTING_SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
ZLIBNG_ROOT_DIR=$SETTING_SCRIPT_DIR/../..

PREBUILT_DIR=$ZLIBNG_ROOT_DIR/prebuilt

if [[ "$OSTYPE" == "linux-gnu" ]]; then
  OS_VERSION=$(grep -oP '(?<=^ID=).+' /etc/os-release | tr -d '"')
  LINUX_VERSION=""
  if [[ "${OS_VERSION}" == "rhel" ]] ;then
    LINUX_VERSION="redhat8"
  elif [[ "${OS_VERSION}" == "ubuntu" ]] ;then
    LINUX_VERSION="ubuntu14"
  else
    echo "${OS_VERSION} is not supported"
    return 1
  fi
  RISCV_LINUX_CLANG_TOOLCHAIN_SERVER_NAME=login2.sifive.com
  RISCV_LINUX_CLANG_TOOLCHAIN_FILE_NAME=/nfs/teams/sw/static/frameworks/riscv_tool/toolsuite-linux/sifive-internal/2023.03.0/rc2/riscv64-unknown-linux-gnu-toolsuite-15.9.0-2023.03.0-rc2-x86_64-linux-${LINUX_VERSION}.tar.gz
  HOST_TYPE=linux
elif [[ "$OSTYPE" == "darwin"* ]]; then
  RISCV_LINUX_CLANG_TOOLCHAIN_FILE_ID=empty
  RISCV_LINUX_CLANG_TOOLCHAIN_FILE_NAME=empty
  HOST_TYPE=darwin
else
  echo "$OSTYPE is not supported"
  return 1
fi

RISCV_LINUX_TOOLCHAIN_PATH_PREFIX=${RISCV_LINUX_TOOLCHAIN_PATH_PREFIX:-$PREBUILT_DIR/toolchain/clang/$HOST_TYPE/riscv_linux}
