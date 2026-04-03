#!/bin/bash
##
## Copyright (c) 2025 Qualcomm Technologies, Inc. and/or its subsidiaries.
## SPDX-License-Identifier: MIT
##

SCRIPT_PATH="$(realpath "${BASH_SOURCE[0]}")"
SCRIPT_DIR="$(dirname "$SCRIPT_PATH")"
IP_RULES="$SCRIPT_DIR/ip.list"
PORTS_RULES="$SCRIPT_DIR/ports.list"

which minitap
if [[ $? -ne 0 ]]; then
    exit $?
fi

if [ "$LANDLOCK_TEST" = "1" ]; then
	RULES=$PORTS_RULES
else
	RULES=$IP_RULES
fi

mini-tapbox -x -F $RULES -- /bin/bash << 'EOF'

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

test_port_not_reachable() {
    local host="$1"
    local port="$2"

    if timeout 3 nc -z "$host" "$port" 2>/dev/null; then
        echo "$host:$port reachable"
        exit 1
    else
        echo "$host:$port blocked as expected"
        return 0
    fi
}


echo -e "\nTest writing in HOME dir"
echo "sandbox-test" > $HOME/test.txt
check_last_command

echo -e "\nTest reading in parent dir of CWD ($PWD/../)"
count=$(ls $PWD/../ | wc -l)
check_ls_count $count 1

echo -e "\nTest showing the network is available"
wget www.google.com -T 1 -t 1
check_last_command


if [ "$LANDLOCK_TEST" = "1" ]; then
  test_port_not_reachable google.com 22
  test_port_not_reachable google.com 12345
else
  echo -e "\nTest showing the network is unavailable"
  wget qualcomm.com --tries=3 --timeout=1
  check_last_command_failed
fi

EOF


exit $?
