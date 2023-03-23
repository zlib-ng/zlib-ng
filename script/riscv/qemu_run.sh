#!/bin/sh

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

# This script will run the excutable file on the linux-based user-mode qemu.

set -e

SCRIPT_PATH=$(dirname "$0")

source $SCRIPT_PATH/setting.sh

if [ "$#" -ne 1 ]; then
  echo "Usage: ./qemu_run.sh <prog>"
  exit 1
fi

PROG=$1
if [ ! -f "${PROG}" ]; then
  echo "program ${PROG} does not exist"
  exit 1
fi

$ZLIBNG_ROOT_DIR/third_party/metal-run/qemu-run \
  --sysroot "$RISCV_LINUX_TOOLCHAIN_PATH_PREFIX/sysroot" \
  -- "${PROG}"
