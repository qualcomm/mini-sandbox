#
# Copyright (c) 2025 Qualcomm Technologies, Inc. and/or its subsidiaries.
# SPDX-License-Identifier: MIT
#

import sys
import os
import shutil
import env

tap = False
script_dir = os.path.abspath(os.path.dirname(__file__))

if len(sys.argv) > 1 and sys.argv[1] == "tap":
    import pyminisandbox.pyminitapbox as mn_sbx
    tap = True
else:
    import pyminisandbox.pyminisandbox as mn_sbx


if __name__ == "__main__":
    missing_dir = os.path.join(os.getcwd(), "../","missing_dir_a", "missing_dir_b")
    top_level_dir = os.path.join(os.getcwd(), "../","missing_dir_a")
    target_path = os.path.join(missing_dir, "../","test.txt")
    expected_content = "uniquetest"

    print(missing_dir)
    assert(not os.path.exists(missing_dir))

    pid = os.fork()
    if pid == 0:
        print("Running outside of the sandbox...")

        res = mn_sbx.mini_sandbox_setup_default()
        assert(res == 0)

        res = mn_sbx.mini_sandbox_mount_write(missing_dir)
        assert(res == 0)

        res = mn_sbx.mini_sandbox_start()
        assert(res == 0)

        print("Running inside the sandbox...")
        os.makedirs(missing_dir, exist_ok=True)
        with open(target_path, "w") as fd:
            fd.write(expected_content)
        print("File written succesfully to {0}".format(target_path))
        exit(0)
    else:
        print("waiting")
        try:
            _, status = os.wait()
            code = os.WEXITSTATUS(status)
            print(f"Exit with {code}")
            assert(code == 0)

            assert(not os.path.exists(target_path))
            assert(not os.path.exists(missing_dir))
            assert(not os.path.exists(top_level_dir))
            print("Confirmed target file and directories do not exist")
        finally:
            if os.path.exists(top_level_dir):
                shutil.rmtree(top_level_dir)
                print("Cleaned up {0}".format(top_level_dir))
        exit(0)
