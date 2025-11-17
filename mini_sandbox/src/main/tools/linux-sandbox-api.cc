#ifdef LIBMINISANDBOX

#include "linux-sandbox-api.h"
#include "error_handling.h"
#include "src/main/tools/linux-sandbox-options.h"
#include "src/main/tools/linux-sandbox.h"

int mini_sandbox_get_last_error_code() {
  return MiniSbxGetErrorCode();
}


const char* mini_sandbox_get_last_error_msg() {
  return MiniSbxGetErrorMsg();
}


int mini_sandbox_enable_log(const std::string& path) {
  return MiniSbxEnableLog(path);
}


int mini_sandbox_setup_default() {
  return MiniSbxSetupDefault();
}


int mini_sandbox_setup_custom(const std::string& overlayfs_dir, const std::string& sandbox_root) {
  return MiniSbxSetupCustom(overlayfs_dir, sandbox_root);
}


int mini_sandbox_setup_hermetic(const std::string& sandbox_root) {
  return MiniSbxSetupHermetic(sandbox_root);
}


int mini_sandbox_start() {
  return MiniSbxStart();
}


int mini_sandbox_mount_bind(const std::string& path) {
  return MiniSbxMountBind(path);
}


int mini_sandbox_mount_write(const std::string& path) {
  return MiniSbxMountWrite(path);
}


int mini_sandbox_mount_tmpfs(const std::string& path) {
  return MiniSbxMountTmpfs(path);
}


int mini_sandbox_mount_overlay(const std::string& path) {
  return MiniSbxMountOverlay(path);
}


int mini_sandbox_mount_empty_output_file(const std::string& path) {
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
int mini_sandbox_allow_connections(const std::string& path) {
  return MiniSbxAllowConnections(path);
}


int mini_sandbox_allow_ipv4(const std::string& ip) {
  return MiniSbxAllowIpv4(ip);
}


int mini_sandbox_allow_domain(const std::string& domain) {
  return MiniSbxAllowDomain(domain);
}


int mini_sandbox_allow_all_domains() {
  return MiniSbxAllowAllDomains();
}


int mini_sandbox_allow_ipv4_subnet(const std::string& subnet) {
  return MiniSbxAllowIpv4Subnet(subnet);
}
#endif

#endif
