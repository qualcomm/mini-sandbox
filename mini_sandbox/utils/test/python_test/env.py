
import sys
import os

SCRIPT_DIR = os.path.dirname(os.path.abspath(__file__))
BINDINGS_DIR = "../../../src/py/"
bindings_abs_path = os.path.join(SCRIPT_DIR, BINDINGS_DIR)

bindings_type = "ctype_bindings"
# If you need to test pybind11 style bindings export an env variable PYBIND
if os.getenv("MINISANDBOX_PYBIND") != None:
    bindings_type = "pybind_bindings"

bindings = os.path.join(bindings_abs_path, bindings_type)
sys.path.append(bindings)


