#
# Copyright (c) 2025 Qualcomm Technologies, Inc. and/or its subsidiaries.
# SPDX-License-Identifier: MIT
#
import os
import ctypes
import platform

script_dir = os.path.dirname( os.path.abspath(__file__))
libname = os.path.join(script_dir, "libmini-tapbox.so")

try:
    if platform.system() == "Linux":
        lib = ctypes.CDLL(libname)
    else:
        lib = None
except Exception as e:
    print("An exception occured while loading the library" + libname)
    print(e)
    lib = None

    
if lib is not None:
    lib.mini_sandbox_get_last_error_msg.restype = ctypes.c_char_p


def mini_sandbox_get_last_error_code():
    if lib is None:
        return -1
    return lib.mini_sandbox_get_last_error_code()

def mini_sandbox_get_last_error_msg():
    if lib is None:
        return -1
    return lib.mini_sandbox_get_last_error_msg().decode()

def mini_sandbox_enable_log(path):
    if lib is None:
        return -1
    return lib.mini_sandbox_enable_log(path.encode())

def mini_sandbox_setup_default():
    if lib is None:
        return -1
    return lib.mini_sandbox_setup_default()

def mini_sandbox_setup_custom(overlayfs_dir, sandbox_root):
    if lib is None:
       return -1
    return lib.mini_sandbox_setup_custom(overlayfs_dir.encode(), sandbox_root.encode())

def mini_sandbox_mount_parents_write(num_of_parents = -1):
    if lib is None:
        return -1
    if num_of_parents == -1:
        return lib.mini_sandbox_mount_parents_write()
    if num_of_parents > 0:
        upper_lvls = "".join("../" for i in range(num_of_parents))
        parent = os.path.abspath(upper_lvls)
        if parent is not None:
            return lib.mini_sandbox_mount_write(parent.encode())
    return -1

def mini_sandbox_setup_hermetic(sandbox_root):
    if lib is None:
        return -1
    return lib.mini_sandbox_setup_hermetic(sandbox_root.encode())
     
def mini_sandbox_start():
    if lib is None:
        return -1
    return lib.mini_sandbox_start()

def mini_sandbox_mount_bind(path):
    if lib is None:
       return -1
    return lib.mini_sandbox_mount_bind(path.encode())

def mini_sandbox_mount_write(path):
    if lib is None:
        return -1
    return lib.mini_sandbox_mount_write(path.encode())

def mini_sandbox_mount_tmpfs(path):
    if lib is None:
        return -1
    return lib.mini_sandbox_mount_tmpfs(path.encode())

def mini_sandbox_mount_overlay(path):
    if lib is None:
        return -1
    return lib.mini_sandbox_mount_overlay(path.encode())

def mini_sandbox_mount_empty_output_file(path):
    if lib is None:
        return -1
    return lib.mini_sandbox_mount_empty_output_file(path.encode())



def mini_sandbox_allow_connections(path):
    if hasattr(lib, "mini_sandbox_allow_connections"):
        return lib.mini_sandbox_allow_connections(path.encode())
    raise NotImplementedError("Not available in this build")

def mini_sandbox_allow_ipv4(ip):
    if hasattr(lib, "mini_sandbox_allow_ipv4"):
        return lib.mini_sandbox_allow_ipv4(ip.encode())
    raise NotImplementedError("Not available in this build")

def mini_sandbox_allow_domain(domain):
    if hasattr(lib, "mini_sandbox_allow_domain"):
        return lib.mini_sandbox_allow_domain(domain.encode())
    raise NotImplementedError("Not available in this build")

def mini_sandbox_allow_all_domains():
    if hasattr(lib, "mini_sandbox_allow_all_domains"):
        return lib.mini_sandbox_allow_all_domains()
    raise NotImplementedError("Not available in this build")

def mini_sandbox_allow_ipv4_subnet(subnet):
    if hasattr(lib, "mini_sandbox_allow_ipv4_subnet"):
        return lib.mini_sandbox_allow_ipv4_subnet(subnet.encode())
    raise NotImplementedError("Not available in this build")

