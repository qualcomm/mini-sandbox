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
WD="$INTERMEDIATE/working-dir"
mkdir -p $WD
echo "mini-sandbox-test" > "$BASE/mini-sandbox-test"
echo "mini-sandbox-test" > "$TOP_LVL/mini-sandbox-test"

pushd $WD || {
        echo "Failed to change dir"
        exit 1
}

echo "mini-sandbox -x -w $INTERMEDIATE -- /bin/bash "
mini-sandbox -x -w $INTERMEDIATE -- /bin/bash << 'EOF'

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

echo $PWD
echo -e "\nTest writing in parent folder $PWD/../test.txt"
echo "sandbox-test" > $PWD/../test.txt
check_last_command

echo -e "\nTest writing in parent of the parent folder $PWD/../../"
echo "sandbox-test" > $PWD/../../test.txt
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

echo -e "\nCheck that mount write has gone through in parent folders"
ls $PWD/../test.txt
check_last_command

echo -e "\nCheck that mount write has gone through in parent's parent folders"
ls $PWD/../../test.txt
check_last_command_failed

echo -e "\nAll done"
