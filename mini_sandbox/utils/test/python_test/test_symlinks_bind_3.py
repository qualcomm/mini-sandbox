#
# Copyright (c) 2025 Qualcomm Technologies, Inc. and/or its subsidiaries.
# SPDX-License-Identifier: MIT
#

import sys
import os
import env
import shutil 

tap = False
script_dir = os.path.abspath(os.path.dirname(__file__))

import os
from pathlib import Path



def make_test_symlinks(root: str = "test_symlinks") -> None:
    """
    Create the test_symlinks directory structure with directories, files,
    and symlinks matching the provided layout.
    """

    root = Path(root).resolve()

    def mkdir(p: Path):
        p.mkdir(parents=True, exist_ok=True)

    def mkfile(p: Path):
        mkdir(p.parent)
        p.touch(exist_ok=True)
        with open(str(p),"w") as f:
            f.write("lore ipsum dolor sit")

    def mksymlink(target: Path, link: Path):
        mkdir(link.parent)
        if link.exists() or link.is_symlink():
            if link.is_dir() and not link.is_symlink():
                shutil.rmtree(link)
            else:
                link.unlink()
        os.symlink(target, link)
    mkdir(root)
    mkdir(root /"usr/test" )

    mkdir(root /"local/" )
    mkfile(root / "local/test")
    mksymlink(root /"local/test", root/ "usr/test")

if len(sys.argv) > 1 and sys.argv[1] == "tap":
    import pyminisandbox.pyminitapbox as mn_sbx
    tap = True
else:
    import pyminisandbox.pyminisandbox as mn_sbx


if __name__ == "__main__":
    print("Running outside of the sandbox...")
    dir_path="/local/mnt/workspace/test_symlinks"
    make_test_symlinks(dir_path)
    res = mn_sbx.mini_sandbox_setup_default()
    assert(res == 0)
    res = mn_sbx.mini_sandbox_enable_log("/tmp/log-pyminisandbox")
    assert(res == 0)
    res = mn_sbx.mini_sandbox_mount_bind(f"{dir_path}/usr/")

    assert(res == 0)

    res = mn_sbx.mini_sandbox_start()
    assert (res == 0)
    print("Running inside the sandbox...")
    try:
        with open(f"{dir_path}/usr/test","r") as f:
            res=f.read()
        print(res)
        
        assert (res == "lore ipsum dolor sit")
    except:
        raise
   
    exit(0)
    
