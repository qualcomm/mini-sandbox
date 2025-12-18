/*
 * Copyright (c) 2025 Qualcomm Technologies, Inc. and/or its subsidiaries.
 * SPDX-License-Identifier: MIT
 */

// This header contains the APIs for C++ only and thus we can 
// use std::string and iostream


#ifndef SRC_MAIN_TOOLS_LINUX_SANDBOX_API_H_
#define SRC_MAIN_TOOLS_LINUX_SANDBOX_API_H_

#ifdef LIBMINISANDBOX

#include <iostream>

// These APIs select a functioning mode and set up the configuration accordingly, in case 
// we need to pivot to a chroot or if we need a folder where to store the overlayfs. 
// Most of the cases you want to use `mini_sandbox_setup_default()` but other modes 
// are still available for custom scenarios
int mini_sandbox_setup_default();
int mini_sandbox_setup_custom(const std::string& overlayfs_dir, const std::string& sandbox_root);
int mini_sandbox_setup_hermetic(const std::string& sandbox_root);

// This will parse all input configurations/options, will unshare() and fork the new
// namespace'd process.
int mini_sandbox_start();

// APIs to mount a certain path as a Bind, Write (this will have output out of the sandbox)
// Read-only (Usually the default) or Overlay
int mini_sandbox_mount_bind(const std::string& path);
int mini_sandbox_mount_write(const std::string& path);
int mini_sandbox_mount_tmpfs(const std::string& path);
int mini_sandbox_mount_overlay(const std::string& path);
int mini_sandbox_mount_empty_output_file(const std::string& path);
int mini_sandbox_mount_parents_write();

// Enables logging at a certain path. Path has to be an existing folder
int mini_sandbox_enable_log(const std::string& path);

// Returns error code and error messages
int mini_sandbox_get_last_error_code();
const char* mini_sandbox_get_last_error_msg();

#ifndef MINITAP
int mini_sandbox_share_network();
#endif



#ifdef MINITAP
int mini_sandbox_allow_connections(const std::string& path);
int mini_sandbox_allow_max_connections(int max_connections);
int mini_sandbox_allow_ipv4(const std::string& ip);
int mini_sandbox_allow_domain(const std::string& domain);
int mini_sandbox_allow_all_domains();
int mini_sandbox_allow_ipv4_subnet(const std::string& subnet);
#endif

#endif

#endif
