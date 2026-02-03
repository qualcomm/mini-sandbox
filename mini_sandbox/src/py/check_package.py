#
# Copyright (c) 2025 Qualcomm Technologies, Inc. and/or its subsidiaries.
# SPDX-License-Identifier: MIT
#

FILES = [
    "__init__.py",
    "internal_mini_sandbox.py",
    "libmini-sandbox.so",
    "libmini-tapbox.so",
    "pyminisandbox.py",
    "pyminitapbox.py"
]

import zipfile, glob, os
wheel = sorted(glob.glob('dist/*.whl'))[-1]
print("Inspecting:", wheel)
archived_files = []
with zipfile.ZipFile(wheel) as z:
    for n in z.namelist():
        archived_files.append(os.path.basename(n))

for f in FILES:
    print(f"did we pack {f} ?")
    assert (f in archived_files)
print("Wheel validated !")

