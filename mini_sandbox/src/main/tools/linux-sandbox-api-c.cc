/*
 * Copyright (c) 2025 Qualcomm Technologies, Inc. and/or its subsidiaries.
 * SPDX-License-Identifier: MIT
 */
#if LIBMINISANDBOX

#include "linux-sandbox-api-c.h"
#include "error_handling.h"
#include "src/main/tools/linux-sandbox-options.h"
#include "src/main/tools/linux-sandbox.h"

int mini_sandbox_get_last_error_code() {
  return MiniSbxGetErrorCode();
}


const char* mini_sandbox_get_last_error_msg() {
  return MiniSbxGetErrorMsg();
}


int mini_sandbox_enable_log(const char* path) {
  return MiniSbxEnableLog(path);
}


int mini_sandbox_setup_default() {
  return MiniSbxSetupDefault();
}


int mini_sandbox_setup_custom(const char* overlayfs_dir, const char* sandbox_root) {
  return MiniSbxSetupCustom(overlayfs_dir, sandbox_root);
}


int mini_sandbox_setup_hermetic(const char* sandbox_root) {
  return MiniSbxSetupHermetic(sandbox_root);
}


int mini_sandbox_start() {
  return MiniSbxStart();
}


int mini_sandbox_mount_bind(const char* path) {
  return MiniSbxMountBind(path);
}


int mini_sandbox_mount_write(const char* path) {
  return MiniSbxMountWrite(path);
}


int mini_sandbox_mount_tmpfs(const char* path) {
  return MiniSbxMountTmpfs(path);
}


int mini_sandbox_mount_overlay(const char* path) {
  return MiniSbxMountOverlay(path);
}


int mini_sandbox_mount_empty_output_file(const char* path) {
  return MiniSbxMountEmptyOutputFile(path);
}

int mini_sandbox_mount_parents_write() {
  return MiniSbxMountParentsWrite();
}

#ifndef MINITAP
int mini_sandbox_share_network() {
  return MiniSbxShareNetNamespace();
}
#endif

#ifdef MINITAP
int mini_sandbox_allow_connections(const char* path) {
  return MiniSbxAllowConnections(path);
}


int mini_sandbox_allow_max_connections(int max_connections) {
  return MiniSbxAllowMaxConnections(max_connections);
}


int mini_sandbox_allow_ipv4(const char* ip) {
  return MiniSbxAllowIpv4(ip);
}


int mini_sandbox_allow_domain(const char* domain) {
  return MiniSbxAllowDomain(domain);
}


int mini_sandbox_allow_all_domains() {
  return MiniSbxAllowAllDomains();
}


int mini_sandbox_allow_ipv4_subnet(const char* subnet) {
  return MiniSbxAllowIpv4Subnet(subnet);
}
#endif

#endif
