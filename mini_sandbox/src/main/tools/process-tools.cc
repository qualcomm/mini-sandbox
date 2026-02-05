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

#include "src/main/tools/process-tools.h"
#include "src/main/tools/logging.h"
#include "src/main/tools/error-handling.h"
#include "src/main/tools/linux-sandbox-options.h"

#include <mntent.h>

#include <array>
#include <cerrno>
#include <csignal>
#include <cstdlib>
#include <cstring>
#include <iostream>
#include <errno.h>
#include <fcntl.h>
#include <math.h>
#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/resource.h>
#include <sys/syscall.h>
#include <sched.h>
#include <signal.h>
#include <sys/stat.h>
#include <sys/time.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <sys/utsname.h>
#include <unistd.h>
#if __has_include(<filesystem>)
#include <filesystem>
namespace fs = std::filesystem;
#else
#include <experimental/filesystem>
namespace fs = std::experimental::filesystem;
#define _EXPERIMENTAL_FILESYSTEM_
#endif
#include <algorithm>
#include <climits>
#include <random>
#include <string>
#include <vector>
#include <memory>
#include <fstream>
#include <cctype>

#define MAX_ATTEMPTS 100
#define INTERNAL_MINI_SANDBOX_ENV "__INTERNAL_MINI_SANDBOX_ON"

static UserNamespaceSupport user_ns_support = NON_INIT;

void InstallSignalHandler(int signum, void (*handler)(int)) {
  struct sigaction sa = {};
  sa.sa_handler = handler;
  if (handler == SIG_IGN || handler == SIG_DFL) {
    // No point in blocking signals when using the default handler or ignoring
    // the signal.
    if (sigemptyset(&sa.sa_mask) < 0) {
      MiniSbxReportGenericError("sigemptyset");
    }
  } else {
    // When using a custom handler, block all signals from firing while the
    // handler is running.
    if (sigfillset(&sa.sa_mask) < 0) {
      MiniSbxReportGenericError("sigfillset");
    }
  }
  // sigaction may fail for certain reserved signals. Ignore failure in this
  // case, but report it in debug mode, just in case.
  if (sigaction(signum, &sa, nullptr) < 0) {
    PRINT_DEBUG("sigaction(%d, &sa, nullptr) failed", signum);
  }
}

void IgnoreSignal(int signum) {
  // These signals can't be handled, so we'll just not do anything for these.
  if (signum != SIGSTOP && signum != SIGKILL) {
    InstallSignalHandler(signum, SIG_IGN);
  }
}

void InstallDefaultSignalHandler(int signum) {
  // These signals can't be handled, so we'll just not do anything for these.
  if (signum != SIGSTOP && signum != SIGKILL) {
    InstallSignalHandler(signum, SIG_DFL);
  }
}


void ClearSignalMask() {
  // Use an empty signal mask for the process.
  sigset_t empty_sset;
  if (sigemptyset(&empty_sset) < 0) {
    MiniSbxReportGenericError("sigemptyset");
  }
  if (sigprocmask(SIG_SETMASK, &empty_sset, nullptr) < 0) {
    MiniSbxReportGenericError("sigprocmask");
  }

  // Set the default signal handler for all signals.
  for (int i = 1; i < NSIG; ++i) {
    if (i == SIGKILL || i == SIGSTOP) {
      continue;
    }

    struct sigaction sa = {};
    sa.sa_handler = SIG_DFL;
    if (sigemptyset(&sa.sa_mask) < 0) {
      MiniSbxReportGenericError("sigemptyset");
    }
    // Ignore possible errors, because we might not be allowed to set the
    // handler for certain signals, but we still want to try.
    sigaction(i, &sa, nullptr);
  }
}

