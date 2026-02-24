
import requests
import sys
import os
import env
import shutil 

tap = False
script_dir = os.path.abspath(os.path.dirname(__file__))

import os
from pathlib import Path

def create_dir_structure(t_symlink):
    t_symlink.mkdir(exist_ok=True)

    t1 = t_symlink / "t1"
    t2 = t_symlink / "t2"
    t3 = t_symlink / "t3"

    t1.mkdir(exist_ok=True)
    t2.mkdir(exist_ok=True)
    t3.mkdir(exist_ok=True)

    t4 = t1 / "t4"
    t4.mkdir(exist_ok=True)

    ola = t2 / "ola"
    ola.write_text("lorem ipsum dolor sit", encoding="utf-8")

    symlink_path = t4 / "ola"

    if symlink_path.exists() or symlink_path.is_symlink():
        symlink_path.unlink()

    target_relative = os.path.abspath(ola)
    os.symlink(target_relative, symlink_path)

    
    print(f"Created folder structure under: {t_symlink}")
    print(f"File created: {ola}")
    print(f"Symlink created: {symlink_path} -> {target_relative}")


if len(sys.argv) > 1 and sys.argv[1] == "tap":
    import pyminisandbox.pyminitapbox as mn_sbx
    tap = True
else:
    import pyminisandbox.pyminisandbox as mn_sbx


if __name__ == "__main__":
    dir_path=Path.home()/"t_symlink"
    pid=os.fork()
    if pid == 0:
        print("Running outside of the sandbox...")
        create_dir_structure(dir_path)
        res = mn_sbx.mini_sandbox_setup_default()
        assert(res == 0)
        res = mn_sbx.mini_sandbox_enable_log("/tmp/log-pyminisandbox")
        assert(res == 0)
        res = mn_sbx.mini_sandbox_mount_bind(str(dir_path/"t1"/"t4"))

        assert(res == 0)

        res = mn_sbx.mini_sandbox_start()
        assert (res == 0)
        print("Running inside the sandbox...")
        with open(str(dir_path/"t1"/"t4"/"ola"),"r") as f:
            res=f.read()
        print(res)
        assert (res == "lorem ipsum dolor sit")
        exit(0)
    else:
        print("waiting")
        _,status =os.wait()
        code = os.WEXITSTATUS(status)
        print(f"Exit with {code}")
        shutil.rmtree(dir_path)
        exit(code)
    exit(0)
