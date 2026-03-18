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


TOP_LVL="$BASE/top-level"
INTERMEDIATE="$TOP_LVL/working-dir-intermediate"
mkdir -p $INTERMEDIATE
FILE_NAME="mini-sandbox-test"
FILE_TO_MOUNT=$INTERMEDIATE/$FILE_NAME

export expected="mini-sandbox-test"
export FILE_TO_MOUNT

echo $expected > "$FILE_TO_MOUNT"


mini-sandbox -x -w $FILE_TO_MOUNT -- /bin/bash << 'EOF'

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


echo -e "\nTest reading from mounted file $FILE_TO_MOUNT "

actual="$(<"$FILE_TO_MOUNT")"

actual="${actual%$'\n'}"

[[ "$actual" == "$expected" ]] || {
  echo "FAIL: content mismatch"
  exit 1
}

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

echo -e "\nAll done"
