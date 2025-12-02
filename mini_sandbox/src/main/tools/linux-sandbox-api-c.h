/*
 * Copyright (c) 2025 Qualcomm Technologies, Inc. and/or its subsidiaries.
 * SPDX-License-Identifier: MIT
 */
// Copyright 2016 The Bazel Authors. All rights reserved.
//
// Licensed under the Apache License, Version 2.0 (the "License");
// you may not use this file except in compliance with the License.
// You may obtain a copy of the License at
//
//    http://www.apache.org/licenses/LICENSE-2.0
//
// Unless required by applicable law or agreed to in writing, software
// distributed under the License is distributed on an "AS IS" BASIS,
// WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
// See the License for the specific language governing permissions and
// limitations under the License.

#ifndef SRC_MAIN_TOOLS_LINUX_SANDBOX_API_H_
#define SRC_MAIN_TOOLS_LINUX_SANDBOX_API_H_

#if LIBMINISANDBOX

#if defined(__cplusplus)
extern "C" {
#endif

// These APIs select a functioning mode and set up the configuration accordingly, in case 
// we need to pivot to a chroot or if we need a folder where to store the overlayfs. 
// Most of the cases you want to use `mini_sandbox_setup_default()` but other modes 
// are still available for custom scenarios
int mini_sandbox_setup_default();
int mini_sandbox_setup_custom(const char* overlayfs_dir, const char* sandbox_root);
int mini_sandbox_setup_hermetic(const char* sandbox_root);

// This will parse all input configurations/options, will unshare() and fork the new
// namespace'd process.
int mini_sandbox_start();

// APIs to mount a certain path as a Bind, Write (this will have output out of the sandbox)
// Read-only (Usually the default) or Overlay
int mini_sandbox_mount_bind(const char* path);
int mini_sandbox_mount_write(const char* path);
int mini_sandbox_mount_tmpfs(const char* path);
int mini_sandbox_mount_overlay(const char* path);
int mini_sandbox_mount_empty_output_file(const char* path);
int mini_sandbox_mount_parents_write();

// Enables logging at a certain path. Path has to be an existing folder
int mini_sandbox_enable_log(const char* path);

// Returns error code and error messages
int mini_sandbox_get_last_error_code();
const char* mini_sandbox_get_last_error_msg();


#ifndef MINITAP
int mini_sandbox_share_network();
#endif



#ifdef MINITAP
int mini_sandbox_allow_connections(const char* path);
int mini_sandbox_allow_ipv4(const char* ip);
int mini_sandbox_allow_domain(const char* domain);
int mini_sandbox_allow_all_domains();
int mini_sandbox_allow_ipv4_subnet(const char* subnet);
#endif

#if defined(__cplusplus)
}
#endif

#endif

#endif
