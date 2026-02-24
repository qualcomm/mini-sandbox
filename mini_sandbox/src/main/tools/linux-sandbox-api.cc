/*
 * Copyright (c) 2025 Qualcomm Technologies, Inc. and/or its subsidiaries.
 * SPDX-License-Identifier: MIT
 */
#if LIBMINISANDBOX

#include "linux-sandbox-api.h"
#include "error-handling.h"
#include "src/main/tools/linux-sandbox-options.h"
#include "src/main/tools/linux-sandbox.h"


static bool already_started=false;


  //This function returns true if mini_sandbox is already running. Either because one of the parent processes already started it or because the app already invoked mini_sandbox_start() 
bool mini_sandbox_is_running(){
  if(already_started){
    return true;
  }else{
    return MiniSbxIsNestedSandbox();
  }
}

int mini_sandbox_get_last_error_code() {
  return MiniSbxGetErrorCode();
}


const char* mini_sandbox_get_last_error_msg() {
  return MiniSbxGetErrorMsg();
}


int mini_sandbox_enable_log(const char* path) {
  if(already_started){
    return MiniSbxReportError(ErrorCode::SandboxAlreadyStarted);
  }
  return MiniSbxEnableLog(path);
}


int mini_sandbox_setup_default() {
  if(already_started){
    return MiniSbxReportError(ErrorCode::SandboxAlreadyStarted);
  }
  return MiniSbxSetupDefault();
}


int mini_sandbox_setup_custom(const char* overlayfs_dir, const char* sandbox_root) {
  if(already_started){
    return MiniSbxReportError(ErrorCode::SandboxAlreadyStarted);
  }
  return MiniSbxSetupCustom(overlayfs_dir, sandbox_root);
}


int mini_sandbox_setup_hermetic(const char* sandbox_root) {
  if(already_started){
    return MiniSbxReportError(ErrorCode::SandboxAlreadyStarted);
  }
  return MiniSbxSetupHermetic(sandbox_root);
}


int mini_sandbox_start() {
  if(already_started){
    return MiniSbxReportError(ErrorCode::SandboxAlreadyStarted);
  }
  already_started=true;
  return MiniSbxStart();
}


int mini_sandbox_set_working_dir(const char* path) {
  return MiniSbxSetWorkingDir(path);
}

int mini_sandbox_mount_bind(const char* path) {
  if(already_started){
    return MiniSbxReportError(ErrorCode::SandboxAlreadyStarted);
  }
  return MiniSbxMountBind(path);
}


int mini_sandbox_mount_write(const char* path) {
  if(already_started){
    return MiniSbxReportError(ErrorCode::SandboxAlreadyStarted);
  }
  return MiniSbxMountWrite(path);
}


int mini_sandbox_mount_tmpfs(const char* path) {
  if(already_started){
    return MiniSbxReportError(ErrorCode::SandboxAlreadyStarted);
  }
  return MiniSbxMountTmpfs(path);
}


int mini_sandbox_mount_overlay(const char* path) {
  if(already_started){
    return MiniSbxReportError(ErrorCode::SandboxAlreadyStarted);
  }
  return MiniSbxMountOverlay(path);
}


int mini_sandbox_mount_empty_output_file(const char* path) {
  if(already_started){
    return MiniSbxReportError(ErrorCode::SandboxAlreadyStarted);
  }
  return MiniSbxMountEmptyOutputFile(path);
}

int mini_sandbox_mount_parents_write() {
  if(already_started){
    return MiniSbxReportError(ErrorCode::SandboxAlreadyStarted);
  }
  return MiniSbxMountParentsWrite();
}

#ifndef MINITAP
int mini_sandbox_share_network() {
  if(already_started){
    return MiniSbxReportError(ErrorCode::SandboxAlreadyStarted);
  }
  return MiniSbxShareNetNamespace();
}
#endif

#ifdef MINITAP
int mini_sandbox_allow_max_connections(int max_connections) {
  if(already_started){
    return MiniSbxReportError(ErrorCode::SandboxAlreadyStarted);
  }
  return MiniSbxAllowMaxConnections(max_connections);
}

int mini_sandbox_allow_connections(const char* path) {
  if(already_started){
    return MiniSbxReportError(ErrorCode::SandboxAlreadyStarted);
  }
  return MiniSbxAllowConnections(path);
}


int mini_sandbox_allow_ipv4(const char* ip) {
  if(already_started){
    return MiniSbxReportError(ErrorCode::SandboxAlreadyStarted);
  }
  return MiniSbxAllowIpv4(ip);
}


int mini_sandbox_allow_domain(const char* domain) {
  if(already_started){
    return MiniSbxReportError(ErrorCode::SandboxAlreadyStarted);
  }
  return MiniSbxAllowDomain(domain);
}


int mini_sandbox_allow_all_domains() {
  if(already_started){
    return MiniSbxReportError(ErrorCode::SandboxAlreadyStarted);
  }
  return MiniSbxAllowAllDomains();
}


int mini_sandbox_allow_ipv4_subnet(const char* subnet) {
  if(already_started){
    return MiniSbxReportError(ErrorCode::SandboxAlreadyStarted);
  }
  return MiniSbxAllowIpv4Subnet(subnet);
}
#endif

#endif
