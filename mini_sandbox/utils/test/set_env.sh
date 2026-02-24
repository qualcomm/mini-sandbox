#!/bin/bash
##
## Copyright (c) 2025 Qualcomm Technologies, Inc. and/or its subsidiaries.
## SPDX-License-Identifier: MIT
##


SCRIPT_DIR="$(cd -- "$(dirname -- "${BASH_SOURCE[0]}")" && pwd)"
REPO_BASE_DIR=$(realpath "$SCRIPT_DIR/../../../")
RELEASE="$REPO_BASE_DIR/release"

if [[ ! -d "$RELEASE" ]]; then
    echo "Release directory missing"
    exit
fi

export PATH="$RELEASE/bin":$PATH
export LD_LIBRARY_PATH="$RELEASE/lib"
export PYTHONPATH="$RELEASE/python/"
export PACKAGE_DIR="$RELEASE"
