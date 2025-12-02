#
# Copyright (c) 2025 Qualcomm Technologies, Inc. and/or its subsidiaries.
# SPDX-License-Identifier: MIT
#


import requests
import sys
import os
import env

import pyminisandbox as mn_sbx

stderr_fd_copy = os.dup(sys.stderr.fileno())
raw_open_with_fd=stderr_fd_copy
print(raw_open_with_fd)
f= open(raw_open_with_fd, "w")
mn_sbx.mini_sandbox_setup_default()
mn_sbx.mini_sandbox_start()

res = f.write("TEST WRITING IN STDERR DUPPED\n")
res = f.close()

print("Done")