// Write contents to a file.
void WriteFile(const std::string &filename, const char *fmt, ...) {
  FILE *stream = fopen(filename.c_str(), "w");
  if (stream == nullptr) {
    DIE("fopen(%s)", filename.c_str());
  }

  va_list ap;
  va_start(ap, fmt);
  int r = vfprintf(stream, fmt, ap);
  va_end(ap);

  if (r < 0) {
    DIE("vfprintf");
  }

  if (fclose(stream) != 0) {
    DIE("fclose(%s)", filename.c_str());
  }
}

static int DieOrReport(const char* msg, bool die_on_error) {
  if (die_on_error)
    DIE("%s", msg);
  return MiniSbxReportGenericError(msg);
}

// Waits for a signal to proceed from the pipe.
int WaitPipe(int *pipe, bool die_on_error) {
  char buf = 0;

  // Close the writer fd of this process as it should only be written to by the
  // writer of the other process.
  if (close(pipe[1]) < 0) {
    return DieOrReport("close", die_on_error);
  }
  if (read(pipe[0], &buf, 1) < 0) {
    return DieOrReport("read", die_on_error);
  }
  if (close(pipe[0]) < 0) {
    return DieOrReport("close", die_on_error);
  }
  return 0;
}



// Sends a signal to the pipe for the other waiting process proceed.
int SignalPipe(int *pipe, bool die_on_error) {
  char buf = 0;
  // Close the reader fd of this process as it should only be read by the reader
  // of the other process.
  if (close(pipe[0]) < 0) {
    return DieOrReport("close", die_on_error);
  }
  if (write(pipe[1], &buf, 1) < 0) {
    return DieOrReport("write", die_on_error);
  }
  if (close(pipe[1]) < 0) {
    return DieOrReport("close", die_on_error);
  }
  return 0;
}


void KillAndWait(pid_t pid) {
  kill(pid, SIGKILL);
  waitpid(pid, NULL, 0);
  return;
}


int CreateDirectory(const std::string& base_path, const std::string& dir_name, std::string& out) {
  int res = 0;
  std::error_code ec;
  out = base_path + "/" + dir_name;
  fs::path p = fs::path(out);
  if (!fs::exists(p, ec)) {
    fs::create_directories(out, ec);
  }
  if (ec) {
    return MiniSbxReportGenericError(ec.message());
  }
  return res;
}

int CreateDirectories(const std::string& base_path) {
  int res = 0;
  std::error_code ec;
  fs::create_directories(base_path, ec);
  if (ec) {
    return MiniSbxReportGenericError(ec.message());
  }
  return res;
}


std::string CreateTempDirectory(const std::string &base_path) {

  std::random_device rd;
  std::mt19937 gen(rd());
  std::uniform_int_distribution<> dis(1000, INT_MAX);
  int randomID;
  int res;
  
  for (int attempts = 0; attempts < MAX_ATTEMPTS; attempts++) {
    std::error_code ec;
    randomID = dis(gen);
    std::string tempDirPath = base_path + "/temp_" + std::to_string(randomID);
    fs::path p = fs::path(tempDirPath);
    if (!fs::exists(p, ec)) {
      res = CreateDirectories(tempDirPath);
      if (res < 0) {
        std::string err_msg = "Could not create temporary directory:" + tempDirPath;
        MiniSbxReportGenericError(err_msg);
      }
      return tempDirPath;
    }
    if (ec) {
      PRINT_DEBUG("%s Access error: %s", __func__, p.c_str());
    }
  }
  return nullptr;
}



std::string CreateRandomFilename(const std::string& base_path) {
    std::mt19937 rng(std::chrono::steady_clock::now().time_since_epoch().count());
    std::uniform_int_distribution<int> dist(1000, INT_MAX);

    std::string filename;
    do {
        int randomNumber = dist(rng);
        filename = base_path + "/" + std::to_string(randomNumber) + ".rules";
    } while (fs::exists(filename));

    return filename;
}

std::string GetCurrentWorkingDirectory() { return fs::current_path().string(); }

