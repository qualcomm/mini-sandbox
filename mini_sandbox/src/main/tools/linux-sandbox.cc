
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

/**
 * linux-sandbox runs commands in a restricted environment where they are
 * subject to a few rules:
 *
 *  - The entire filesystem is made read-only.
 *  - The working directory (-W) will be made read-write, though.
 *  - Individual files or directories can be made writable (but not deletable)
 *    (-w).
 *  - If the process takes longer than the timeout (-T), it will be killed with
 *    SIGTERM. If it does not exit within the grace period (-t), it all of its
 *    children will be killed with SIGKILL.
 *  - tmpfs can be mounted on top of existing directories (-e).
 *  - If option -R is passed, the process will run as user 'root'.
 *  - If option -U is passed, the process will run as user 'nobody'.
 *  - Otherwise, the process runs using the current uid / gid.
 *  - If linux-sandbox itself gets killed, the process and all of its children
 *    will be killed.
 *  - If linux-sandbox's parent dies, it will kill itself, the process and all
 *    the children.
 *  - Network access is allowed, but can be disabled via -N.
 *  - The hostname and domainname will be set to "sandbox".
 *  - The process runs in its own PID namespace, so other processes on the
 *    system are invisible.
 */

#include "src/main/tools/linux-sandbox.h"
#include "src/main/tools/docker-support.h"
#include "src/main/tools/linux-sandbox-options.h"
#include "src/main/tools/linux-sandbox-pid1.h"
#include "src/main/tools/linux-sandbox-runtime.h"
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

//#ifdef VERSION
__attribute__((used, section(".version")))
const char build_version[] = VERSION;
//#endif

uid_t global_outer_uid;
gid_t global_outer_gid;

DockerMode docker_mode;



static int ValidateOptions() {
  if (opt.use_overlayfs) {
    if (ValidateOverlayOutOfFolder(opt.tmp_overlayfs, opt.working_dir) < 0)
      return MiniSbxReportError(ErrorCode::IllegalConfiguration);
  }

  if (opt.hermetic ) {
     if (ValidateOverlayOutOfFolder(opt.sandbox_root, opt.working_dir) < 0)
      return MiniSbxReportError(ErrorCode::IllegalConfiguration);
  }
 
  if (ValidateReadWritePaths(opt.bind_mount_sources, opt.writable_files) < 0)
      return MiniSbxReportError(ErrorCode::FileReadAndWrite);

  if (docker_mode != PRIVILEGED_CONTAINER) {
    for (auto writable_file : opt.writable_files) {
      if (opt.use_overlayfs && ValidateOverlayOutOfFolder(opt.tmp_overlayfs, writable_file) < 0)
        return MiniSbxReportError(ErrorCode::IllegalConfiguration);
    }
  }

  // If we are running with opt.use_default == true we'll create overlayfs, 
  // sandbox dir and fake temp dir all under /tmp . Thus we don't want users
  // to add -w /tmp -- If the previous files in /tmp are needed the options
  // are: 
  // 1 - Mount only the file/dir but not the whole /tmp (-w /tmp/needed-dir)
  // 2 - copy the file in /tmp/mini-sandbox-tmp/ before starting the sandbox
  // 3 - instead of running the sandbox with the "Default" (-x) functioning mode
  // use the custom functioning mode with -o/-d
  if (ValidateTmpNotRemounted(opt.writable_files) < 0)
      return MiniSbxReportError(ErrorCode::TmpNotRemounted);

  if (ValidateTmpNotRemounted(opt.bind_mount_sources) < 0)
      return MiniSbxReportError(ErrorCode::TmpNotRemounted);
 
  return 0;
}

static int StartLogging() {
  // Open the file PRINT_DEBUG writes to.
  // Must happen early enough so we don't lose any debugging output.
  if (!opt.debug_path.empty()) {
    global_debug = fopen(opt.debug_path.c_str(), "w");
    if (!global_debug) {
      std::string err_msg = "fopen(" + opt.debug_path + ") failed";
      return MiniSbxReportGenericError(err_msg);
    }
  }
  return 0;
}


static int StartLoggingAndWorkingDir() {
  int res = 0;
  res = StartLogging();
  if (res < 0) return res;

  LogSystem();
  PRINT_DEBUG("UserNamespaceSupported = %d", UserNamespaceSupported());

  // Ask the kernel to kill us with SIGKILL if our parent dies.
  if (prctl(PR_SET_PDEATHSIG, SIGKILL) < 0) {
    MiniSbxReportGenericError("prctl");
  }

  if (opt.working_dir.empty()) {
    char *working_dir = getcwd(nullptr, 0);
    if (working_dir == nullptr) {
      return -1;
    }
    opt.working_dir = std::string(working_dir);
  }
  return res;

}

static int Prepare() {
  int res = ValidateOptions();
  res += MiniSbxCreateInit();
  return res;
}

static void SetGlobalIDs() {
  // Set up two globals used by the child process.
  global_outer_uid = getuid();
  global_outer_gid = getgid();
}

#ifdef MINITAP
static int PrepareMiniTap() {
  int res = 0;
  std::string rules = CreateRandomFilename(std::string("/tmp"));
  DumpRules(&(opt.fw_rules), rules);
  res = RunTCPIP(global_outer_uid, global_outer_gid, rules);
  // In this case the Network namespace has been taken care of by RunTCPIP so
  // we don't need to create a new one
  opt.create_netns = NO_NETNS;
  return res;
}
#endif

int MiniSbxStartImpl(MiniSbxRunTime& rt) {
  int res;

  if (opt.is_running != NOT_RUNNING) {
    return MiniSbxReportError(ErrorCode::SandboxAlreadyStarted);
  }

  res = StartLoggingAndWorkingDir();
  HANDLE(res);

  if (MiniSbxGetInternalEnv() == 0) {
    return MiniSbxReportError(ErrorCode::NestedSandbox);
  }

  if (docker_mode == UNPRIVILEGED_CONTAINER) {
    // In this case there's not much to do. If we're running
    // inside a Docker container that doesn't use --privileged
    // our best is to drop certain capabilities and either spawn
    // a new child with the command line or just let the execution
    // resume in the original caller if this is the library
    DropCapabilities();
    return rt.HandleUnprivilegedContainer();
  }
  else if (docker_mode == PRIVILEGED_CONTAINER) {
    MiniSbxMountBind(ETC);
  }

  res = Prepare();
  HANDLE(res);

  rt.HandleSignals();

  SetGlobalIDs();
#ifdef MINITAP
  res = PrepareMiniTap();
  HANDLE(res);
#endif
  // Ensure we don't pass on any FDs from our parent to our child other than
  // stdin, stdout, stderr and global_debug.
  rt.MaybeCloseFds();
  return rt.RunTime();
}

int MiniSbxStartCLI() {
    MiniSbxCLIRunTime rt;
    return MiniSbxStartImpl(rt);
}



int MiniSbxStart() {
  MiniSbxLibRunTime rt;
  return MiniSbxStartImpl(rt);
}



bool MiniSbxIsNestedSandbox(){
  return MiniSbxGetInternalEnv() == 0;
}

bool MiniSbxIsRunning(){
  return (opt.is_running != NOT_RUNNING) || MiniSbxIsNestedSandbox();
}
