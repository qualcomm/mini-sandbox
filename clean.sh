#!/bin/bash
##
## Copyright (c) 2025 Qualcomm Technologies, Inc. and/or its subsidiaries.
## SPDX-License-Identifier: MIT
##

SCRIPT_DIR=$(dirname "$(readlink -f "$0")")
make -C $SCRIPT_DIR/mini_sandbox/src/main/tools clean
make -C $SCRIPT_DIR/mini_sandbox/utils/test clean
make -C $SCRIPT_DIR/mini_sandbox/src/py/pybind_bindings clean
make -C $SCRIPT_DIR/minitap clean
rm -rf $SCRIPT_DIR/release/
