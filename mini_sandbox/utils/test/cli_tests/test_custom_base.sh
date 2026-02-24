#!/bin/bash
##
## Copyright (c) 2025 Qualcomm Technologies, Inc. and/or its subsidiaries.
## SPDX-License-Identifier: MIT
##

DIR="/tmp/sbx_dir"
EXPECTED_MOUNTS=12
export EXPECTED_MOUNTS
mini-sandbox -o $DIR -d $DIR -k /etc -k /lib -k /lib64 -k /sbin -k /bin -k /usr -M /var -M /opt -- /bin/bash << 'EOF'

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

    if [ "$count" -eq "$threshold" ]; then
        echo "Success: Directory contains $threshold items ($count)."
        return 0
    else
        echo "Error: Directory contains $count items, expected value is $threshold."
        exit 1
    fi
}

echo -e "\nTest that only $EXPECTED_MOUNTS mounts are under /"
count=$(ls / | wc -l)
check_ls_count $count $EXPECTED_MOUNTS

echo -e "\nTest showing the network is unavailable"
wget google.com -T 1
check_last_command_failed
EOF

exit $?
