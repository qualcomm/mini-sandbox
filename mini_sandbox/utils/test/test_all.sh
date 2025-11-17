#!/bin/bash

set -e 

SCRIPT_PATH="$(realpath "${BASH_SOURCE[0]}")"
SCRIPT_DIR="$(dirname "$SCRIPT_PATH")"
pushd $SCRIPT_DIR > /dev/null
make
$SCRIPT_DIR/cli_tests/run_all.sh
$SCRIPT_DIR/client_libmini_sandbox/run_all.sh
$SCRIPT_DIR/client_libmini_tapbox/run_all.sh
$SCRIPT_DIR/python_test/run_all.sh
unset LDLIBRARY_PATH
popd > /dev/null
