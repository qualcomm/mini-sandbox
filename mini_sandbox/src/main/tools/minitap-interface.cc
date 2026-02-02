/*
 * Copyright (c) 2025 Qualcomm Technologies, Inc. and/or its subsidiaries.
 * SPDX-License-Identifier: MIT
 */

#include "src/main/tools/error-handling.h"
#include "src/main/tools/process-tools.h"
#include "src/main/tools/linux-sandbox-options.h"

#include <sys/types.h>
#include <sys/wait.h>
#include <unistd.h>
#include <stdlib.h>
#include <sched.h>
#include <signal.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <dlfcn.h>
#include <iostream>
#include <limits.h>
#if __has_include(<filesystem>)
#include <filesystem>
namespace fs = std::filesystem;
#else
#include <experimental/filesystem>
namespace fs = std::experimental::filesystem;
#define _EXPERIMENTAL_FILESYSTEM_
#endif

#define MINITAPBIN "minitap"

char MinitapBin[PATH_MAX] = {0};

volatile sig_atomic_t signal_received = 0;

void handler(int sig) {
    signal_received = 1;
}



static std::string ResolveSymlink(const std::string& path) {
    std::error_code ec;
    fs::path fsPath(path);

    if (fs::is_symlink(fsPath, ec)) {
        char resolvedPath[PATH_MAX];
        if (realpath(path.c_str(), resolvedPath)) {
            return std::string(resolvedPath);
        } else {
            return path;
        }
    }
    return path;
}


int GetSharedObjectPath(std::string& output) {
    Dl_info info;
    void* symbol = reinterpret_cast<void*>(&GetSharedObjectPath); // or any known symbol in the .so

    if (dladdr(symbol, &info)) {
        if (strstr(info.dli_fname, ".so") != NULL) {
            output = ResolveSymlink(info.dli_fname);
            return 0;
        } 
    } 
    return -1;
}


static std::string LocateBinaryFromParent(const char* exePath) {
    try {
      fs::path currentBinaryPath(exePath);
      fs::path directory = currentBinaryPath.parent_path();
      fs::path MiniTapBinPath = directory / MINITAPBIN;
      if (!fs::exists(MiniTapBinPath)) 
        return "";
      return MiniTapBinPath.string();
    } catch (const fs::filesystem_error& e) {
      std::cerr << "Filesystem error: " << e.what() << std::endl;
      return "";
    }
}


int GetMinitapBinDir() {
    // If already initialized we are done here
    if (MinitapBin[0] != '\0') 
      return 0;
    const char* mini_tap_env = std::getenv("MINI_SANDBOX_TAP_BINARY");
    if (mini_tap_env) {
        strncpy(MinitapBin, mini_tap_env, PATH_MAX - 1);
        MinitapBin[PATH_MAX - 1] = '\0';
    } 
    else {
      std::string BinPath;
#ifdef LIBMINISANDBOX
      std::string libPath;
      int res = GetSharedObjectPath(libPath);
      if (res < 0)
          return -1;
      BinPath = LocateBinaryFromParent(libPath.c_str());
#else        
      char exePath[PATH_MAX];
      ssize_t len = readlink("/proc/self/exe", exePath, sizeof(exePath) - 1);
      if (len == -1) {
          perror("readlink");
          return -1;
      }
      exePath[len] = '\0'; // Null-terminate the string
      BinPath = LocateBinaryFromParent(exePath); 
#endif
      if (BinPath.empty())
          return -1;
      strncpy(MinitapBin, BinPath.c_str(), PATH_MAX - 1);
      MinitapBin[PATH_MAX - 1] = '\0';
    }
    return 0;
}


static int RunMinitap(std::string& rules) {
    signal(SIGUSR1, handler);
    if (GetMinitapBinDir() < 0) 
        // If all our heuristics for finding the minitap binary go wrong
        // we try to see if it's in PATH
        strncpy(MinitapBin, MINITAPBIN, sizeof(MINITAPBIN));

    char* const  m_args[] = {MinitapBin, (char*)rules.c_str(), NULL};

    pid_t p = fork();
    if (p == 0) {
        execvp(m_args[0], m_args);
        std::cerr << "Failed to execute minitap binary. No TUN device and firewall available\n";
        kill(getppid(), SIGUSR1);
        exit(-1);
    }
    else {
        while (signal_received == 0) 
            pause();
        return p;
    }
    // Should never go here
    return -1;
}


static int JoinNetNs(pid_t p) {
  char netns_path[256];
  snprintf(netns_path, sizeof(netns_path), "/proc/%d/ns/net", p);

  int fd = open(netns_path, O_RDONLY);
  if (fd == -1) {
    return -1;
  }

  if (setns(fd, CLONE_NEWNET) == -1) {
    close(fd);
    return -1;
  }

  if (close(fd) < 0)
    return -1;
  return 0;
}


int RunTCPIP(uid_t outer_uid, gid_t outer_gid, std::string& rules) {
  pid_t extern_pid = fork();
 
  if(extern_pid == 0) {
    int r = unshare(CLONE_NEWUSER);
    if (r != 0) {
      exit(0);
    }
    WriteFile("/proc/self/uid_map", "0 %u 1\n", outer_uid);
    WriteFile("/proc/self/setgroups", "deny");
    WriteFile("/proc/self/gid_map", "0 %u 1\n", outer_gid);
    pid_t minitap_pid = fork();
    if (minitap_pid == 0) {
      pid_t tcp_p = RunMinitap(rules);
      if ( JoinNetNs(tcp_p) < 0)
        exit(0);
      return 0;
    } else {
      int status = 0;
      if (waitpid(minitap_pid, &status, 0) == -1) {
        perror("waitpid failed");
        exit(EXIT_FAILURE);
      }
      if (WIFEXITED(status)) {
        int exit_code = WEXITSTATUS(status);
        exit(exit_code);
      } else {
        exit(EXIT_FAILURE);
      }
    }
  }
  else {
    int status = 0;
    if (waitpid(extern_pid, &status, 0) == -1) {
      perror("waitpid failed");
      exit(EXIT_FAILURE);
    }

    if (WIFEXITED(status)) {
      int child_exit_code = WEXITSTATUS(status);
      int init_status = MiniSbxReadInit();
      if (init_status == 0)
        return -1;
      exit(child_exit_code);
    }
    else {
      fprintf(stderr, "Child did not exit normally\n");
      Cleanup();
      exit(EXIT_FAILURE);
    }
  }
}
