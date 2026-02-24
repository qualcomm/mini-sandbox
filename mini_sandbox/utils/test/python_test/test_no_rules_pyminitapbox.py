#
# Copyright (c) 2025 Qualcomm Technologies, Inc. and/or its subsidiaries.
# SPDX-License-Identifier: MIT
#

import requests
import sys
import os
import env

script_dir = os.path.abspath(os.path.dirname(__file__))

from test import attempt_network_connection
import pyminisandbox.pyminitapbox as mn_sbx

if __name__ == "__main__":
    print("Running outside of the sandbox...")
    
    res = mn_sbx.mini_sandbox_setup_default()
    res = mn_sbx.mini_sandbox_start()
    assert (res == 0)
    
    print("Running inside the sandbox...")
    res = attempt_network_connection("http://www.google.com") 
    assert (res == 0)
    res = attempt_network_connection("http://www.qualcomm.com")
    assert (res == 0)
