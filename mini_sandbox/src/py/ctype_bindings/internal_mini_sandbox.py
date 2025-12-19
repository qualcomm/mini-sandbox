#
# Copyright (c) 2025 Qualcomm Technologies, Inc. and/or its subsidiaries.
# SPDX-License-Identifier: MIT
#

import os
import ctypes
import platform

script_dir = os.path.dirname( os.path.abspath(__file__))

# Some handy error codes.
# 
# LIB_NOT_LOADED -> something went wrong when loading the .so (see exception)
# FEATURE_NOT_AVAILABLE -> the API is not available in the current shared object. Need to re-build
# INVALID_ARG -> you called an API with wrong argument

LIB_NOT_LOADED = -1
FEATURE_NOT_AVAILABLE = -2
INVALID_ARG = -3

_lib = None
_tap = False

def init(tap_mode = False):
    global _tap
    global _lib

    _tap = tap_mode
    if tap_mode:
        so_name = "libmini-tapbox.so"
    else:
        so_name = "libmini-sandbox.so"

    libname = os.path.join(script_dir, so_name)
    try:
        if platform.system() == "Linux":
            _lib = ctypes.CDLL(libname)
        else:
            _lib = None
    except Exception as e:
        print("An exception occured while loading the library" + libname)
        print(e)
        _lib = None
        
    if _lib is not None:
        _lib.mini_sandbox_get_last_error_msg.restype = ctypes.c_char_p


def mini_sandbox_get_last_error_code():
    if _lib is None:
        return LIB_NOT_LOADED
    return _lib.mini_sandbox_get_last_error_code()

def mini_sandbox_get_last_error_msg():
    if _lib is None:
        return LIB_NOT_LOADED
    return _lib.mini_sandbox_get_last_error_msg().decode()

def mini_sandbox_enable_log(path):
    if _lib is None:
        return LIB_NOT_LOADED
    return _lib.mini_sandbox_enable_log(path.encode())

def mini_sandbox_mount_parents_write(num_of_parents = -1):
    if _lib is None:
        return LIB_NOT_LOADED
    if num_of_parents == -1:
        return _lib.mini_sandbox_mount_parents_write()
    if num_of_parents > 0:
        upper_lvls = "".join("../" for i in range(num_of_parents))
        parent = os.path.abspath(upper_lvls)
        if parent is not None:
            return _lib.mini_sandbox_mount_write(parent.encode())
    return INVALID_ARG

def mini_sandbox_setup_default():
    if _lib is None:
        return LIB_NOT_LOADED
    return _lib.mini_sandbox_setup_default()

def mini_sandbox_setup_custom(overlayfs_dir, sandbox_root):
    if _lib is None:
       return LIB_NOT_LOADED
    return _lib.mini_sandbox_setup_custom(overlayfs_dir.encode(), sandbox_root.encode())

def mini_sandbox_setup_hermetic(sandbox_root):
    if _lib is None:
        return LIB_NOT_LOADED
    return _lib.mini_sandbox_setup_hermetic(sandbox_root.encode())
     
def mini_sandbox_start():
    if _lib is None:
        return LIB_NOT_LOADED
    return _lib.mini_sandbox_start()

def mini_sandbox_mount_bind(path):
    if _lib is None:
       return LIB_NOT_LOADED
    return _lib.mini_sandbox_mount_bind(path.encode())

def mini_sandbox_mount_write(path):
    if _lib is None:
        return LIB_NOT_LOADED
    return _lib.mini_sandbox_mount_write(path.encode())

def mini_sandbox_mount_tmpfs(path):
    if _lib is None:
        return LIB_NOT_LOADED
    return _lib.mini_sandbox_mount_tmpfs(path.encode())

def mini_sandbox_mount_overlay(path):
    if _lib is None:
        return LIB_NOT_LOADED
    return _lib.mini_sandbox_mount_overlay(path.encode())

def mini_sandbox_mount_empty_output_file(path):
    if _lib is None:
        return LIB_NOT_LOADED
    return _lib.mini_sandbox_mount_empty_output_file(path.encode())

def mini_sandbox_share_network():
    if _lib is None:
        return LIB_NOT_LOADED
    return _lib.mini_sandbox_share_network()


def mini_sandbox_allow_max_connections(max_connections):
    if _lib is None:
        return LIB_NOT_LOADED
    if _tap and hasattr(_lib, "mini_sandbox_allow_max_connections"):
        return _lib.mini_sandbox_allow_max_connections(max_connections)
    return FEATURE_NOT_AVAILABLE

def mini_sandbox_allow_connections(path):
    if _lib is None:
        return LIB_NOT_LOADED
    if _tap and hasattr(_lib, "mini_sandbox_allow_connections"):
        return _lib.mini_sandbox_allow_connections(path.encode())
    raise FEATURE_NOT_AVAILABLE

def mini_sandbox_allow_ipv4(ip):
    if _lib is None:
        return LIB_NOT_LOADED
    if _tap and hasattr(_lib, "mini_sandbox_allow_ipv4"):
        return _lib.mini_sandbox_allow_ipv4(ip.encode())
    return FEATURE_NOT_AVAILABLE

def mini_sandbox_allow_domain(domain):
    if _lib is None:
        return LIB_NOT_LOADED

    if _tap and hasattr(_lib, "mini_sandbox_allow_domain"):
        return _lib.mini_sandbox_allow_domain(domain.encode())
    return FEATURE_NOT_AVAILABLE

def mini_sandbox_allow_all_domains():
    if _lib is None:
        return LIB_NOT_LOADED
    if _tap and hasattr(_lib, "mini_sandbox_allow_all_domains"):
        return _lib.mini_sandbox_allow_all_domains()
    return FEATURE_NOT_AVAILABLE

def mini_sandbox_allow_ipv4_subnet(subnet):
    if _lib is None:
        return LIB_NOT_LOADED

    if _tap and hasattr(_lib, "mini_sandbox_allow_ipv4_subnet"):
        return _lib.mini_sandbox_allow_ipv4_subnet(subnet.encode())
    return FEATURE_NOT_AVAILABLE

