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

import pyminisandbox.pyminitapbox as mn_sbx

def attempt_network_connection(domain):
    res = 0
    try:
        print("Trying to connect to {0}...".format(domain))
        response = requests.get(domain, timeout = 2)
        print("Connection worked for {0}".format(domain))

        print("Status Code:", response.status_code)
        print("Content:", response.text[:500])  # Print the first 500 characters of the response

    except requests.exceptions.Timeout:
        print("The request timed out after 3 seconds")
        res = -1
    except Exception as e: 
        print("Connection failed as expected !")
        print(e)
        res = -2
    return res


if __name__ == "__main__":
    print("Running outside of the sandbox...")
    file_path = os.path.join(script_dir, "read_only.test")
    os.system(f"touch {file_path}")
    os.system(f"chmod 444 {file_path}")
    res = mn_sbx.mini_sandbox_setup_default()
    assert(res == 0)
    # Mount all Python libraries in case Python isn't an executable.
    for path in sys.path:
        if Path(path).exists():
            mn_sbx.mini_sandbox_mount_write(path)

    res= mn_sbx.mini_sandbox_allow_all_domains()
    assert (res ==0)


    res= mn_sbx.mini_sandbox_allow_domain("www.google.com")
    assert(res<0)








