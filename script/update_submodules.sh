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

# This script will update submodules.

set -e

git submodule update --init --recursive third_party/metal-run
