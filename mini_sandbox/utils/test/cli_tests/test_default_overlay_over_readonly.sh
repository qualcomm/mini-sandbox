#!/bin/bash
##
## Copyright (c) 2025 Qualcomm Technologies, Inc. and/or its subsidiaries.
## SPDX-License-Identifier: MIT
##


#SCRIPT_PATH="$(realpath "${BASH_SOURCE[0]}")"

if [[ -n "$1" ]]; then
        export BASE="$1"
else
        export BASE=$HOME
fi

export WRITABLE="/var/tmp/writable"
export OVERLAY="/var/tmp/overlay"
mkdir -p $WRITABLE
mkdir -p $OVERLAY

mini-sandbox -x -w $WRITABLE -k $OVERLAY -- /bin/bash << 'EOF'

check_last_command() {
    if [ $? -ne 0 ]; then
        echo "Error: Last command failed."
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
	exit 1
    fi
}


check_ls_count() {
    local count="$1"
    local threshold="$2"

    if [ "$count" -gt "$threshold" ]; then
        echo "Success: Directory contains more than $threshold items ($count)."
        return 0
    else
        echo "Error: Directory contains $count items, which is not more than $threshold."
        exit 1
    fi
}

echo -e "\nWriting to /var that defaults to read-only"
echo "sandbox-test" > /var/test.txt
check_last_command_failed

echo -e "\nTest writing to folder mounted as write $WRITABLE"
echo "sandbox-test" > $WRITABLE/test.txt
check_last_command

echo -e "\nTest writing to folder mounted as overlay $OVERLAY"
echo "sandbox-test" > $OVERLAY/test.txt
check_last_command

EOF

if [[ $? -ne 0 ]]; then
    exit $?
fi



check_last_command() {
    if [ $? -ne 0 ]; then
        echo "Error: Last command failed."
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
        exit 1
    fi
}

echo -e "\nCheck that mount write has gone through in writable folder $WRITABLE"
ls $WRITABLE/test.txt
check_last_command

echo -e "\nCheck that mount overlay has not gone through in overlay folder $OVERLAY"
ls $OVERLAY/test.txt
check_last_command_failed

echo -e "\nAll done"
