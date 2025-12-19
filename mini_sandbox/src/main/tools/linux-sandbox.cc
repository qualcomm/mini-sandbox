
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
#include "src/main/tools/docker_support.h"
#include "src/main/tools/linux-sandbox-options.h"
#include "src/main/tools/linux-sandbox-pid1.h"
#include "src/main/tools/logging.h"
#include "src/main/tools/minitap-interface.h"
#include "src/main/tools/process-tools.h"
#include "src/main/tools/error_handling.h"
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

// The PID of our child process, for use in signal handlers.
static std::atomic<pid_t> global_child_pid{0};
// Our parent's pid at the outset, to check if the original parent has exited.
pid_t initial_ppid;

// Must we politely ask the child to exit before we send it a SIGKILL (once we
// want it to exit)? Holds only zero or one.
static std::atomic<int> global_need_polite_sigterm{false};

#if __cplusplus >= 201703L
static_assert(global_child_pid.is_always_lock_free);
static_assert(global_need_polite_sigterm.is_always_lock_free);
#endif

// Make sure the child process does not inherit any accidentally left open file
// handles from our parent.

#if (!(LIBMINISANDBOX))
static void CloseFds() {
  DIR *fds = opendir("/proc/self/fd");
  if (fds == nullptr) {
    MiniSbxReport("opendir");
  }

  while (1) {
    errno = 0;
    struct dirent *dent = readdir(fds);

    if (dent == nullptr) {
      if (errno != 0) {
        MiniSbxReport("readdir");
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
          MiniSbxReport("close");
        }
      }
    }
  }

  if (closedir(fds) < 0) {
    MiniSbxReport("closedir");
  }
}
#endif

#if (!(LIBMINISANDBOX))
static void OnTimeoutOrTerm(int) {
  // Find the PID of the child, which main set up before installing us as a
  // signal handler.
  const pid_t child_pid = global_child_pid.load(std::memory_order_relaxed);

  // Figure out whether we should send a SIGTERM here. If so, we won't want to
  // next time we're called.
  const bool need_polite_sigterm =
      global_need_polite_sigterm.fetch_and(0, std::memory_order_relaxed);

  // If we're not supposed to ask politely, simply forcibly kill the child.
  if (!need_polite_sigterm) {
    kill(child_pid, SIGKILL);
    return;
  }

  // Otherwise make a polite request, then arrange to be called again after a
  // delay, at which point we'll send SIGKILL.
  //
  // Note that main sets us up as the signal handler for SIGALRM, and arranges
  // for this code path to be taken only if kill_delay_secs > 0.
  kill(child_pid, SIGTERM);
  alarm(opt.kill_delay_secs);
}
#endif

static pid_t SpawnPid1() {
  const int kStackSize = 1024 * 1024;
  std::vector<char> child_stack(kStackSize);

  PRINT_DEBUG("calling pipe(2)...");

  int pipe_from_child[2], pipe_to_child[2];
  if (pipe(pipe_from_child) < 0) {
    MiniSbxReport("pipe");
  }
  if (pipe(pipe_to_child) < 0) {
    MiniSbxReport("pipe");
  }

  int clone_flags = CLONE_NEWUSER | CLONE_NEWNS | CLONE_NEWIPC | CLONE_NEWPID;
#ifndef LIBMINISANDBOX
  clone_flags |= SIGCHLD;
#endif

  if (opt.create_netns != NO_NETNS) {
    clone_flags |= CLONE_NEWNET;
  }

  if (opt.fake_hostname) {
    clone_flags |= CLONE_NEWUTS;
  }

  // We use clone instead of unshare, because unshare sometimes fails with
  // EINVAL due to a race condition in the Linux kernel (see
  // https://lkml.org/lkml/2015/7/28/833).
  PRINT_DEBUG("calling clone(2)...");

  Pid1Args pid1Args;
  pid1Args.pipe_to_parent = pipe_from_child;
  pid1Args.pipe_from_parent = pipe_to_child;
#ifdef LIBMINISANDBOX
  int unshare_res = unshare(clone_flags);
  if (unshare_res < 0) {
    MiniSbxReport("unshare failed\n");
  }
  pid_t child_pid = fork();
#else
  const pid_t child_pid =
      clone(Pid1Main, child_stack.data() + kStackSize, clone_flags, &pid1Args);
#endif

  if (child_pid < 0) {
    MiniSbxReport("clone");
  }

#ifdef LIBMINISANDBOX
  if (child_pid == 0) {
    int sandbox_res = Pid1Main(&pid1Args);
    if (sandbox_res < 0) {
      MiniSbxReport("Failed in Pid1Main\n");
    }
    return 0;
  } else {
#endif
    // Signal the child that it can now proceed to spawn pid2.
    SignalPipe(pipe_to_child);
    PRINT_DEBUG("linux-sandbox-pid1 has PID %d", child_pid);

    // Wait for a signal from the child linux-sandbox-pid1 process; this proves
    // to the child process that we still existed after it ran
    // prctl(PR_SET_PDEATHSIG, SIGKILL), thus preventing a race condition where
    // the parent is killed before that call was made.
    WaitPipe(pipe_from_child);

    PRINT_DEBUG("done manipulating pipes");

    return child_pid;
#ifdef LIBMINISANDBOX
  }
#endif
  return 0;
}


