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
#include "src/main/tools/logging.h"
#include "src/main/tools/minitap-interface.h"
#include "src/main/tools/process-tools.h"
#include "src/main/tools/error-handling.h"
#include "src/main/tools/firewall.h"
#include "src/main/tools/constants.h"
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


#include <linux/landlock.h>
#include <sys/syscall.h>

std::set<std::string> ReadOnlyPaths;

static int SysLandlockCreateRuleset(const struct landlock_ruleset_attr* attr,
                                   size_t size, uint32_t flags) {
  return (int)syscall(SYS_landlock_create_ruleset, attr, size, flags);
}

static int SysLandlockAddRule(int ruleset_fd, enum landlock_rule_type rule_type,
                              const void* rule_attr, uint32_t flags) {
  return (int)syscall(SYS_landlock_add_rule, ruleset_fd, rule_type, rule_attr, flags);
}

static int SysLandlockRestrictSelf(int ruleset_fd, uint32_t flags) {
  return (int)syscall(SYS_landlock_restrict_self, ruleset_fd, flags);
}


static __u64 AccessRO() {
  return LANDLOCK_ACCESS_FS_READ_FILE |
         LANDLOCK_ACCESS_FS_READ_DIR |
         LANDLOCK_ACCESS_FS_EXECUTE;
}

static __u64 AccessRW() {
  return AccessRO() |
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
}


static int LLGetAbi() {
  int abi = SysLandlockCreateRuleset(NULL, 0, LANDLOCK_CREATE_RULESET_VERSION);

  printf("Landlock ABI: %d\n", abi);
  if (abi < 0) {
  	const int err = errno;
  
  	perror("Failed to check Landlock compatibility");
  	switch (err) {
  	case ENOSYS:
  		fprintf(stderr,
  			"Hint: Landlock is not supported by the current kernel. "
  			"To support it, build the kernel with "
  			"CONFIG_SECURITY_LANDLOCK=y and prepend "
  			"\"landlock,\" to the content of CONFIG_LSM.\n");
  		break;
  	case EOPNOTSUPP:
  		fprintf(stderr,
  			"Hint: Landlock is currently disabled. "
  			"It can be enabled in the kernel configuration by "
  			"prepending \"landlock,\" to the content of CONFIG_LSM, "
  			"or at boot time by setting the same content to the "
  			"\"lsm\" kernel parameter.\n");
  		break;
  	}
  	return 1;
  }
  return abi;
}

static int CreateBasicRulesetFd() {
  struct landlock_ruleset_attr ruleset_attr = {};
  ruleset_attr.handled_access_fs =
      LANDLOCK_ACCESS_FS_EXECUTE |
      LANDLOCK_ACCESS_FS_WRITE_FILE |
      LANDLOCK_ACCESS_FS_READ_FILE |
      LANDLOCK_ACCESS_FS_READ_DIR |
      LANDLOCK_ACCESS_FS_REMOVE_DIR |
      LANDLOCK_ACCESS_FS_REMOVE_FILE |
      LANDLOCK_ACCESS_FS_MAKE_CHAR |
      LANDLOCK_ACCESS_FS_MAKE_DIR |
      LANDLOCK_ACCESS_FS_MAKE_REG |
      LANDLOCK_ACCESS_FS_MAKE_SOCK |
      LANDLOCK_ACCESS_FS_MAKE_FIFO |
      LANDLOCK_ACCESS_FS_MAKE_BLOCK |
      LANDLOCK_ACCESS_FS_MAKE_SYM |
      LANDLOCK_ACCESS_FS_REFER |
      LANDLOCK_ACCESS_FS_TRUNCATE;

  // TODO Handle network in Network subclasses 
  // ruleset_attr.handled_access_net = 0;
  // ruleset_attr.scoped = 0;

  int fd = SysLandlockCreateRuleset(&ruleset_attr, sizeof(ruleset_attr), 0);
  if (fd < 0) return -errno;
  return fd;
}



static int OpenPathFd(const std::string& path) {
  int fd = open(path.c_str(), O_PATH | O_CLOEXEC);
  return fd;
}



