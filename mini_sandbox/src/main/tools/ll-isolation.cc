/*
 * Copyright (c) 2025 Qualcomm Technologies, Inc. and/or its subsidiaries.
 * SPDX-License-Identifier: MIT
 */

#include "src/main/tools/linux-sandbox.h"
#include "src/main/tools/docker-support.h"
#include "src/main/tools/linux-sandbox-options.h"
#include "src/main/tools/linux-sandbox-pid1.h"
#include "src/main/tools/linux-sandbox-runtime.h"
#include "src/main/tools/linux-sandbox-network.h"
#include "src/main/tools/linux-sandbox-isolation.h"
#include "src/main/tools/ns-isolation.h"
#include "src/main/tools/ll-isolation.h"
#include "src/main/tools/logging.h"
#include "src/main/tools/minitap-interface.h"
#include "src/main/tools/process-tools.h"
#include "src/main/tools/error-handling.h"
#include "src/main/tools/firewall.h"
#include "src/main/tools/constants.h"

#include "landlock_compat.h"

#include <ctype.h>
#include <dirent.h>
#include <errno.h>
#include <fcntl.h>
#include <math.h>
#include <sched.h>
#include <signal.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/prctl.h>
#include <sys/resource.h>
#include <sys/stat.h>
#include <sys/time.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <unistd.h>
#include <grp.h>

#if __has_include(<filesystem>)
#include <filesystem>
namespace fs = std::filesystem;
#else
#include <experimental/filesystem>
namespace fs = std::experimental::filesystem;
#define _EXPERIMENTAL_FILESYSTEM_
#endif

#include <atomic>
#include <iostream>
#include <string>
#include <system_error>
#include <vector>
#include <chrono>
#include <cstring>
#include <dirent.h>
#include <numeric>
#include <stdexcept>
#include <thread>
#include <mntent.h>
#include <sys/syscall.h>

std::set<std::string> ReadOnlyPaths;
static int gRuleset = -1;
static int gABI = -1;

static int SysLandlockCreateRuleset(const struct landlock_ruleset_attr* attr,
                                   size_t size, uint32_t flags) {
  return ll_landlock_create_ruleset(attr, size, flags);
}

static int SysLandlockAddRule(int ruleset_fd, enum landlock_rule_type rule_type,
                              const void* rule_attr, uint32_t flags) {
  int res = ll_landlock_add_rule(ruleset_fd, rule_type, rule_attr, flags);
  return res;
}

static int SysLandlockRestrictSelf(int ruleset_fd, uint32_t flags) {
  return ll_landlock_restrict_self(ruleset_fd, flags);
}

static int LandlockProbeABI() {
  return ll_landlock_probe_abi();
}

static inline __u64 ABIMask() {
  return ll_supported_fs_mask_for_abi(gABI);
}


static inline __u64 AccessRO() {
  __u64 access = 
         LANDLOCK_ACCESS_FS_READ_FILE |
         LANDLOCK_ACCESS_FS_READ_DIR |
         LANDLOCK_ACCESS_FS_EXECUTE;
  return access & ABIMask();
}

static inline __u64 AccessRW() {
 __u64 access = AccessRO() |
         LANDLOCK_ACCESS_FS_WRITE_FILE |
         LANDLOCK_ACCESS_FS_TRUNCATE |
         LANDLOCK_ACCESS_FS_REMOVE_FILE |
         LANDLOCK_ACCESS_FS_REMOVE_DIR |
         LANDLOCK_ACCESS_FS_MAKE_REG |
         LANDLOCK_ACCESS_FS_MAKE_DIR |
         LANDLOCK_ACCESS_FS_MAKE_SYM |
         LANDLOCK_ACCESS_FS_MAKE_FIFO |
         LANDLOCK_ACCESS_FS_MAKE_SOCK |
         LANDLOCK_ACCESS_FS_MAKE_CHAR |
         LANDLOCK_ACCESS_FS_MAKE_BLOCK |
         LANDLOCK_ACCESS_FS_REFER;
  return access & ABIMask();
}


