#!/bin/bash
##
## Copyright (c) 2025 Qualcomm Technologies, Inc. and/or its subsidiaries.
## SPDX-License-Identifier: MIT
##

set -ex


SCRIPT_PATH="$(realpath "${BASH_SOURCE[0]}")"
SCRIPT_DIR="$(dirname "$SCRIPT_PATH")"
pushd $SCRIPT_DIR > /dev/null
echo "__version__ = \"${VERSION}\"" > $SCRIPT_DIR/ctype_bindings/pyminisandbox/__init__.py
chmod +x $SCRIPT_DIR/ctype_bindings/pyminisandbox/minitap
python3 -m build
python3 $SCRIPT_DIR/check_package.py
popd > /dev/null

set +ex
