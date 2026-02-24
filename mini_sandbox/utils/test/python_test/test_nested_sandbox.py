
import requests
import sys
import os
import env

tap = False
script_dir = os.path.abspath(os.path.dirname(__file__))

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
        print(e)
        res = -2
    return res

def attempt_file_access():
    res = 0
    try:
        f = open("/tmp/sandboxed_file", "w")
        f.write("Test inside the sandbox\n")
        f.close()
        print("File written succesfully in tmp")
    except:
        print("Couldnt write in the path")
        res = -1
    return res


if len(sys.argv) > 1 and sys.argv[1] == "tap":
    import pyminisandbox.pyminitapbox as mn_sbx
    tap = True
else:
    import pyminisandbox.pyminisandbox as mn_sbx


if __name__ == "__main__":
    print("Running outside of the sandbox...")
    
    res = mn_sbx.mini_sandbox_setup_default()
    assert(res == 0)
    res = mn_sbx.mini_sandbox_enable_log("/tmp/log-pyminisandbox")
    assert(res == 0)
    res = mn_sbx.mini_sandbox_mount_bind("/bin")
    assert(res == 0)
    res = mn_sbx.mini_sandbox_mount_empty_output_file("/tmp/pyminisandbox-empty-file")
    assert(res == 0)
    res = mn_sbx.mini_sandbox_mount_overlay(os.path.join(script_dir, "../"))
    assert(res == 0)
    if not tap:
        res = mn_sbx.mini_sandbox_share_network()
        assert(res == 0)
    else:
        res = mn_sbx.mini_sandbox_allow_domain("www.google.com")
        assert (res == 0)
        res = mn_sbx.mini_sandbox_allow_ipv4_subnet("10.49.88.124") #DNS resolver
        assert (res == 0)
        res = mn_sbx.mini_sandbox_allow_ipv4_subnet("142.0.0.0/8") # With this and previous, google connection should go through
        assert (res == 0)

    res = mn_sbx.mini_sandbox_start()
    assert (res == 0)
    res=mn_sbx.mini_sandbox_is_running()
    assert (res is True)
    res = mn_sbx.mini_sandbox_start()
    assert (res < 0)
    res = mn_sbx.mini_sandbox_get_last_error_code()
    assert (res==mn_sbx.MiniSandboxErrors.SANDBOX_ALREADY_STARTED)
    #The sandbox should work anyway, even if we started it twice.
    print("Running inside the sandbox...")
    res = attempt_network_connection("http://www.google.com") 
    assert (res == 0)
    res = attempt_file_access() 
    assert(res == 0)