static inline __u64 AccessFile() {
  __u64 access =
         LANDLOCK_ACCESS_FS_EXECUTE |
         LANDLOCK_ACCESS_FS_WRITE_FILE |
         LANDLOCK_ACCESS_FS_READ_FILE |
         LANDLOCK_ACCESS_FS_TRUNCATE |
         LANDLOCK_ACCESS_FS_IOCTL_DEV;
  return access & ABIMask();
}

static int AddTcpRule(int ruleset_fd, uint64_t port) {
  struct landlock_net_port_attr rule = {
    .allowed_access = LANDLOCK_ACCESS_NET_CONNECT_TCP,
    .port = port,
  };

  if (SysLandlockAddRule(ruleset_fd, LANDLOCK_RULE_NET_PORT, &rule, 0) < 0)
    return -1;
  return 0;
}

static int MapPorts(int ruleset_fd, const uint16_t* ports, size_t num_ports) { 
  int res = 0;
  if (!ports && num_ports != 0)
    return -1;
  
  for (size_t i = 0; i < num_ports; ++i) {
    res += AddTcpRule(ruleset_fd, (uint64_t)ports[i]);
  }
  
  return res;
}

static int CreateBasicRulesetFd() {
  if (gABI < 0)
    gABI = LandlockProbeABI();

  struct landlock_ruleset_attr ruleset_attr = {};
  ruleset_attr.handled_access_fs = ll_supported_fs_mask_for_abi(gABI);

  // We set up the network policy here. We can have three distinct cases
  // Case1: User wants to have a firewall like behavior as we used to have 
  // with minitap. For now the best effort is to allow only a subset of TCP
  // ports
  if (opt.fw_rules.mode == FirewallMode::FirewallEnabled) {
    ruleset_attr.handled_access_net = ll_supported_net_mask_for_abi(gABI);
  }
  // Case2: We don't add any ports' rule meaning that all TCP ports are denied
  else if (opt.create_netns == NETNS_WITH_LOOPBACK || opt.create_netns == NETNS) {
    ruleset_attr.handled_access_net = ll_supported_net_mask_for_abi(gABI);
  }
  // Case3: We don't add any network restriction meaning that all connections are
  // allowed
  else
    ruleset_attr.handled_access_net = 0;

  ruleset_attr.scoped = ll_supported_scope_mask_for_abi(gABI);

  int fd = SysLandlockCreateRuleset(&ruleset_attr, sizeof(ruleset_attr), 0);

  if (fd < 0) return -errno;
  return fd;
}


static int AddPathRule(int ruleset_fd, const std::string& path, __u64 allowed_access) {
 
  int fd = -1;
  bool is_dir = IsDir(path.c_str(), &fd);
  if (fd < 0) {
    return -1;
  }

  struct landlock_path_beneath_attr rule = {};
  rule.allowed_access = allowed_access;
  rule.parent_fd = fd;

  if (!is_dir) {
    rule.allowed_access &= AccessFile();
  }

  int rc = SysLandlockAddRule(ruleset_fd, LANDLOCK_RULE_PATH_BENEATH, &rule, 0);
  int saved_errno = errno;
  close(fd);

  if (rc < 0) return -saved_errno;
  return 0;
}





static int MapReadOnlyPath(const std::string& path) {
  int res = AddPathRule(gRuleset, path, AccessRO());  
  PRINT_DEBUG("Map %s as RO == %d", path.c_str(), res);
  return res;
}


static int MapReadWritePath(const std::string& path) {
  int res = AddPathRule(gRuleset, path, AccessRW());  
  PRINT_DEBUG("Map %s as RW == %d", path.c_str(), res);
  return res;
}


static int LLMapReadOnlyPaths() {  
  int rc = 0;
  PRINT_DEBUG("Mount bind_mounts");
  for (const auto& p : opt.bind_mount_sources) {
    rc += MapReadOnlyPath(p);
  }
  
  PRINT_DEBUG("Mount internal read_only");
  for (const auto& p : ReadOnlyPaths) {
    rc += MapReadOnlyPath(p);
  }
  return rc;
}



static int LLMapReadWritePaths() { 
  int rc = 0;
  for (const auto& p : opt.writable_files) {
    rc += MapReadWritePath(p);
  }
  return rc;
}