static int AddPathRule(int ruleset_fd, const std::string& path, __u64 allowed_access) {
 
  int fd = OpenPathFd(path);
  if (fd < 0) {
    return -1;
  }

  struct landlock_path_beneath_attr rule = {};
  rule.allowed_access = allowed_access;
  rule.parent_fd = fd;

  int rc = SysLandlockAddRule(ruleset_fd, LANDLOCK_RULE_PATH_BENEATH, &rule, 0);
  int saved_errno = errno;
  close(fd);

  if (rc < 0) return -saved_errno;
  return 0;
}


static int LLMapReadOnlyPaths(int ruleset_fd) {  
  int rc = 0;
  for (const auto& p : opt.bind_mount_sources) {
    printf("Mapping RO %s\n", p.c_str());
    rc = AddPathRule(ruleset_fd, p, AccessRO());
    if (rc < 0) { close(ruleset_fd); return rc; }
  }
  
  for (const auto& p : ReadOnlyPaths) {
    printf("Mapping (internal) RO %s\n", p.c_str());
    rc = AddPathRule(ruleset_fd, p, AccessRO());
    if (rc < 0) { close(ruleset_fd); return rc; }

  }
  return rc;
}

static int LLMapReadWritePaths(int ruleset_fd) { 
  int rc = 0;
  for (const auto& p : opt.writable_files) {

    printf("Mapping RW %s\n", p.c_str());
    rc = AddPathRule(ruleset_fd, p, AccessRW());
    if (rc < 0) { close(ruleset_fd); return rc; }
  }
  return rc;
}

static int MapWorkingDirMountPoint(const std::string& mount_point) {
  int res = 0;
  std::string home_dir = GetHomeDir();
  std::string top_level = GetTopLevelFolder(mount_point,home_dir);
  if (top_level.empty()) 
    return -1;
  if (opt.parents_writable)
    res = MiniSbxMountWrite(top_level);
  else 
    res = MiniSbxMountBind(top_level);
  printf("Top level: %s\n", top_level.c_str());
  return res;
}

static int MapAllFilesystem(const int ruleset_fd) {
  int res = LLMapReadOnlyPaths(ruleset_fd);
  res += LLMapReadWritePaths(ruleset_fd); 
  return res;
}

static int LLRunTime() {
  int abi = LLGetAbi() ;
  printf("Final ABI: %d\n", abi);

  const int ruleset_fd = CreateBasicRulesetFd();
  int res = 0;
  if (ruleset_fd < 0) return ruleset_fd;

  if (opt.use_default) {
    const std::string mount_point = GetMountPointOf(opt.working_dir);
    MiniSbxMountWrite(kTmp);
    MiniSbxMountWrite(opt.working_dir);
    MapWorkingDirMountPoint(mount_point);
    AddLeftoverFoldersToReadOnlyPaths();
    MapAllFilesystem(ruleset_fd);

  } else {
    
    res = LLMapReadOnlyPaths(ruleset_fd);
    
    res += LLMapReadWritePaths(ruleset_fd);

    printf("After mapping paths get res == %d\n", res);
  }
 
  if (prctl(PR_SET_NO_NEW_PRIVS, 1, 0, 0, 0)) {
  	perror("Failed to restrict privileges");
  	return -1;
  } 
 
  if (SysLandlockRestrictSelf(ruleset_fd, 0) < 0) {
    int rc = -errno;
    perror("Failure RestrictSelf");
    close(ruleset_fd);
    return rc;
  }
  printf("AAAAAAAAAAAAAAAAAA\n");
   
  close(ruleset_fd);
  return 0;

}


int LLRunTimeCLI() {
  printf("Start landlock CLI isolation\n");
  int res = LLRunTime();
  if (res < 0) 
    return res;
  opt.args.push_back(nullptr);
  if (execvp(opt.args[0], opt.args.data()) < 0) {
    perror("execvp");  
    return -1;
  }
  return res;
}

int LLRunTimeLib() {
  printf("Start landlock LIB isolation");
  return LLRunTime();
}


