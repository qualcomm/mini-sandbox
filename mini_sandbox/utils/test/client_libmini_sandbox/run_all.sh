#!/bin/bash
##
## Copyright (c) 2025 Qualcomm Technologies, Inc. and/or its subsidiaries.
## SPDX-License-Identifier: MIT
##

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

check_last_command() {
    if [ $? -ne 0 ]; then
        echo "Error: Last command failed."
        cd $ORIGINAL_DIR
        exit 1
    else
        echo "Success: Last command succeeded."
    fi
}


check_last_command_failed() {
    if [ $? -ne 0 ]; then
        echo "Success: Last command failed (As Expected)."
        return 0
    else
        echo "Error: Last command succeeded but should have failed."
        cd $ORIGINAL_DIR
        exit 1
    fi
}

rm -f $PARENT_FOLDER/libminisandbox.test
rm -f $HOME/libminisandbox.test


check_exit $SCRIPT_DIR/client_c.bin

check_exit $SCRIPT_DIR/client_cxx.bin

mkdir -p "/tmp/overlay_client"
mkdir -p "/tmp/sandbox_client"
check_exit $SCRIPT_DIR/client_cxx_custom.bin

ls "$HOME/libminisandbox.test"
check_last_command_failed
ls "$PARENT_FOLDER/libminisandbox.test"
check_last_command_failed

check_exit $SCRIPT_DIR/client_cxx_default.bin
ls "$HOME/libminisandbox.test"
check_last_command_failed
ls "$PARENT_FOLDER/libminisandbox.test"
check_last_command_failed

check_exit $SCRIPT_DIR/client_cxx_hermetic.bin
ls "$PARENT_FOLDER/libminisandbox.test"
check_last_command_failed


check_exit $SCRIPT_DIR/client_cxx_default_write_parents.bin
ls "$HOME/libminisandbox.test"
check_last_command_failed
ls "$PARENT_FOLDER/libminisandbox.test"
check_last_command
rm -f $PARENT_FOLDER/libminisandbox.test
rm -f $HOME/libminisandbox.test
cd $ORIGINAL_DIR
