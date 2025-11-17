#!/bin/bash

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

if ! [ -z "$PYTHON" ]; then
	echo "Using the suggested python interpreter $PYTHON"
else
	PYTHON=$(which python)
fi

check_exit $PYTHON test_all_api.py
check_exit $PYTHON test_all_api.py tap

check_exit $PYTHON test_err.py

check_exit $PYTHON test_parents_write.py
ls "$SCRIPT_DIR/../test_pyminisandbox.test"
check_last_command
rm "$SCRIPT_DIR/../test_pyminisandbox.test"

check_exit $PYTHON test.py
ls "$SCRIPT_DIR/../pyminisandbox-test-default.test"
check_last_command
rm "$SCRIPT_DIR/../pyminisandbox-test-default.test"

check_exit $PYTHON test.py tap
ls "$SCRIPT_DIR/../pyminisandbox-test-default.test"
check_last_command
rm "$SCRIPT_DIR/../pyminisandbox-test-default.test"

cd $ORIGINAL_DIR
