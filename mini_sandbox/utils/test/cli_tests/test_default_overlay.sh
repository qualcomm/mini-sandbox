#!/bin/bash
##
## Copyright (c) 2025 Qualcomm Technologies, Inc. and/or its subsidiaries.
## SPDX-License-Identifier: MIT
##

mini-sandbox -x -- /bin/bash << 'EOF'

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


echo -e "\nTest writing in parent folder $PWD/../"
echo "sandbox-test" > $PWD/../test.txt
check_last_command

echo -e "\nTest writing in /local/mnt/workspace (if exists)"
DIR="/local/mnt/workspace"
if [ -d "$DIR" ]; then
	echo "sandbox-test" > /local/mnt/workspace/test.txt
	check_last_command
fi

echo -e "\nTest writing in /opt (read-only)"
echo "sandbox-test" > /opt/test.txt
check_last_command_failed

echo -e "\nTest writing in /tmp"
echo "sandbox-test" > /tmp/test.txt
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

echo -e "\nCheck that overlayfs has not gone through in parent folders"
ls $PWD/../test.txt
check_last_command_failed

DIR="/local/mnt/workspace"
if [ -d "$DIR" ]; then
	echo -e "\nCheck that overlayfs has not gone through in other folders"
	ls /local/mnt/workspace/test.txt
	check_last_command_failed
fi

echo -e "\nCheck that /tmp has been written (/tmp/mini-sandbox-tmp/)"
ls /tmp/mini-sandbox-tmp/test.txt
check_last_command