int GetCWD(std::string& res) {

  try {
    fs::path currentPath = fs::current_path();
    res += currentPath.string();
    return 0;
  } catch (const fs::filesystem_error& e) {
        return MiniSbxReportGenericError("Filesystem error");
  } catch (const std::exception& e) {
        return MiniSbxReportGenericError("General exception");
  } catch (...) {
    return MiniSbxReportError(ErrorCode::Unknown);
  }

}


int CountMounts() {
    FILE* fp = setmntent("/proc/self/mounts", "r");
    if (!fp) return -1;

    int count = 0;
    while (getmntent(fp)) {
        ++count;
    }

    endmntent(fp);
    return count;
}


bool isSubpath(const fs::path &base, const fs::path &sub) {
  auto baseIt = base.begin();
  auto subIt = sub.begin();

  while (baseIt != base.end() && subIt != sub.end() && *baseIt == *subIt) {
    ++baseIt;
    ++subIt;
  }
  return baseIt == base.end();
}

#include <sys/sysmacros.h>

static int ValidateDevId(const char* mnt_dir, dev_t st_dev ) {
  struct stat mountStat;
  if (stat(mnt_dir, &mountStat) == 0) {
      if (st_dev == mountStat.st_dev) {
          return 0;         
      }
  }
  return -1;
}

std::string GetMountPointOf(const std::string& dir) {
    PRINT_DEBUG("Executing %s\n", __func__);

    const char* dir_str = dir.c_str();
    
    struct stat dirStat;
    if (stat(dir_str, &dirStat) != 0) {
        perror("stat");
        return "";
    }

    FILE* mtab = setmntent("/etc/mtab", "r");
    if (!mtab) {
        perror("setmntent");
        return "";
    }

    std::string mountPoint;
    size_t currMountPointLen = 0;
    struct mntent* ent;
    while ((ent = getmntent(mtab)) != nullptr) {
        if (isSubpath(ent->mnt_dir, dir_str)) {
	    if (ValidateDevId(ent->mnt_dir, dirStat.st_dev) == 0) {
                if (mountPoint.empty()) {
                    mountPoint = ent->mnt_dir;
                    currMountPointLen = strlen(ent->mnt_dir);
                } 
                else {
                    size_t newMountPointLen = strlen(ent->mnt_dir);
                    if (newMountPointLen > currMountPointLen) {
                        mountPoint = ent->mnt_dir;
                    }
                } 
            }
        }
    }
    PRINT_DEBUG("Final mount point -> %s\n", mountPoint.c_str());

    endmntent(mtab);
    return mountPoint;
}


std::string GetParentCWD() {
  fs::path currentPath = fs::current_path();
  fs::path parentPath = currentPath.parent_path();
  return parentPath.string();
}

std::string GetHomeDir() {
  std::string home_dir(std::getenv("HOME"));
  return home_dir;
}

std::string GetLocalBin() {
  std::string home_dir = GetHomeDir();
  return home_dir.append("/.local/bin");
}

std::string GetLocalLib() {
  std::string home_dir = GetHomeDir();
  return home_dir.append("/.local/lib");
}

void addIfNotPresent(std::vector<std::string> &paths, const char *path_to_check) {
  // Convert the const char* to std::string for comparison
  std::string pathStr(path_to_check);

  // Check if the path is already in the vector
  if (std::find(paths.begin(), paths.end(), pathStr) == paths.end()) {
    // If not found, add the path to the vector
    paths.push_back(pathStr);
  }

}


