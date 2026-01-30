#
# Copyright (c) 2025 Qualcomm Technologies, Inc. and/or its subsidiaries.
# SPDX-License-Identifier: MIT
#


import requests
import sys
import os
import env
from test import attempt_network_connection


tap = False
script_dir = os.path.abspath(os.path.dirname(__file__))

if len(sys.argv) > 1 and sys.argv[1] == "tap":
    import pyminisandbox.pyminitapbox as mn_sbx
    tap = True
else:
    import pyminisandbox.pyminisandbox as mn_sbx



if __name__ == "__main__":

    initial_pid = os.getpid()
    print(f"Running outside of the sandbox with pid {initial_pid}...") 
    res = mn_sbx.mini_sandbox_setup_default()
    res = mn_sbx.mini_sandbox_start()
    assert (res == 0)
    new_pid = os.getpid()
    print(f"New pid is {new_pid}. Parent is {os.getppid()}")

    assert(initial_pid != os.getpid())
    sys.exit(0)

    # We should never be able to reach this assert if the user of libmini-sandbox calls
    # the exit()
    assert (False)






