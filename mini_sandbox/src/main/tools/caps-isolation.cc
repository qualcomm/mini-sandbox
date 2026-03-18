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
#include "src/main/tools/caps-isolation.h"
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
#include <sys/prctl.h>
#include <linux/capability.h>
#include <sys/syscall.h>


static void DropCapabilitiesExcept(uint64_t keep) {
  struct __user_cap_header_struct hdr = {
    .version = CAP_VERSION,
    .pid = 0,
  };
  struct __user_cap_data_struct data[CAP_WORDS];
  int i;

  if (syscall(SYS_capget, &hdr, data))
    MiniSbxReportError(ErrorCode::SysCapget);

  for (i = 0; i < CAP_WORDS; i++) {
    uint32_t mask = (uint32_t)(keep >> (32 * i));

    data[i].effective &= mask;
    data[i].permitted &= mask;
    data[i].inheritable &= mask;
  }

  if (syscall(SYS_capset, &hdr, data))
    MiniSbxReportError(ErrorCode::SysCapset);
}


void DropCapabilities() {
  std::cout << "Warning: Sandbox cannot be fully enabled (either due to Docker or AppArmor). "
          "We'll just drop the capabilities of the current process but cannot provide advanced "
          "features such as usernamespace, overlayfs, rootless firewall, etc." << std::endl;

  uint64_t keep = 0;
  // TODO: Add either a CLI flag/API or env variable to configure capabilities
  DropCapabilitiesExcept(keep);
  return;
}


int CapsRunTime() {

  DropCapabilities();
  if (prctl(PR_SET_NO_NEW_PRIVS, 1, 0, 0, 0) < 0) {
      return MiniSbxReportError(ErrorCode::PRSetNoNewPrivsFail);
  } 
  return 0;

}

int CapsRunTimeCLI() {
  PRINT_DEBUG("Start capabilities CLI isolation\n");
  int res = CapsRunTime();
  if (res == 0) 
    SpawnChild(true);
  return res;
}

int CapsRunTimeLib() {
  PRINT_DEBUG("Start capabilities LIB isolation");
  return CapsRunTime();
}

