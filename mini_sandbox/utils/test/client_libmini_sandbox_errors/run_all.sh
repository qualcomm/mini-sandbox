#!/bin/bash
##
## Copyright (c) 2025 Qualcomm Technologies, Inc. and/or its subsidiaries.
## SPDX-License-Identifier: MIT
##

set -x

SCRIPT_PATH="$(realpath "${BASH_SOURCE[0]}")"
SCRIPT_DIR="$(dirname "$SCRIPT_PATH")"
PARENT_FOLDER="$(dirname "$SCRIPT_DIR")"
ORIGINAL_DIR="$(pwd)"

cd $SCRIPT_DIR

check_exit() {
    "$@"
    local status=$?

    if [ $status -ne 0 ]; then
        echo "Command $@ failed with exit code $status"
        cd $ORIGINAL_DIR
        exit $status
    fi

    return 0
}


check_exit $SCRIPT_DIR/call_default_after_start.bin
check_exit $SCRIPT_DIR/call_start_twice.bin
check_exit $SCRIPT_DIR/check_only_one_root.bin
check_exit $SCRIPT_DIR/check_overlay_enabled.bin
check_exit $SCRIPT_DIR/check_validate_path.bin
check_exit $SCRIPT_DIR/only_one_log.bin
check_exit $SCRIPT_DIR/check_exit_from_user.bin
check_exit $SCRIPT_DIR/check_runtime_init_error.bin
check_exit $SCRIPT_DIR/check_runtime_init_error_custom.bin
check_exit $SCRIPT_DIR/check_runtime_init_error_readonly.bin

cd $ORIGINAL_DIR