static inline void makeWritable(const fs::path &dir, unsigned depth, unsigned max_depth) {
  std::error_code ec;

  fs::permissions(dir,
                  fs::perms::owner_write | fs::perms::group_write |
                      fs::perms::others_write | fs::perms::owner_exec |
                      fs::perms::group_exec | fs::perms::others_exec |
                      fs::perms::owner_read | fs::perms::group_read |
                      fs::perms::others_read
#ifdef _EXPERIMENTAL_FILESYSTEM_
                      | fs::perms::add_perms,
#else
                  ,
                  fs::perm_options::add,
#endif
                  ec);

  if (ec) {
    PRINT_DEBUG("Warning: error changing permissions for %s",
                dir.string().c_str());
  }
  if (depth > max_depth)
    return;

  for (const auto &entry : fs::directory_iterator(
           dir, fs::directory_options::skip_permission_denied)) {
    if (fs::is_directory(entry.status())) {
      makeWritable(entry.path(), depth + 1, max_depth);
    }
  }
}



void Cleanup() {
  // If we are debugging we can leave the temp folders
  if (!opt.debug_path.empty()) return;

  // else let's remove them
  if (opt.use_default || opt.use_overlayfs || opt.hermetic) {
    PRINT_DEBUG("delete %s\n", opt.sandbox_root.c_str());
    try {
      fs::path sandbox_dir = opt.sandbox_root.c_str();
      makeWritable(sandbox_dir, 0, MAX_DEPTH_SANDBOX_ROOT);
      if (fs::remove_all(sandbox_dir) < 0) {
        MiniSbxReportGenericError("ERROR when removing the sandbox directory");
      }

      if (!opt.hermetic) {
        fs::path overlayfs_dir = opt.tmp_overlayfs.c_str();
        makeWritable(overlayfs_dir, 0, MAX_DEPTH_OVERLAYFS_ROOT);
        if (fs::remove_all(overlayfs_dir) < 0) {
          MiniSbxReportGenericError("ERROR when removing the overlay directory");
        }
      }
    } catch (const std::exception &e) {
      std::cerr << "Warning: Could not remove the trash files: " << e.what()
                << std::endl;
    }
  }
}


uid_t get_outer_uid() {
    FILE *f = fopen("/proc/self/uid_map", "r");
    if (!f) {
        perror("fopen /proc/self/uid_map");
        return (uid_t)-1;  // Sentinel for error
    }

    char line[256];
    uid_t outer_uid = (uid_t)-1;

    while (fgets(line, sizeof(line), f)) {
        unsigned int inside, outside, size;
        if (sscanf(line, "%u %u %u", &inside, &outside, &size) == 3) {
            if (inside == 0) {
                outer_uid = (uid_t)outside;
                break;
            }
        }
    }

    fclose(f);
    return outer_uid;
}


gid_t get_outer_gid() {
    FILE *f = fopen("/proc/self/gid_map", "r");
    if (!f) {
        perror("fopen /proc/self/gid_map");
        return (gid_t)-1;  // Sentinel for error
    }

    char line[256];
    gid_t outer_gid = (gid_t)-1;

    while (fgets(line, sizeof(line), f)) {
        unsigned int inside, outside, size;
        if (sscanf(line, "%u %u %u", &inside, &outside, &size) == 3) {
            if (inside == 0) {
                outer_gid = (gid_t)outside;
                break;
            }
        }
    }

    fclose(f);
    return outer_gid;
}


int MiniSbxSetInternalEnv() {
  if (setenv(INTERNAL_MINI_SANDBOX_ENV, "1", 1) != 0) {
      std::cerr << "Failed to set environment variable." << std::endl;
      MiniSbxReportGenericError("Failed to set environment variable __INTERNAL_MINI_SANDBOX_ON");
  }
  return 0;
}


int MiniSbxGetInternalEnv() {
  const char* value = getenv(INTERNAL_MINI_SANDBOX_ENV);
  if (value) {
    return 0;
  } 
  return -1;
}

std::string GetFirstFolder(const std::string& path) {
    if (path.empty() || path[0] != '/' || path == "/") {
        return "";
    }

    std::size_t secondSlash = path.find('/', 1);
    if (secondSlash != std::string::npos) {
        return path.substr(0, secondSlash);
    } else {
        return path;
    }
}



