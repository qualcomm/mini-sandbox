#
# Copyright (c) 2025 Qualcomm Technologies, Inc. and/or its subsidiaries.
# SPDX-License-Identifier: MIT
#

import requests
import sys
import os
import env
from pathlib import Path

tap = False
script_dir = os.path.abspath(os.path.dirname(__file__))

if len(sys.argv) > 1 and sys.argv[1] == "tap":
    import pyminisandbox.pyminitapbox as mn_sbx
    tap = True
else:
    import pyminisandbox.pyminisandbox as mn_sbx

def attempt_file_access(f_path = "/tmp/sandboxed_file"):
    res = 0
    try:
        f = open(f_path, "w")
        f.write("Test inside the sandbox\n")
        f.close()
        print("File written succesfully")
    except:
        print("Couldnt write in the path")
        res = -1
    return res

if __name__ == "__main__":
    print("Running outside of the sandbox...")
    file_path="./read_only"
    res = mn_sbx.mini_sandbox_setup_default()
    assert(res == 0)
    # Mount all Python libraries in case Python isn't an executable.
    for path in sys.path:
        if Path(path).exists():
            mn_sbx.mini_sandbox_mount_write(path)
    mn_sbx.mini_sandbox_allow_all_domains()
    res = mn_sbx.mini_sandbox_start()
    assert(res == 0)
    print("Running inside the sandbox...")
    res = attempt_file_access(f_path =file_path )    
    assert(res == -1)


    





