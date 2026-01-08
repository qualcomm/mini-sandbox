#
# Copyright (c) 2025 Qualcomm Technologies, Inc. and/or its subsidiaries.
# SPDX-License-Identifier: MIT
#

from internal_mini_sandbox import *

init(tap_mode = True)

def mini_sandbox_allow_max_connections(max_connections):
    if lib is None:
        return -1
    return lib.mini_sandbox_allow_max_connections(max_connections)


