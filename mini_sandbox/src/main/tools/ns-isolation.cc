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

// The PID of our child process, for use in signal handlers.
static std::atomic<pid_t> global_child_pid{0};

// Must we politely ask the child to exit before we send it a SIGKILL (once we
// want it to exit)? Holds only zero or one.
static std::atomic<int> global_need_polite_sigterm{false};
extern pid_t initial_ppid;
#if __cplusplus >= 201703L
static_assert(global_child_pid.is_always_lock_free);
static_assert(global_need_polite_sigterm.is_always_lock_free);
#endif



static pid_t SpawnPid1() {
  const int kStackSize = 1024 * 1024;
  std::vector<char> child_stack(kStackSize);

  PRINT_DEBUG("calling pipe(2)...");

  int pipe_from_child[2], pipe_to_child[2];
  if (pipe(pipe_from_child) < 0) {
    return MiniSbxReportGenericError("pipe");
  }
  if (pipe(pipe_to_child) < 0) {
    return MiniSbxReportGenericError("pipe");
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
    MiniSbxReportGenericError("unshare failed\n");
  }
  pid_t child_pid = fork();
#else
  const pid_t child_pid =
      clone(Pid1Main, child_stack.data() + kStackSize, clone_flags, &pid1Args);
#endif

  if (child_pid < 0) {
    MiniSbxReportGenericError("clone");
  }


#ifdef LIBMINISANDBOX
  if (child_pid == 0) {
    int sandbox_res = Pid1Main(&pid1Args);
    if (sandbox_res < 0) {
      MiniSbxReportGenericError("Failed in Pid1Main\n");
    }
    return 0;
  } else {
#endif
    // Signal the child that it can now proceed to spawn pid2.
    int res = SignalPipe(pipe_to_child, false);

    // If an error happens in SignalPipe or later in WaitPipe we want to return
    // but first we'll have to kill the child
    if (res < 0) {
      KillAndWait(child_pid);
      return res;
    }
    PRINT_DEBUG("linux-sandbox-pid1 has PID %d", child_pid);

    // Wait for a signal from the child linux-sandbox-pid1 process; this proves
    // to the child process that we still existed after it ran
    // prctl(PR_SET_PDEATHSIG, SIGKILL), thus preventing a race condition where
    // the parent is killed before that call was made.
    res = WaitPipe(pipe_from_child, false);

    // same error logic as for SignalPipe
    if (res < 0) {
      KillAndWait(child_pid);
      return res;
    }

    PRINT_DEBUG("done manipulating pipes");

    return child_pid;
#ifdef LIBMINISANDBOX
  }
#endif
  return 0;
}



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

    MiniSbxReportGenericError("wait4");
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

int NSRunTimeCLI() {
  if (! UserNamespaceSupported() ) {
    return MiniSbxReportError(ErrorCode::UserNSNotSupported);
  }

  // Spawn the child that will fork the sandboxed program with fresh
  // namespaces etc.
  const pid_t child_pid = SpawnPid1();
  if (child_pid < 0) {
    PRINT_DEBUG("SpawnPid1 returned -1\n");
    exit(-1);
  }
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
   int exit_res = WaitForPid1(child_pid);
   Cleanup();
   return exit_res;
}


int NSRunTimeLib() {
  if (! UserNamespaceSupported() ) {
    return MiniSbxReportError(ErrorCode::UserNSNotSupported);
  }

  // Spawn the child that will fork the sandboxed program with fresh
  // namespaces etc.
  pid_t pid = fork();
  if (pid < 0) {
    perror("fork1 - error");
    return -1;
  } else if (pid == 0) {
    const pid_t child_pid = SpawnPid1();
    if (child_pid < 0) {
      PRINT_DEBUG("SpawnPid1 returned -1\n");
      exit(-1);
    }

    if (child_pid != 0) {
      // If a kill delay has been configured, let the signal handlers installed
      // below know that it needs to be respected.
      if (opt.kill_delay_secs > 0) {
        global_need_polite_sigterm.store(1, std::memory_order_relaxed);
      }

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
  else {
    int status;
    if (waitpid(pid, &status, 0) == -1) {
          perror("waitpid failed");
          Cleanup();
          exit(EXIT_FAILURE);
    }
         
   if (WIFEXITED(status)) {
          // Child exited normally, get its exit status
          int child_exit_code = WEXITSTATUS(status);
          int init_status = MiniSbxReadInit();
          Cleanup();
          // init_status tells us if the Pid1 inside the sandbox has completed the initialization process
          // it evaluates to 0 if something went wrong, or 1 if everything went good. If we had a 
          // mini-sandbox internal's problem we want to return -1 in the library and don't DIE the whole
          // process. In the other cases instead we wanna exit cause the exit signal is coming from the 
          // sandboxed process
          if (init_status == 0) {
            //We consider mini sandbox as "running" from this moment onwards
            opt.is_running = FAILED;  
#ifdef MINITAP
          // If we are in libminitapbox the pid executing this code is child of the one we forked in
          // RunTcpIp() . Thus we want to exit here and the father that is waiting for us will return for 
          // us
            exit(0);
#else
          // If we are in libminisandbox here we can just return -1 (error code) and then the user 
          // will do something with this value
            return -1;
#endif
          }
          exit(child_exit_code); // Exit parent with the same code
      } else {
          // Child did not exit normally
          fprintf(stderr, "Child did not exit normally\n");
          Cleanup();
          exit(EXIT_FAILURE);
      }
  }
  return 0;
}
