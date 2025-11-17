
import requests
import sys
import os
import env


tap = False
script_dir = os.path.abspath(os.path.dirname(__file__))

if len(sys.argv) > 1 and sys.argv[1] == "tap":
    import pyminitapbox as mn_sbx
    tap = True
else:
    import pyminisandbox as mn_sbx


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
    parent = os.path.join(script_dir, "../")
    res = mn_sbx.mini_sandbox_mount_write(parent)
    assert (res == 0)

    if tap:
        # rules_path = os.path.join(script_dir, "../../utils/firewall_example_rules/rules.bak")

        res = mn_sbx.mini_sandbox_allow_domain("google.com")
        assert (res == 0)
        res = mn_sbx.mini_sandbox_allow_ipv4_subnet("10.49.88.124") #DNS resolver
        assert (res == 0)
        res = mn_sbx.mini_sandbox_allow_ipv4_subnet("142.0.0.0/8") # With this and previous, google connection should go through
        assert (res == 0)
        #mn_sbx.mini_sandbox_allow_ipv4_subnet("172.0.0.0/8") # With this and previous, google connection should go through
        # mn_sbx.mini_sandbox_allow_all_domains() # With this all the connection should go through

    res = mn_sbx.mini_sandbox_start()

    assert (res == 0)
    
    print("Running inside the sandbox...")
    res = attempt_network_connection("http://google.com") 
    if tap:
        assert (res == 0)
    else:
        assert(res < 0)
 
    res = attempt_network_connection("http://www.qualcomm.com")
    assert(res < 0)
    res = attempt_file_access(os.path.join(parent, "pyminisandbox-test-default.test"))    
    assert(res == 0)


    





