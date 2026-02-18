#
# Copyright (c) 2025 Qualcomm Technologies, Inc. and/or its subsidiaries.
# SPDX-License-Identifier: MIT
#

import requests
import sys
import os
import env

tap = False
script_dir = os.path.abspath(os.path.dirname(__file__))

import pyminisandbox.pyminisandbox as mn_sbx

def attempt_file_access(f = "/tmp/sandboxed_file"):
    res = 0
    try:
        f = open(f, "w")
        f.write("Test inside the sandbox\n")
        f.close()
        print("File written succesfully")
    except:
        print("Couldnt write in the path")
        res = -1
    return res


if __name__ == "__main__":
    print("Running outside of the sandbox...")
    res = mn_sbx.mini_sandbox_set_working_dir(script_dir)
    assert (res == 0)
    res = mn_sbx.mini_sandbox_setup_default()
    assert (res == 0)
    res = mn_sbx.mini_sandbox_start()
    assert (res == 0)
    
    print("Running inside the sandbox...")
    parent_path = os.path.join(script_dir, "../test_pyminisandbox.test")
    print("Writing into " + str(parent_path))
    res = attempt_file_access(f = parent_path)    
    assert(res == 0)


    





