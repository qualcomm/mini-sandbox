// Copyright 2015 The Bazel Authors. All rights reserved.
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

#ifndef SRC_MAIN_TOOLS_PROCESS_TOOLS_H_
#define SRC_MAIN_TOOLS_PROCESS_TOOLS_H_

#include <stdbool.h>
#include <sys/types.h>
#include <string>
#include <vector>

#if __has_include(<filesystem>)
#include <filesystem>
namespace fs = std::filesystem;
#else
#include <experimental/filesystem>
namespace fs = std::experimental::filesystem;
#define _EXPERIMENTAL_FILESYSTEM_
#endif

#define MAX_DEPTH_SANDBOX_ROOT 5
#define MAX_DEPTH_OVERLAYFS_ROOT 12

// Set up a signal handler for a signal.
void InstallSignalHandler(int signum, void (*handler)(int));

// Set the signal handler for `signum` to SIG_IGN (ignore).
void IgnoreSignal(int signum);

// Set the signal handler for `signum` to SIG_DFL (default).
void InstallDefaultSignalHandler(int sig);

// Use an empty signal mask for the process and set all signal handlers to their
// default.
void ClearSignalMask();

// Write contents to a file.
void WriteFile(const std::string &filename, const char *fmt, ...);

// Waits for a process to terminate but does *not* collect its exit status.
//
// Note that the process' zombie status may not be available immediately after
// this call returns.
//
// May not be implemented on all platforms.
int WaitForProcessToTerminate(pid_t pid);

// Waits for a process group to terminate.  Assumes that the process leader
// still exists in the process table (though it may be a zombie), and allows
// it to remain.
//
// Assumes that the pgid has been sent a termination signal on entry to
// terminate quickly (or else this will send its own termination signal to
// the group).
//
// May not be implemented on all platforms.
int WaitForProcessGroupToTerminate(pid_t pgid);

// Terminates and waits for all descendents of the given process to exit.
//
// Assumes that the caller has enabled the child subreaper feature before
// spawning any subprocesses.
//
// Assumes that the caller has already waited for the process to collect its
// exit code as this discards the exit code of all processes it encounters.
//
// May not be implemented on all platforms.
int TerminateAndWaitForAll(pid_t pid);

// Blocks and waits on a pipe for the a signal to proceed from the other process
// `SignalPipe()`.
int WaitPipe(int *pipe, bool die_on_err);

// Signals to the other process blocked with a read `WaitPipe()` on the pipe
// that it can proceed by writing a byte to the pipe.
int SignalPipe(int *pipe, bool die_on_err);

std::string CreateTempDirectory(const std::string& base_path);
std::string CreateRandomFilename(const std::string& base_path);
int CreateDirectory(const std::string& base_path, const std::string& dir_name, std::string& out);
int CreateDirectories(const std::string& base_path);
int CountMounts();
std::string GetCurrentWorkingDirectory();
bool isSubpath(const fs::path &base, const fs::path &sub);
std::string GetMountPointOf(const std::string& dir);
int GetCWD(std::string& res);
std::string GetParentCWD();
std::string GetHomeDir();
std::string GetLocalBin();
std::string GetLocalLib();
uid_t get_outer_uid();
gid_t get_outer_gid();

void addIfNotPresent(std::vector<std::string>& paths, const char* path);
void Cleanup();

int MiniSbxSetInternalEnv();
int MiniSbxGetInternalEnv();
std::string GetFirstFolder(const std::string& path);
bool GetOSName(std::string& printable_name, std::string& version_id);
bool GetKernelInfo(struct utsname* buf);
bool UserNamespaceSupported();
void KillAndWait(pid_t pid);


enum UserNamespaceSupport {
    NON_INIT,
    USER_NS_SUPPORTED,
    USER_NS_NOT_SUPPORTED
};

#endif  // PROCESS_TOOLS_H__
