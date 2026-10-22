#!/bin/bash
##
## Copyright (c) 2025 Qualcomm Technologies, Inc. and/or its subsidiaries.
## SPDX-License-Identifier: MIT
##

mini-sandbox -x -D /tmp/log -- /bin/bash << 'EOF'
exit 5
EOF
R=$?
if [ $R -ne 5 ]; then
	echo "Failed (exited with $R)"
	exit 1;
else
	echo "Success"
fi
