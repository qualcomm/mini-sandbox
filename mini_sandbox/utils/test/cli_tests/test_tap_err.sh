#!/bin/bash
##
## Copyright (c) 2025 Qualcomm Technologies, Inc. and/or its subsidiaries.
## SPDX-License-Identifier: MIT
##

mini-tapbox -x -- /bin/bash << 'EOF'
exit 5
EOF
exit_code=$?
if [ $exit_code -ne 5 ]; then
	echo "Failed (exited with $?)"
	exit 1;
else
	echo "Success (exit was $exit_code)"
fi
