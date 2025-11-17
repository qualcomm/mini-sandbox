
import requests
import sys
import os
import env


tap = False

if len(sys.argv) > 1 and sys.argv[1] == "tap":
    import pyminitapbox as mn_sbx
    tap = True
else:
    import pyminisandbox as mn_sbx

if __name__ == "__main__":
    print("Running outside of the sandbox...")
    
    mn_sbx.mini_sandbox_setup_default()
    mn_sbx.mini_sandbox_mount_write("/usr2/folder_that_does_not_exist/")
    res = mn_sbx.mini_sandbox_get_last_error_msg()
    err_code = mn_sbx.mini_sandbox_get_last_error_code()
    assert (err_code != 0)
    assert (res != "" and res != None)



