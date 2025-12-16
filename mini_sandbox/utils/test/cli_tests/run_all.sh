#!/bin/bash
##
## Copyright (c) 2025 Qualcomm Technologies, Inc. and/or its subsidiaries.
## SPDX-License-Identifier: MIT
##

set -ex

SCRIPT_PATH="$(realpath "${BASH_SOURCE[0]}")"
SCRIPT_DIR="$(dirname "$SCRIPT_PATH")"
WORKSPACE="/local/mnt/workspace/"

check_exit() {
    "$@"
    local status=$?

    if [ $status -ne 0 ]; then
        echo "Command failed with exit code $status"
        exit $status
    fi

    return 0
}


check_exit $SCRIPT_DIR/test_err.sh
check_exit $SCRIPT_DIR/test_default_base.sh  
check_exit $SCRIPT_DIR/test_default_overlay.sh  

if [ "$HOME" != "/root" ]; then
    check_exit $SCRIPT_DIR/test_default_parents_folders_overlay.sh $HOME
fi

if [ -d $WORKSPACE ]; then
    check_exit $SCRIPT_DIR/test_default_parents_folders_overlay.sh $WORKSPACE
fi

IFS=":" read -ra paths <<< "$OTHER_PATH"

for p in "${paths[@]}"; do
    echo -e "\nTesting additional path $p"
    check_exit $SCRIPT_DIR/test_default_parents_folders_overlay.sh $p
done

check_exit $SCRIPT_DIR/test_default_write.sh
check_exit $SCRIPT_DIR/test_default_tap.sh
check_exit $SCRIPT_DIR/test_tap_firewall.sh
