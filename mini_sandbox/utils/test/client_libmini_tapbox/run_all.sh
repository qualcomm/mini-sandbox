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


rm -f $HOME/test2
check_exit $SCRIPT_DIR/client.bin
ls "$HOME/test2"
check_last_command

rm -f $HOME/test2
# THis is going to use the dynamic linker so if the lib is not installed system-wide
# You'll have to export LD_LIBRARY_PATH=/path/to/libs/
check_exit $SCRIPT_DIR/client.so.bin
ls "$HOME/test2"
check_last_command

rm -f $HOME/test2
cd $ORIGINAL_DIR
