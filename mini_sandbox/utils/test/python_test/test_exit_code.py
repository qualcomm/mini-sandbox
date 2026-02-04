#
# Copyright (c) 2025 Qualcomm Technologies, Inc. and/or its subsidiaries.
# SPDX-License-Identifier: MIT
#

import requests
import sys
import os
import env


tap = False

if len(sys.argv) > 1 and sys.argv[1] == "tap":
    import pyminisandbox.pyminitapbox as mn_sbx
    tap = True
else:
    import pyminisandbox.pyminisandbox as mn_sbx

if __name__ == "__main__":
    print("Running outside of the sandbox...")
    
    mn_sbx.mini_sandbox_setup_default()
    mn_sbx.mini_sandbox_start()
    sys.exit(5)
    assert (False)
    print("Ok")



