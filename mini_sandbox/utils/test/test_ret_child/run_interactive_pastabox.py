import os
import sys
import subprocess
sys.path.append("../../../src/py/pybind_bindings")
import pyminitapbox as mn_sbx

mn_sbx.mini_sandbox_setup_default()

for i in sys.path:
    if os.path.exists(i):
        mn_sbx.mini_sandbox_mount_bind(i)
mn_sbx.mini_sandbox_mount_bind("/usr2/rstellut/.pyenv/")
mn_sbx.mini_sandbox_enable_log("/tmp/sandbox_log")
mn_sbx.mini_sandbox_start()

proc = subprocess.run(["/bin/sh", "-c", "exit -5"], capture_output=True) #expected 2 as return code

print(proc.stdout)
print(proc.stderr, file=sys.stderr)
print(proc.returncode)
assert(proc.returncode == 2)
