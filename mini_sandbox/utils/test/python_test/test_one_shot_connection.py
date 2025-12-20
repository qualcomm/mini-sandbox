#
# Copyright (c) 2025 Qualcomm Technologies, Inc. and/or its subsidiaries.
# SPDX-License-Identifier: MIT
#


import requests
import sys
import os
import env

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
        print("Connection failed !")
        print(e)
        res = -2
    return res


def attempt_file_access(filepath):
    res = 0
    try:
        f = open(filepath, "w")
        f.write("Test inside the sandbox\n")
        f.close()
        print("File written succesfully in overlay filesystem")
    except:
        print("Couldnt write in the path")
        res = -1
    return res


if __name__ == "__main__":
    print("Running outside of the sandbox...")
    
    res = mn_sbx.mini_sandbox_setup_default()
    assert (res == 0)
    res = mn_sbx.mini_sandbox_allow_max_connections(2)
    assert (res == 0)
    res = mn_sbx.mini_sandbox_start()
    assert (res == 0)
    print("Running inside the sandbox...")
    res = attempt_network_connection("http://www.google.com") 
    assert (res == 0)
    res = attempt_network_connection("http://www.qualcomm.com")
    assert(res < 0)


    





