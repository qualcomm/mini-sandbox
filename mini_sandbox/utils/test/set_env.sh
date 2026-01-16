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

PATH="$RELEASE/bin":$PATH
LD_LIBRARY_PATH="$RELEASE/lib"
PYTHONPATH="$RELEASE/python/"
PACKAGE_DIR="$RELEASE"
