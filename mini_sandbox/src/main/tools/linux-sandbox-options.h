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

#ifndef SRC_MAIN_TOOLS_LINUX_SANDBOX_OPTIONS_H_
#define SRC_MAIN_TOOLS_LINUX_SANDBOX_OPTIONS_H_

#include <stdbool.h>
#include <stddef.h>

#include <string>
#include <vector>

#ifdef MINITAP
#include "firewall.h"
#endif

#define TMP "/tmp"
#define MINI_SBX_TMP "mini-sandbox-tmp"

enum NetNamespaceOption {NETNS_WITH_LOOPBACK,  NO_NETNS, NETNS};

enum DockerMode {NO_CONTAINER, UNPRIVILEGED_CONTAINER, PRIVILEGED_CONTAINER};
extern DockerMode docker_mode;

// Options parsing result.
struct Options {
  // Working directory (-W)
  std::string working_dir;
  // How long to wait before killing the child (-T)
  int timeout_secs;
  // How long to wait before sending SIGKILL in case of timeout (-t)
  int kill_delay_secs;
  // Send a SIGTERM to the child on receipt of a SIGINT (-i)
  bool sigint_sends_sigterm;
  // Files or directories to make writable for the sandboxed process (-w)
  std::vector<std::string> writable_files;
  // Directories where to mount an empty tmpfs (-e)
  std::vector<std::string> tmpfs_dirs;
  // Source of files or directories to explicitly bind mount in the sandbox (-M)
  std::vector<std::string> bind_mount_sources;
  // Target of files or directories to explicitly bind mount in the sandbox (-m)
  std::vector<std::string> bind_mount_targets;
  // Set the hostname inside the sandbox to 'localhost' (-H)
  bool fake_hostname;
  // Create a new network namespace (-n/-N)
  NetNamespaceOption create_netns;
  // Pretend to be root inside the namespace (-R)
  bool fake_root;
  // Set the username inside the sandbox to 'nobody' (-U)
  bool fake_username;
  // Enable writing to /dev/pts and map the user's gid to tty to enable
  // pseudoterminals (-P)
  bool enable_pty;
  // Print debugging messages (-D)
  std::string debug_path;
  // Improved hermetic build using whitelisting strategy (-h)
  bool hermetic;
  // The sandbox root directory (-s)
  std::string sandbox_root;
  //enable overlayfs
  bool use_overlayfs;
  //temporary dir for the overlayfs
  std::string tmp_overlayfs;
  //Vector of paths for the overlayfs
  std::vector<std::string> overlayfsmount;
  // Command to run (--)
  std::vector<char *> args;
  // default setup for Android builds
  bool use_default;
  // if we are running in docker or not
  bool docker = false;
  // when using the overlay we can either mount the folders parents of CWD 
  // in the overlay or let them read/write. This field defaults to overlay 
  // but can be enabled via API to mount the parents of CWD as write until
  // the mount point
  bool parents_writable = false;
  // path to firewall rules if tap mode is enabled
#ifdef MINITAP
  std::string firewall_rules_path;
  FirewallRules fw_rules;
#endif
};

extern struct Options opt;

// Handles parsing all command line flags and populates the global opt struct.
void ParseOptions(int argc, char *argv[]);
void SetupDefaultMounts(std::vector<std::string>& bind_mount_sources, std::vector<std::string>& bind_mount_targets);
int ValidateOverlayOutOfFolder(const std::string& overlay_dir, const std::string& sandbox_dir);
int ValidateReadWritePaths(std::vector<std::string>& readables, std::vector<std::string>& writables);
int ValidateTmpNotRemounted(std::vector<std::string>& paths);

// Internal APIs
int MiniSbxEnableLog(const std::string &path);
int MiniSbxSetupDefault();
int MiniSbxSetupCustom(const std::string &overlayfs_dir, const std::string& sandbox_root);
int MiniSbxSetupHermetic(const std::string& sandbox_root);
int MiniSbxMountBind(const std::string& path);
int MiniSbxMountWrite(const std::string& path);
int MiniSbxMountTmpfs(const std::string& path);
int MiniSbxMountOverlay(const std::string& path);
int MiniSbxMountEmptyOutputFile(const std::string& path);
int MiniSbxMountParentsWrite();

#ifndef MINITAP
int MiniSbxShareNetNamespace() ;
#endif

#ifdef MINITAP
int MiniSbxAllowConnections(const std::string& path);
int MiniSbxAllowMaxConnections(int max_connections);
int MiniSbxAllowIpv4(const std::string& ip);
int MiniSbxAllowDomain(const std::string& domain);
int MiniSbxAllowAllDomains();
int MiniSbxAllowIpv4Subnet(const std::string& subnet);
#endif


#endif