static inline void trim(std::string& s) {
  auto is_not_space = [](unsigned char ch) { return !std::isspace(ch); };
  s.erase(s.begin(), std::find_if(s.begin(), s.end(), is_not_space));
  s.erase(std::find_if(s.rbegin(), s.rend(), is_not_space).base(), s.end());
}

// Remove surrounding single or double quotes if present
static inline void unquote(std::string& s) {
  if (s.size() >= 2 &&
      ((s.front() == '"' && s.back() == '"') ||
       (s.front() == '\'' && s.back() == '\''))) {
    s = s.substr(1, s.size() - 2);
  }
}

// Parses /etc/os-release and returns PRETTY_NAME and VERSION_ID via out-params.
// Returns true iff at least one of the requested keys was found.
// On failure to read the file or if keys are missing, outputs remain empty.
bool GetOSName(std::string& printable_name, std::string& version_id) {
  printable_name.clear();
  version_id.clear();

  std::ifstream file("/etc/os-release");
  if (!file.is_open()) {
    return false; // Could not open the file
  }

  std::string line;
  while (std::getline(file, line)) {
    if (line.empty() || line[0] == '#') continue;

    const auto eq_pos = line.find('=');
    if (eq_pos == std::string::npos) continue;

    std::string key = line.substr(0, eq_pos);
    std::string value = line.substr(eq_pos + 1);

    trim(key);
    trim(value);
    unquote(value); 

    if (key == "NAME") {
      printable_name = value;
    } else if (key == "VERSION_ID") {
      version_id = value;
    }

    if (!printable_name.empty() && !version_id.empty()) {
      break;
    }
  }
  return (!printable_name.empty() || !version_id.empty());
}


bool GetKernelInfo(struct utsname* buf) {
  return (uname(buf) == 0);
}


bool parseVersion(const std::string &versionStr, int &major, int &minor) {
    std::istringstream iss(versionStr);
    char dot;
    if (!(iss >> major)) return false;
    if (!(iss >> dot) || dot != '.') return false;
    if (!(iss >> minor)) return false;
    return true;
}


bool HasUserNamespaceSupport() {
    static const char* const paths[] = {
        "/proc/self/ns/user",
        "/proc/self/ns/pid",
        "/proc/self/ns/net",
        "/proc/self/ns/ipc",
    };

    for (const char* p : paths) {
        if (access(p, F_OK) == -1) {
            return false;
        }
    }
    return true;
}

// Code heavily inspired by 
//https://github.com/mozilla-firefox/firefox/blob/131497bb1b747587b2b21b1abf14f44ecffad805/security/sandbox/linux/SandboxInfo.cpp
bool CanCreateUserNamespace() {
    pid_t pid = static_cast<pid_t>(
        syscall(__NR_clone, SIGCHLD | CLONE_NEWUSER,
                nullptr, nullptr, nullptr, nullptr));

    if (pid == 0) {
        int rv = unshare(CLONE_NEWPID);
        _exit(rv == 0 ? 0 : 1);
    }

    if (pid == -1) {
        return false;
    }

    int status = 0;
    pid_t w;
    do {
        w = waitpid(pid, &status, 0);
    } while (w == -1 && errno == EINTR);

    if (w == -1) {
        return false;
    }

    bool ok = (WIFEXITED(status) && WEXITSTATUS(status) == 0);
    return ok;
}



bool UserNamespaceSupported() {
  bool res = false;
  if (user_ns_support != NON_INIT)
    res = (user_ns_support == USER_NS_SUPPORTED) ? true : false ;
  else if (std::getenv("MINI_SANDBOX_FORCE_USER_NAMESPACE") != nullptr)
    res = true;
  else {
    res = HasUserNamespaceSupport() && CanCreateUserNamespace();
    user_ns_support = (res) ? USER_NS_SUPPORTED : USER_NS_NOT_SUPPORTED;
  }
  return res;
}

