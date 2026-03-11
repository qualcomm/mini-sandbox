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
#include "src/main/tools/logging.h"
#include "src/main/tools/minitap-interface.h"
#include "src/main/tools/process-tools.h"
#include "src/main/tools/error-handling.h"
#include "src/main/tools/firewall.h"
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

pid_t initial_ppid;

// Make sure the child process does not inherit any accidentally left open file
// handles from our parent.
static void CloseFds() {
  DIR *fds = opendir("/proc/self/fd");
  if (fds == nullptr) {
    MiniSbxReportGenericError("opendir");
  }

  while (1) {
    errno = 0;
    struct dirent *dent = readdir(fds);

    if (dent == nullptr) {
      if (errno != 0) {
        MiniSbxReportGenericError("readdir");
      }
      break;
    }

    if (isdigit(dent->d_name[0])) {
      errno = 0;
      int fd = strtol(dent->d_name, nullptr, 10);

      // (1) Skip unparseable entries.
      // (2) Close everything except stdin, stdout, stderr and debug output.
      // (3) Do not accidentally close our directory handle.
      if (errno == 0 && fd > STDERR_FILENO &&
          (global_debug == NULL || fd != fileno(global_debug)) &&
          fd != dirfd(fds)) {
        if (close(fd) < 0) {
          MiniSbxReportGenericError("close");
        }
      }
    }
  }

  if (closedir(fds) < 0) {
    MiniSbxReportGenericError("closedir");
  }
}


// CLI RunTime specific methods
int MiniSbxCLIRunTime::HandleUnprivilegedContainer()  {
  // Keep current behavior: Spawn a new child (typically doesn't return).
  SpawnChild(false);
  return 0; 
}

void MiniSbxCLIRunTime::HandleSignals() {
  ClearSignalMask();
  IgnoreSignal(SIGTTIN);
  IgnoreSignal(SIGTTOU);
  initial_ppid = getppid();
}

void MiniSbxCLIRunTime::MaybeCloseFds() {
  CloseFds();
}

int MiniSbxCLIRunTime::RunTime() {
  return isolation().RunIsolation(MiniSbxExecMode::CLI);
}


// LIB RunTime specific methods
int MiniSbxLibRunTime::HandleUnprivilegedContainer() {
  return 0; 
}

void MiniSbxLibRunTime::HandleSignals() {}

void MiniSbxLibRunTime::MaybeCloseFds() {}

int MiniSbxLibRunTime::RunTime() {
  return isolation().RunIsolation(MiniSbxExecMode::LIB);
}

// RunNet() is the only common method as internally it calls RunNetwork() 
// from MiniSbxNetwork
int MiniSbxRunTime::RunNet() {
  return network().RunNetwork();
}
