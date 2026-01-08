#!/bin/bash
##
## Copyright (c) 2025 Qualcomm Technologies, Inc. and/or its subsidiaries.
## SPDX-License-Identifier: MIT
##

mini-sandbox -x -- /bin/bash << 'EOF'
exit 1
EOF

if [ $? -ne 1 ]; then
	echo "Failed (exited with $?)"
	exit 1;
else
	echo "Success"
fi