static int MapWorkingDirMountPoint(const std::string& mount_point) {
  int res = 0;
  std::string home_dir = GetHomeDir();
  std::string top_level = GetTopLevelFolder(mount_point, home_dir, opt.working_dir);
  if (top_level.empty()) 
    return -1;
  if (opt.parents_writable)
    res = MiniSbxMountWrite(top_level);
  else 
    res = MiniSbxMountBind(top_level);
  PRINT_DEBUG("Top level: %s\n", top_level.c_str());
  return res;
}

static int MapAllFilesystem() {
  int res = LLMapReadOnlyPaths();
  res += LLMapReadWritePaths(); 
  return res;
}


static void MapDev() {
  struct stat st;
  int i = 0;
  for (i = 0; ll_devs[i] != NULL; i++) {
    if (stat(ll_devs[i], &st) != 0) 
      continue;
    MapReadWritePath(ll_devs[i]);
  }
  for (i = 0; devs[i] != NULL; i++) {
    if (stat(devs[i], &st) != 0)
      continue;
    MapReadWritePath(devs[i]);
  }
  for (i = 0; i < DEV_LINKS; i++) {
    std::string abs = links[i].link_path;   
    if (!abs.empty() && abs.front() != '/') {
      abs.insert(abs.begin(), '/');      
      MapReadWritePath(abs);
    }
  }
}


static int MapFilesystemPartiallyReadOnly() {
  int res = 0;
  FILE *mounts = setmntent(kMounts, "r");  
  if (mounts == nullptr) { 
    return MiniSbxReportGenericError("setmntent failed");
  }

  struct mntent* ent;
  while ((ent = getmntent(mounts)) != nullptr) {
    if (EndsWith(ent->mnt_dir, kProc))    
      continue;

    if (!ShouldBeWritable(ent->mnt_dir) ) {
      res += MapReadOnlyPath(ent->mnt_dir);
    }
    else {
      res += MapReadWritePath(ent->mnt_dir);
    }
  }
  return res;
}

static int LLRunTime() {

  if (! LandlockSupported() ) {
    return MiniSbxReportError(ErrorCode::LLNotSupported);
  }

  gRuleset = CreateBasicRulesetFd();
  int res = 0;
  if (gRuleset < 0) return gRuleset;

  if (opt.use_default) {
    const std::string mount_point = GetMountPointOf(opt.working_dir);
    MiniSbxMountWrite(kTmp);
    MiniSbxMountWrite(opt.working_dir);
    res += MapWorkingDirMountPoint(mount_point);
    AddLeftoverFoldersToReadOnlyPaths();
    res += MapAllFilesystem();
    MapDev();

  } else if (opt.hermetic || opt.use_overlayfs) {
    res = MapAllFilesystem(); 
  }
  else {
    MiniSbxMountWrite(kTmp);
    MiniSbxMountWrite(opt.working_dir);
    MapFilesystemPartiallyReadOnly();
  }


  if (opt.fw_rules.mode == FirewallMode::FirewallEnabled) {    
    SetPorts(&opt.fw_rules);
    res = MapPorts(gRuleset, opt.fw_rules.ports, opt.fw_rules.ports_count);    
    if (res < 0)
      return MiniSbxReportError(ErrorCode::LLPortsNotSet);
  } 

 
  if (prctl(PR_SET_NO_NEW_PRIVS, 1, 0, 0, 0)) {
  	return MiniSbxReportGenericError("Failed to restrict privileges");
  } 
 
  if (SysLandlockRestrictSelf(gRuleset, 0) < 0) {
    close(gRuleset);
    return MiniSbxReportGenericError("LandlockRestrictSelf failed");
  }
   
  close(gRuleset);
  return res;

}


int LLRunTimeCLI() {
  PRINT_DEBUG("Start landlock CLI isolation\n");
  int res = LLRunTime();

  if (res < 0) 
    return res;
  opt.args.push_back(nullptr);
  if (execvp(opt.args[0], opt.args.data()) < 0) {
    MiniSbxReportGenericError("execvp failed");  
    return -1;
  }
  return res;
}

int LLRunTimeLib() {
  PRINT_DEBUG("Start landlock LIB isolation");
  return LLRunTime();
}


int LandlockSupported() {
  gABI = LandlockProbeABI();
  PRINT_DEBUG("Landlock ABI: %d", gABI);
  return gABI >= MIN_LL_ABI;
}