#if (!(LIBMINISANDBOX))
static int WaitForPid1(const pid_t child_pid) {
  // Wait for the child to exit, obtaining usage information. Restart in the
  // case of a signal interrupting us.
  int child_status;
  struct rusage child_rusage;
  while (true) {
    const int ret = wait4(child_pid, &child_status, 0, &child_rusage);
    if (ret > 0) {
      break;
    }

    // We've been handed off to a reaper process and should die.
    if (getppid() != initial_ppid) {
      break;
    }

    if (errno == EINTR) {
      continue;
    }

    MiniSbxReport("wait4");
  }

  // If we're supposed to write stats to a file, do so now.
  // if (!opt.stats_path.empty()) {
  //  WriteStatsToFile(&child_rusage, opt.stats_path);
  //}

  // We want to exit in the same manner as the child.
  if (WIFSIGNALED(child_status)) {
    const int signal = WTERMSIG(child_status);
    PRINT_DEBUG("child exited due to receiving signal: %s", strsignal(signal));
    return 128 + signal;
  }

  const int exit_code = WEXITSTATUS(child_status);
  PRINT_DEBUG("child exited normally with code %d", exit_code);
  return exit_code;
}
#endif

static int ValidateOptions() {

  if (opt.use_overlayfs) {
    if (ValidateOverlayOutOfFolder(opt.tmp_overlayfs, opt.working_dir) < 0)
      return MiniSbxReportError(__func__, ErrorCode::IllegalConfiguration);
  }
 
  if (ValidateReadWritePaths(opt.bind_mount_sources, opt.writable_files) < 0)
      return MiniSbxReportError(__func__, ErrorCode::FileReadAndWrite);

  if (docker_mode == PRIVILEGED_CONTAINER) {
    MiniSbxMountBind(std::string("/etc"));
  }
  else {
    for (auto writable_file : opt.writable_files) {
      if (opt.use_overlayfs && ValidateOverlayOutOfFolder(opt.tmp_overlayfs, writable_file) < 0)
        return MiniSbxReportError(__func__, ErrorCode::IllegalConfiguration);
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
      return MiniSbxReportError(__func__, ErrorCode::IllegalConfiguration);

  if (ValidateTmpNotRemounted(opt.bind_mount_sources) < 0)
      return MiniSbxReportError(__func__, ErrorCode::IllegalConfiguration);
 
  return 0;
}


#if LIBMINISANDBOX
int MiniSbxStart() {
#else
int main(int argc, char *argv[]) {
#endif
  int res;

  docker_mode = CheckDockerMode();   

  // Ask the kernel to kill us with SIGKILL if our parent dies.
  if (prctl(PR_SET_PDEATHSIG, SIGKILL) < 0) {
    MiniSbxReport("prctl");
  }

#if (!(LIBMINISANDBOX))
  // Parse our command-line options.
  ParseOptions(argc, argv);
#else
  if (opt.working_dir.empty()) {
    char *working_dir = getcwd(nullptr, 0);
    if (working_dir == nullptr) {
      return -1;
    }
    opt.working_dir = working_dir;
  }
#endif

  if (MiniSbxGetInternalEnv() == 0) {
    PRINT_DEBUG("Already running inside mini-sandbox. Not nesting another sandbox\n");
#if (!(LIBMINISANDBOX))
    SpawnChild(true);  
#else
    return MiniSbxReportGenericRecoverableError("Already running inside mini-sandbox");
#endif
  }

  if (docker_mode == UNPRIVILEGED_CONTAINER) {
    // In this case there's not much to do. If we're running
    // inside a Docker container that doesn't use --privileged
    // our best is to drop certain capabilities and either spawn
    // a new child with the command line or just let the execution
    // resume in the original caller if this is the library
    DropCapabilities();
#if (!(LIBMINISANDBOX))
    SpawnChild(false);  
#else
    return 0;
#endif
  }

  res = ValidateOptions();
  if (res < 0)
    return res;
  
  // Open the file PRINT_DEBUG writes to.
  // Must happen early enough so we don't lose any debugging output.
  if (!opt.debug_path.empty()) {
    global_debug = fopen(opt.debug_path.c_str(), "w");
    if (!global_debug) {
      MiniSbxReport("fopen(%s)", opt.debug_path.c_str());
    }
  }

#if (!(LIBMINISANDBOX))
  // Start with default signal actions and a clear signal mask.
  ClearSignalMask();

  // Ignore SIGTTIN and SIGTTOU, as we hand off the terminal to the child in
  // SpawnChild.
  IgnoreSignal(SIGTTIN);
  IgnoreSignal(SIGTTOU);

  // Remember the parent pid so we can exit if the parent has exited.
  // Doing this before prctl(PR_SET_PDEATHDIG, 0) ensures no race condition.
  initial_ppid = getppid();
#endif


  // Set up two globals used by the child process.
  global_outer_uid = getuid();
  global_outer_gid = getgid();

#ifdef MINITAP
  std::string rules = CreateRandomFilename(std::string("/tmp"));
  DumpRules(&(opt.fw_rules), rules);
  RunTCPIP(global_outer_uid, global_outer_gid, rules);
  // In this case the Network namespace has been taken care of by RunTCPIP so
  // we don't need to create a new one
  opt.create_netns = NO_NETNS;

#endif
  // Ensure we don't pass on any FDs from our parent to our child other than
  // stdin, stdout, stderr and global_debug.
#if (!(LIBMINISANDBOX))

  CloseFds();
#endif
  // Spawn the child that will fork the sandboxed program with fresh
  // namespaces etc.

#ifdef LIBMINISANDBOX
  pid_t pid = fork();
  if (pid < 0) {
    perror("fork1 - error");
    exit(-1);
  } else if (pid == 0) {
#endif // ends #ifdef LIBMINISANDBOX
    const pid_t child_pid = SpawnPid1();
#ifdef LIBMINISANDBOX
    if (child_pid != 0) {
#endif
      // Let the signal handlers installed below know the PID of the child.
      global_child_pid.store(child_pid, std::memory_order_relaxed);

      // If a kill delay has been configured, let the signal handlers installed
      // below know that it needs to be respected.
      if (opt.kill_delay_secs > 0) {
        global_need_polite_sigterm.store(1, std::memory_order_relaxed);
      }

      // OnTimeoutOrTerm, which is used for other signals below, assumes that it
      // handles SIGALRM. We also explicitly invoke it after the timeout using
      // alarm(2).
#if (!(LIBMINISANDBOX))
      InstallSignalHandler(SIGALRM, OnTimeoutOrTerm);

      // If requested, arrange for the child to be killed (optionally after
      // being asked politely to terminate) once the timeout expires.
      //
      // Note that it's important to set this up before support for SIGTERM and
      // SIGINT. Otherwise one of those signals could arrive before we get here,
      // and then we would reset its opt.kill_delay_secs interval timer.
      if (opt.timeout_secs > 0) {
        alarm(opt.timeout_secs);
      }

      // Also ask/tell the child to quit on SIGTERM, and optionally for SIGINT
      // too.
      InstallSignalHandler(SIGTERM, OnTimeoutOrTerm);
      if (opt.sigint_sends_sigterm) {
        InstallSignalHandler(SIGINT, OnTimeoutOrTerm);
      }
#endif
#ifdef LIBMINISANDBOX

      int status;
      if (waitpid(child_pid, &status, 0) == -1) {
            perror("waitpid failed");
            exit(EXIT_FAILURE);
      }

      if (WIFEXITED(status)) {
            // Child exited normally, get its exit status
            int child_exit_code = WEXITSTATUS(status);
            exit(child_exit_code); // Exit parent with the same code
      } else {
            // Child did not exit normally
            fprintf(stderr, "Child did not exit normally\n");
            exit(EXIT_FAILURE);
      }
    }
  }
#else
    int exit_res = WaitForPid1(child_pid);
#endif


#ifdef LIBMINISANDBOX
  else {
    int status;
    if (waitpid(pid, &status, 0) == -1) {
          perror("waitpid failed");
          exit(EXIT_FAILURE);
    }
     
#endif
    Cleanup();
#ifdef LIBMINISANDBOX

   if (WIFEXITED(status)) {
          // Child exited normally, get its exit status
          int child_exit_code = WEXITSTATUS(status);
          exit(child_exit_code); // Exit parent with the same code
      } else {
          // Child did not exit normally
          fprintf(stderr, "Child did not exit normally\n");
          exit(EXIT_FAILURE);
      }
  }
  return 0;
#else
  return exit_res;
#endif
}
