// Copyright 2016 The Bazel Authors. All rights reserved.
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
 * This is PID 1 inside the sandbox environment and runs in a separate user,
 * mount, UTS, IPC and PID namespace.
 */

#include "src/main/tools/linux-sandbox-pid1.h"
#include "src/main/tools/docker-support.h"

#include <errno.h>
#include <fcntl.h>
#include <grp.h>
#include <libgen.h>
#include <math.h>
#include <mntent.h>
#include <net/if.h>
#include <pwd.h>
#include <signal.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/ioctl.h>
#include <sys/mount.h>
#include <sys/prctl.h>
#include <sys/stat.h>
#include <sys/statfs.h>
#include <sys/statvfs.h>
#include <sys/syscall.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <unistd.h>
#include <linux/magic.h>

#ifndef XFS_SUPER_MAGIC
#define XFS_SUPER_MAGIC 0x58465342
#endif

#include <algorithm>
#include <string>
#include <unordered_set>
#include <set>
#if __has_include(<filesystem>)
#include <filesystem>
namespace fs = std::filesystem;
#else
#include <experimental/filesystem>
namespace fs = std::experimental::filesystem;
#define _EXPERIMENTAL_FILESYSTEM_
#endif
#include <chrono>
#include <cstring>
#include <dirent.h>
#include <iostream>
#include <numeric>
#include <stdexcept>
#include <thread>
#ifndef MS_REC
// Some systems do not define MS_REC in sys/mount.h. We might be able to grab it
// from linux/fs.h instead (cf. #2667).
#include <linux/fs.h>
#endif

#include <linux/capability.h>
#include <sys/syscall.h>

#define CAP_VERSION _LINUX_CAPABILITY_VERSION_3
#define CAP_WORDS   _LINUX_CAPABILITY_U32S_3
#define BIT(n)                       (1UL << (n))
#define OVELAY_MAX_DEPTH 5
#define OVELAY_DEPTH_THRESHOLD 2

#ifndef TEMP_FAILURE_RETRY
// Some C standard libraries like musl do not define this macro, so we'll
// include our own version for compatibility.
#define TEMP_FAILURE_RETRY(exp)                                                \
  ({                                                                           \
    decltype(exp) _rc;                                                         \
    do {                                                                       \
      _rc = (exp);                                                             \
    } while (_rc == -1 && errno == EINTR);                                     \
    _rc;                                                                       \
  })
#endif // TEMP_FAILURE_RETRY

#ifdef  _EXPERIMENTAL_FILESYSTEM_
fs::path make_relative(const fs::path& target, const fs::path& base) {
    auto target_abs = fs::canonical(target);
    auto base_abs = fs::canonical(base);

    auto target_it = target_abs.begin();
    auto base_it = base_abs.begin();

    // Skip common prefix
    while (target_it != target_abs.end() && base_it != base_abs.end() && *target_it == *base_it) {
        ++target_it;
        ++base_it;
    }

    if (base_it != base_abs.end())
        // This should be unreachable as we assume that the mount dir
        // always contains the CWD
        return "";

    fs::path result;


    for (; target_it != target_abs.end(); ++target_it) {
        result /= *target_it;
    }

    if (result == ".")
        result = fs::path("");
    return result;
}
#endif

#include "src/main/tools/linux-sandbox-options.h"
#include "src/main/tools/linux-sandbox.h"
#include "src/main/tools/logging.h"
#include "src/main/tools/process-tools.h"

static int global_child_pid __attribute__((unused));
extern DockerMode docker_mode;
std::string home_dir;
std::set<std::string> ReadOnlyPaths;
std::set<std::string> AutoFSRoots;

void MountAllOverlayFs(std::vector<std::string> list_of_dirs, int depth);
void MountOverlayFs(std::string lowerdir, int depth);
std::vector<std::string> GenerateListForOverlayFS();
bool isSubpath(const fs::path &base, const fs::path &sub);
bool alreadyMounted(const char *str, std::vector<std::string> overlay_dirs);

static bool isDevPath(const char *str) { return std::strcmp(str, "/dev") == 0; }


bool isXFS(const std::string &path) {
  struct statfs fsInfo{};
  
  if (statfs(path.c_str(), &fsInfo) != 0) {
     std::string err_msg = std::strerror(errno);
     PRINT_DEBUG("Error: Unable to statfs '%s': %s\n", path.c_str(), err_msg.c_str());
     return false;
  }
  return (fsInfo.f_type == XFS_SUPER_MAGIC);
}


// Helper methods
static void CreateFile(const char *path) {
  int handle = open(path, O_CREAT | O_WRONLY | O_EXCL, 0666);
  if (handle < 0) {
    DIE("open");
  }
  if (close(handle) < 0) {
    DIE("close");
  }
}

// Creates an empty file at 'path' by hard linking it from a known empty file.
// This is over two times faster than creating empty files via open() on
// certain filesystems (e.g. XFS).
static void LinkFile(const char *path) {
  if (link("tmp/empty_file", path) < 0) {
    CreateFile(path);
  }
}

// Recursively creates the file or directory specified in "path" and its parent
// directories.
// Return -1 on failure and sets errno to:
//    EINVAL   path is null
//    ENOTDIR  path exists and is not a directory
//    EEXIST   path exists and is a directory
//    ENOENT   stat call with the path failed
static int CreateTarget(const char *path, bool is_directory) {
  if (path == NULL) {
    errno = EINVAL;
    return -1;
  }

  struct stat sb;
  // If the path already exists...

  if (stat(path, &sb) == 0) {
    if (is_directory && S_ISDIR(sb.st_mode)) {
      // and it's a directory and supposed to be a directory, we're done here.
      return 0;
    } else if (!is_directory && S_ISREG(sb.st_mode)) {
      // and it's a regular file and supposed to be one, we're done here.
      return 0;
    } else {
      // otherwise something is really wrong.
      errno = is_directory ? ENOTDIR : EEXIST;
      return -1;
    }
  } else {
    // If stat failed because of any error other than "the path does not exist",
    // this is an error.
    if (errno != ENOENT) {
      return -1;
    }
  }

  // Create the parent directory.
  {
    char *buf, *dir;

    if (!(buf = strdup(path)))
      DIE("strdup");

    dir = dirname(buf);
    if (CreateTarget(dir, true) < 0) {
      DIE("CreateTarget %s", dir);
    }

    free(buf);
  }

  if (is_directory) {
    if (mkdir(path, 0755) < 0) {
      // Some of these mkdir(0755) might fail but we usually dont care. A way to fix
      // would be to stat() the folder, get the right mode and mkdir(path, right-mode)
      // but stat() can trigger a timeout for nfs , so for perfs better avoid to mount 
      // certain weird paths (so far we've only seen this with /sys/fs/cgroup/unified )   	    
      //DIE("mkdir(%s)", path);
      return -1;
    }
  } else {
    LinkFile(path);
  }

  return 0;
}

#if (!(LIBMINISANDBOX))
static void SetupSelfDestruction(int *pipe_to_parent) {
  // We could also poll() on the pipe fd to find out when the parent goes away,
  // and rely on SIGCHLD interrupting that otherwise. That might require us to
  // install some trivial handler for SIGCHLD. Using O_ASYNC to turn the pipe
  // close into SIGIO may also work. Another option is signalfd, although that's
  // almost as obscure as this prctl.
  if (prctl(PR_SET_PDEATHSIG, SIGKILL) < 0) {
    DIE("prctl");
  }

  // Switch to a new process group, otherwise our process group will still refer
  // to the outer PID namespace. We might then accidentally kill our parent by a
  // call to e.g. `kill(0, sig)`.
  if (setpgid(0, 0) < 0) {
    DIE("setpgid");
  }

  // Verify that the parent still lives.
  SignalPipe(pipe_to_parent);
}
#endif

static void SetupMountNamespace() {
  // Isolate only mounts in the slave (us) but not mounts in the master.
  // This is needed to support autofs
  if (mount(nullptr, "/", nullptr, MS_REC | MS_SLAVE, nullptr) < 0) {
    DIE("mount");
  }
}


static void SetupUserNamespace() {
  // Disable needs for CAP_SETGID.
  struct stat sb;
  if (stat("/proc/self/setgroups", &sb) == 0) {
    WriteFile("/proc/self/setgroups", "deny");
  } else {
    // Ignore ENOENT, because older Linux versions do not have this file (but
    // also do not require writing to it).
    if (errno != ENOENT) {
      DIE("stat(/proc/self/setgroups");
    }
  }

  uid_t inner_uid;
  gid_t inner_gid;
  if (opt.fake_root) {
    // Change our username to 'root'.
    inner_uid = 0;
    inner_gid = 0;
  } else if (opt.fake_username) {
    // Change our username to 'nobody'.
    struct passwd *pwd = getpwnam("nobody");
    if (pwd == nullptr) {
      DIE("unable to find passwd entry for user nobody")
    }

    inner_uid = pwd->pw_uid;
    inner_gid = pwd->pw_gid;
  } else {
    // Do not change the username inside the sandbox.
#ifndef MINITAP
    inner_uid = global_outer_uid;
    inner_gid = global_outer_gid;
#else
    inner_uid = global_outer_uid;
    inner_gid = global_outer_gid;
    global_outer_uid = 0;
    global_outer_gid = 0;
#endif
  }
  if (opt.enable_pty) {
    // Change the group to "tty" regardless of what was previously set
    struct group grp;
    char buf[256];
    size_t buflen = sizeof(buf);
    struct group *result;
    getgrnam_r("tty", &grp, buf, buflen, &result);
    if (result == nullptr) {
      DIE("getgrnam_r");
    }
    inner_gid = grp.gr_gid;
  }


  WriteFile("/proc/self/uid_map", "%u %u 1\n", inner_uid, global_outer_uid);
  WriteFile("/proc/self/gid_map", "%u %u 1\n", inner_gid, global_outer_gid);
}

static void SetupUtsNamespace() {
  if (sethostname("localhost", 9) < 0) {
    DIE("sethostname");
  }

  if (setdomainname("localdomain", 11) < 0) {
    DIE("setdomainname");
  }
}

static void MountFilesystems() {
  // An attempt to mount the sandbox in tmpfs will always fail, so this block is
  // slightly redundant with the next mount() check, but dumping the mount()
  // syscall is incredibly cryptic, so we explicitly check against and warn
  // about attempts to use tmpfs.
  for (const std::string &tmpfs_dir : opt.tmpfs_dirs) {
    if (opt.working_dir.find(tmpfs_dir) == 0) {
      DIE("The sandbox working directory cannot be below a path where we mount "
          "tmpfs (you requested mounting %s in %s). Is your --output_base= "
          "below one of your --sandbox_tmpfs_path values?",
          opt.working_dir.c_str(), tmpfs_dir.c_str());
    }
  }

  std::unordered_set<std::string> bind_mount_sources;

  for (size_t i = 0; i < opt.bind_mount_sources.size(); i++) {
    const std::string &source = opt.bind_mount_sources.at(i);
    bind_mount_sources.insert(source);
    const std::string &target = opt.bind_mount_targets.at(i);
    if (mount(source.c_str(), target.c_str(), nullptr, MS_BIND | MS_REC,
              nullptr) < 0) {
      DIE("mount(%s, %s, nullptr, MS_BIND | MS_REC, nullptr)", source.c_str(),
          target.c_str());
    }
  }

  for (const std::string &tmpfs_dir : opt.tmpfs_dirs) {
    if (mount("tmpfs", tmpfs_dir.c_str(), "tmpfs",
              MS_NOSUID | MS_NODEV | MS_NOATIME, nullptr) < 0) {
      DIE("mount(tmpfs, %s, tmpfs, MS_NOSUID | MS_NODEV | MS_NOATIME, nullptr)",
          tmpfs_dir.c_str());
    }
  }

  for (const std::string &writable_file : opt.writable_files) {
    if (bind_mount_sources.find(writable_file) != bind_mount_sources.end()) {
      // Bind mount sources contained in writable_files will be kept writable in
      // MakeFileSystemMostlyReadOnly, but have already been mounted at this
      // point.
      continue;
    }
    if (mount(writable_file.c_str(), writable_file.c_str(), nullptr,
              MS_BIND | MS_REC, nullptr) < 0) {
      DIE("mount(%s, %s, nullptr, MS_BIND | MS_REC, nullptr)",
          writable_file.c_str(), writable_file.c_str());
    }
  }

  // Make sure that the working directory is writable (unlike most of the rest
  // of the file system, which is read-only by default). The easiest way to do
  // this is by bind-mounting it upon itself.
  PRINT_DEBUG("working dir: %s", opt.working_dir.c_str());

  if (mount(opt.working_dir.c_str(), opt.working_dir.c_str(), nullptr, MS_BIND,
            nullptr) < 0) {
    DIE("mount(%s, %s, nullptr, MS_BIND, nullptr)", opt.working_dir.c_str(),
        opt.working_dir.c_str());
  }
}

std::vector<std::string> list_directories(const std::string string_path) {
  std::vector<std::string> directories;
  std::string path = fs::path(string_path);
  if (fs::exists(path) && fs::is_directory(path)) {
    for (const auto &entry : fs::directory_iterator(path)) {
      if (fs::is_directory(entry.status())) {
        directories.push_back(entry.path().string());
      }
    }
  } else {
    DIE("Path does not exist or is not a directory.");
  }
  return directories;
}

void MountOverlayFs(std::string lowerdir, int depth) {

  std::string overlayfs = opt.tmp_overlayfs + lowerdir;
  CreateTarget(overlayfs.c_str(), true);
  std::string workingdir = overlayfs + std::string("/workingdir");
  CreateTarget(workingdir.c_str(), true);

  std::string upperdir = overlayfs + std::string("/upperdir");
  CreateTarget(upperdir.c_str(), true);

  std::string destinationdir = opt.sandbox_root + lowerdir;
  CreateTarget(destinationdir.c_str(), true);

  std::string data = "lowerdir=" + lowerdir + ",upperdir=" + upperdir +
                     ",workdir=" + workingdir ;
  PRINT_DEBUG("mount(\"overlay\", %s, overlay, MS_MGC_VAL, %s",
              destinationdir.c_str(), data.c_str());
  int error = mount("overlay", destinationdir.c_str(), "overlay", MS_MGC_VAL,
                    data.c_str());
  if (error < 0 && errno == EINVAL) {
    // If we receive EINVAL it could either be that we cannot mount it or more
    // likely that we need to mount the subfolders one by one.
    PRINT_DEBUG("%s - %d for lowerdir %s and depth == %d", strerror(errno), error, lowerdir.c_str(), depth);
    // If we have reached a certain depth usually this means that the there are permission issues
    // so we mount as read only
    if (depth >= OVELAY_DEPTH_THRESHOLD) {
        ReadOnlyPaths.insert(lowerdir);
	    return;    
    } 
    else {
        std::vector<std::string> directories = list_directories(lowerdir);
        MountAllOverlayFs(directories, depth + 1);
    }	
    
  } else if (error < 0) {
    PRINT_DEBUG("%s", strerror(errno));
    DIE("mount(\"overlay\", %s, overlay, MS_MGC_VAL, %s",
        destinationdir.c_str(), data.c_str());
  }
}


static bool ends_with(const char *mnt_dir, const char *suffix) {
  size_t len_dir = strlen(mnt_dir);
  size_t len_suffix = strlen(suffix);
  if (len_dir < len_suffix)
    return false;

  return strcmp(mnt_dir + len_dir - len_suffix, suffix) == 0;
}



// We later remount everything read-only, except the paths for which this method
// returns true.
static bool ShouldBeWritable(const std::string &mnt_dir) {
  if (mnt_dir == opt.working_dir) {
    return true;
  }

  if (ends_with(mnt_dir.c_str(), "/proc")) 
    return true;

  if (ends_with(mnt_dir.c_str(), "/tmp")) {
    return true;
  }

  if (opt.enable_pty && mnt_dir == "/dev/pts") {
    return true;
  }

  for (const std::string &writable_file : opt.writable_files) {
    if (mnt_dir == writable_file) {
      return true;
    }
  }

  for (const std::string &tmpfs_dir : opt.tmpfs_dirs) {
    if (mnt_dir == tmpfs_dir) {
      return true;
    }
  }

  return false;
}

bool contains(const std::vector<std::string> &vec, const char *str) {
  return std::find(vec.begin(), vec.end(), std::string(str)) != vec.end();
}


// This checks if sub is a subpath of base


bool alreadyMounted(const char *str, std::vector<std::string> overlay_dirs) {

  fs::path inputPath(str);
  PRINT_DEBUG("is %s already mounted?\n", str);

  // This check must be done as early as possible as for home_dir we have a
  // special logic in the default case
  if (isSubpath(home_dir, inputPath) || isSubpath(inputPath, home_dir)) {
    return true;
  }

  if (isDevPath(str)) {
    return false;
  }

  for (const auto &pathStr : overlay_dirs) {
    fs::path path(pathStr);
    if (isSubpath(path, inputPath) || isSubpath(inputPath, path)) {
      return true;
    }
  }

  for (const auto &pathStr : opt.writable_files) {
    fs::path path(pathStr);
    if (isSubpath(path, inputPath) || isSubpath(inputPath, path)) {
      return true;
    }
  }

  for (const auto& ro_path : ReadOnlyPaths) {
    fs::path readable_path(ro_path);
    if (isSubpath(inputPath, readable_path)) {
      return true;
    }
  }

  fs::path overlay_path(opt.tmp_overlayfs);
  if (isSubpath(overlay_path, inputPath) ||
      isSubpath(inputPath, overlay_path)) {
    return true;
  }
  fs::path sandbox_root_path(opt.sandbox_root);
  if (isSubpath(sandbox_root_path, inputPath) ||
      isSubpath(inputPath, sandbox_root_path)) {
    return true;
  }

  fs::path work_dir(opt.working_dir);
  if (isSubpath(work_dir, inputPath) || isSubpath(inputPath, work_dir)) {
    return true;
  }

  fs::path dev("/dev");
  if (isSubpath(dev, inputPath) || isSubpath(inputPath, dev)) {
    return true;
  }

  return false;
}

void makeEmptyHome() {

  if (home_dir.empty()) {
    DIE("HOME environment variable not set. Indicate it with -r option");
  }
  std::string sandboxed_path_homedir = opt.sandbox_root + home_dir;
  if (CreateTarget(sandboxed_path_homedir.c_str(), true) < 0) {
    DIE("CreateTarget %s", sandboxed_path_homedir.c_str());
  }
}


// This method iterates over all the root subpaths (/bin, /etc, /lib, ..). For
// each path it checks if it is about to be mounted with a specific way
// (overlayfs, bind, write, tmpfs). If it has not been indicated in any of the
// options (see opt struct) we add it ti opt.bind_mount_sources and the
// following functions will make sure to mount it. The goal is to make sure that
// all system folders are mounted (then we'll make them read-only)
static void
AddLeftoverFoldersToBindMounts(std::vector<std::string> &overlay_dirs) {

  std::string root_path = "/"; // Root directory
  try {

    for (const auto &entry : fs::directory_iterator(root_path)) {
      if (fs::is_directory(entry.status())) {
        std::string path = entry.path().string();
        const char *entry_path_str = path.c_str();
        if (ends_with(entry_path_str, "/tmp")) {
            continue;
        }
        bool already_mounted = alreadyMounted(entry_path_str, overlay_dirs); 
        PRINT_DEBUG(" result of alreadyMonted() == %d\n", already_mounted);
        if (!already_mounted) {
          bool alreadyInBinds = false;
          for (const auto &s : opt.bind_mount_sources) {
            if (s == path) {
              alreadyInBinds = true;
              break;
            }
          }
          if (!alreadyInBinds) {
            PRINT_DEBUG("ADDING %s in InternalReadOnlyPaths", path.c_str());
            ReadOnlyPaths.insert(path);
          }
        }
      }
    }
  } catch (const fs::filesystem_error &e) {
    std::cerr << "Filesystem error: " << e.what() << std::endl;
  } catch (const std::exception &e) {
    std::cerr << "General error: " << e.what() << std::endl;
  }

  return;
}


static void MakeFilesystemPartiallyReadOnly(bool need_mount, std::vector<std::string>& overlay_dirs, int num_of_mounts) {

  FILE *mounts = setmntent("/proc/self/mounts", "r");
  if (mounts == nullptr) {
    DIE("setmntent");
  }

  struct mntent *ent;
  int count = 0;

  while ((ent = getmntent(mounts)) != nullptr) {
    if (count > num_of_mounts)
      break;

    count +=1;
    if (ends_with(ent->mnt_dir, "/proc"))
      continue;

    std::string mnt_dir(ent->mnt_dir);
    if (need_mount) {
      bool has_been_mounted = alreadyMounted(ent->mnt_dir, overlay_dirs);
      PRINT_DEBUG("already mounted -> %d\n", has_been_mounted);
      if (has_been_mounted)
        continue;
    }

    const std::string full_sandbox_path( opt.sandbox_root + std::string(ent->mnt_dir));

    int mountFlags = MS_BIND | MS_REMOUNT;
    if (hasmntopt(ent, "nodev") != nullptr) {
      mountFlags |= MS_NODEV;
    }
    if (hasmntopt(ent, "noexec") != nullptr) {
      mountFlags |= MS_NOEXEC;
    }
    if (hasmntopt(ent, "nosuid") != nullptr) {
      mountFlags |= MS_NOSUID;
    }
    if (hasmntopt(ent, "noatime") != nullptr) {
      mountFlags |= MS_NOATIME;
    }
    if (hasmntopt(ent, "nodiratime") != nullptr) {
      mountFlags |= MS_NODIRATIME;
    }
    if (hasmntopt(ent, "relatime") != nullptr) {
      mountFlags |= MS_RELATIME;
    }

    if (!ShouldBeWritable(ent->mnt_dir)) {
      mountFlags |= MS_RDONLY;
    }

    const char* target;
    if (need_mount) {

      std::string type(ent->mnt_type);
      if (type.compare("autofs") == 0 || type.compare("nfs") == 0) {
        std::string base = GetFirstFolder(ent->mnt_dir);
        PRINT_DEBUG("%s mounted in autofs/nfs. Deferring mount of %s\n", ent->mnt_dir, base.c_str());
        if (base != "")
          AutoFSRoots.insert(base);
        continue;
      }

      bool IsDirectory = true;
      if (CreateTarget(full_sandbox_path.c_str(), IsDirectory) < 0) { 
        // Here we are mounting filesystems that we might not even need
        // so if we fail something we'll just try to do our best
        continue;
      }
    

      int result = mount(ent->mnt_dir,
                       full_sandbox_path.c_str(),
                       NULL, MS_REC | MS_BIND, 
                       NULL);

       PRINT_DEBUG("%s mount: %s -> %s = %d\n", __func__, ent->mnt_dir,
                full_sandbox_path.c_str(), result);
       if (result < 0)
         continue;
       target = (const char*)full_sandbox_path.c_str();
    } 
    else {
       target = ent->mnt_dir;
    }

    PRINT_DEBUG("remount %s: %s", (mountFlags & MS_RDONLY) ? "ro" : "rw",
                ent->mnt_dir);

    if (mount(nullptr, target, nullptr, mountFlags, nullptr) < 0) {
      switch (errno) {
      case EACCES:
      case EPERM:
      case EINVAL:
      case ENOENT:
      case ESTALE:
      case ENODEV:
        PRINT_DEBUG(
            "remount(nullptr, %s, nullptr, %d, nullptr) failure (%m) ignored",
            ent->mnt_dir, mountFlags);
        break;
      default:
        DIE("remount(nullptr, %s, nullptr, %d, nullptr)", ent->mnt_dir,
            mountFlags);
      }
    }
  }
  endmntent(mounts);
}

static void MountProcAndSys() {
// Mount a new proc on top of the old one, because the old one still refers to
// our parent PID namespace.
  if (mount("/proc", "/proc", "proc", MS_NODEV | MS_NOEXEC | MS_NOSUID,
            nullptr) < 0) {
    DIE("mount /proc");
  }

  if (opt.create_netns == NO_NETNS) {
    return;
  }

  // Same for sys, but only if a separate network namespace was requested.
  if (mount("none", "/sys", "sysfs",
            MS_NOEXEC | MS_NOSUID | MS_NODEV | MS_RDONLY, nullptr) < 0) {
    DIE("mount /sys");
  }
}

static void EnableICMP() {
    WriteFile("/proc/sys/net/ipv4/ping_group_range", "0 0");
}


static void SetupNetworking() {

  // When running in a separate network namespace, enable the loopback interface
  // because some application may want to use it.
  if (opt.create_netns == NETNS_WITH_LOOPBACK) {
    // By default we disable network except for loopback interface
    int fd;
    fd = socket(AF_INET, SOCK_DGRAM, 0);
    if (fd < 0) {
      DIE("socket");
    }

    struct ifreq ifr = {};
    strncpy(ifr.ifr_name, "lo", IF_NAMESIZE);

    // Verify that name is valid.
    if (if_nametoindex(ifr.ifr_name) == 0) {
      DIE("if_nametoindex");
    }

    // Enable the interface.
    ifr.ifr_flags |= IFF_UP;
    if (ioctl(fd, SIOCSIFFLAGS, &ifr) < 0) {
      DIE("ioctl");
    }

    if (close(fd) < 0) {
      DIE("close");
    }
  }

  if (opt.create_netns != NO_NETNS && opt.fake_root) {
    // Allow IPPROTO_ICMP sockets when already allowed outside of the namespace.
    // In a namespace, /proc/sys/net/ipv4/ping_group_range is reset to the
    // default of 1 0, which does not match any groups. However, it can only be
    // overridden when the namespace has a fake root. This may be a kernel bug.
    EnableICMP() ;

  }

}

static void EnterWorkingDirectory() {
  std::string path = opt.working_dir;
  PRINT_DEBUG("Chdir to %s", path.c_str());

  if (chdir(path.c_str()) < 0) {

    DIE("chdir(%s)", path.c_str());
  }
}

static void drop_caps_ep_except(uint64_t keep) {
  struct __user_cap_header_struct hdr = {
    .version = CAP_VERSION,
    .pid = 0,
  };
  struct __user_cap_data_struct data[CAP_WORDS];
  int i;

  if (syscall(SYS_capget, &hdr, data))
    DIE("Couldn't get current capabilities");

  for (i = 0; i < CAP_WORDS; i++) {
    data[i].effective = 0;
    data[i].permitted = 0;
  }

  for (i = 0; i < CAP_WORDS; i++) {
    uint32_t mask = keep >> (32 * i);

    data[i].effective &= mask;
    data[i].permitted &= mask;
  }

  if (syscall(SYS_capset, &hdr, data))
    DIE("Couldn't drop capabilities");
}


void DropCapabilities() {
  std::cout << "Warning: Sandbox cannot be fully enabled cause we are running in an unprivileged Docker "
          "container. Will drop the capabilities of the current process but cannot provide advanced"
          "features such as usernamespace, overlayfs, rootless firewall, etc." << std::endl;

  uint64_t keep;
  keep = BIT(CAP_NET_BIND_SERVICE) | BIT(CAP_CHOWN) | BIT(CAP_DAC_READ_SEARCH) |
         BIT(CAP_KILL) | BIT(CAP_SYS_RESOURCE) | BIT(CAP_FOWNER);

  drop_caps_ep_except(keep);
  return;
}

#if (!(LIBMINISANDBOX))
static void ForwardSignal(int signum) { kill(-global_child_pid, signum); }


static int WaitForChild() {
  while (true) {
    // Wait for some process to exit. This includes reparented processes in our
    // PID namespace.
    int status;
    const pid_t pid = TEMP_FAILURE_RETRY(wait(&status));

    if (pid < 0) {
      // We don't expect any errors besides EINTR. In particular, ECHILD should
      // be impossible because we haven't yet seen global_child_pid exit.
      DIE("wait");
    }

    PRINT_DEBUG("wait returned pid=%d, status=0x%02x", pid, status);

    // If this isn't our child's PID, there's nothing further to do; we've
    // successfully reaped a zombie.
    if (pid != global_child_pid) {
      continue;
    }

    // If the child exited due to a signal, log that fact and exit with the same
    // status.
    if (WIFSIGNALED(status)) {
      const int signal = WTERMSIG(status);
      PRINT_DEBUG("child exited due to signal %d", WTERMSIG(status));
      return 128 + signal;
    }

    // Otherwise it must have exited normally.
    const int exit_code = WEXITSTATUS(status);
    PRINT_DEBUG("child exited normally with code %d", exit_code);
    return exit_code;
  }
}
#endif

void SpawnChild(bool nested) {
  PRINT_DEBUG("calling fork...");
  global_child_pid = fork();

  if (global_child_pid < 0) {
    DIE("fork()");
  } else if (global_child_pid == 0) {

    if (docker_mode != UNPRIVILEGED_CONTAINER && !nested) {
      // Put the child into its own process group.
      if (setpgid(0, 0) < 0) {
        DIE("setpgid");
      }

      // Try to assign our terminal to the child process.
      if (tcsetpgrp(STDIN_FILENO, getpgrp()) < 0 && errno != ENOTTY) {
        DIE("tcsetpgrp");
      }

      // Unblock all signals, restore default handlers.
      ClearSignalMask();

      // Close the file PRINT_DEBUG writes to.
      // Must happen late enough so we don't lose any debugging output.
      if (global_debug) {
        fclose(global_debug);
        global_debug = nullptr;
      }

      // Force umask to include read and execute for everyone, to make output
      // permissions predictable.
      umask(022);
    }

    // argv[] passed to execve() must be a null-terminated array.
    opt.args.push_back(nullptr);
    if (execvp(opt.args[0], opt.args.data()) < 0) {
      DIE("execvp(%s, %p)", opt.args[0], opt.args.data());
    }
  } else {
    PRINT_DEBUG("child started with PID %d", global_child_pid);

    if (docker_mode == UNPRIVILEGED_CONTAINER || nested ){
      int status;
      if (waitpid(global_child_pid, &status, 0) == -1) {
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
}

// Mount the sandbox root inside the mount namespace
static void MountSandboxAndGoThere() {
  PRINT_DEBUG("opt.sandbox_root -> %s\n", opt.sandbox_root.c_str());
  if (mount(opt.sandbox_root.c_str(), opt.sandbox_root.c_str(), nullptr,
            MS_BIND | MS_NOSUID,
            nullptr) < 0) { // Make mount available inside the sandbox
    if (docker_mode == PRIVILEGED_CONTAINER) {
      DIE("Attempt to mount the sandbox root folder %s failed. Most likely you "
          "are sharing it with the host via"
          "-v in the docker command line and thus we don't have sufficient "
          "privilege",
          opt.sandbox_root.c_str());
    }
    DIE("mount");
  }
  PRINT_DEBUG("mounted %s", opt.sandbox_root.c_str());
  if (chdir(opt.sandbox_root.c_str()) < 0) {
    DIE("chdir(%s)", opt.sandbox_root.c_str());
  }
}

static void CreateEmptyFile() {
  // This is used as the base for bind mounting.
  if (CreateTarget("tmp", true) < 0) {
    DIE("CreateTarget tmp")
  }
  CreateFile("tmp/empty_file");
}

static void MountDev() {
  if (CreateTarget("dev", true) < 0) {
    DIE("CreateTarget /dev");
  }
  const char *devs[] = {"/dev/null", "/dev/random", "/dev/urandom", "/dev/zero",
                        NULL};
  for (int i = 0; devs[i] != NULL; i++) {
    LinkFile(devs[i] + 1);
    if (mount(devs[i], devs[i] + 1, NULL, MS_BIND, NULL) < 0) {
      DIE("mount");
    }
  }
  if (symlink("/proc/self/fd", "dev/fd") < 0) {
    DIE("symlink");
  }
}

static int RemountRO(const std::string &path,
                     const std::string &full_sandbox_path) {
  struct statvfs vfs;
  int result = 0;
  int mountFlags = MS_BIND | MS_REMOUNT |  MS_RDONLY;
  PRINT_DEBUG("Remounting RO %s", path.c_str());
  if (statvfs(path.c_str(), &vfs) == 0) {

    if (ShouldBeWritable(path.c_str()) || ShouldBeWritable(full_sandbox_path.c_str())) {
      return result;
    }

    PRINT_DEBUG("%s should be RDONLY\n", path.c_str());
    if (vfs.f_flag & ST_NOSUID) {
      mountFlags |= MS_NOSUID;
    }
    if (vfs.f_flag & ST_NODEV) {
      mountFlags |= MS_NODEV;
    }
    if (vfs.f_flag & ST_NOEXEC) {
      mountFlags |= MS_NOEXEC;
    }
    if (vfs.f_flag & ST_NOATIME) {
      mountFlags |= MS_NOATIME;
    }
    if (vfs.f_flag & ST_NODIRATIME) {
      mountFlags |= MS_NODIRATIME;
    }
    if (vfs.f_flag & ST_RELATIME) {
      mountFlags |= MS_RELATIME;
    }
    result =
        mount(nullptr, full_sandbox_path.c_str() , NULL, mountFlags, NULL);
    PRINT_DEBUG("mount(%s,  MS_RDONLY) -> %d\n", path.c_str(), result);

  }
  PRINT_DEBUG("statvfs failed\n");

  return result;
}


static void MountAndRemountRO(const std::string& full_sandbox_path, const std::string& item) {

  PRINT_DEBUG("%s mount: %s -> %s\n", __func__, item.c_str(),   
              full_sandbox_path.c_str());
  struct stat sb;
  if (stat(item.c_str(), &sb) < 0)
    DIE("stat");
  bool IsDirectory = S_ISDIR(sb.st_mode);
  if (CreateTarget(full_sandbox_path.c_str(), IsDirectory) < 0) {
    DIE("CreateTarget %s", full_sandbox_path.c_str());
  }
  int result = mount(item.c_str(),
                     full_sandbox_path.c_str(),
                     NULL, MS_REC | MS_BIND, 
                     NULL);
  PRINT_DEBUG("%s mount: %s -> %s\n", __func__, item.c_str(),
              full_sandbox_path.c_str());
  result = RemountRO(item, full_sandbox_path);

  if (result != 0) {
    DIE("mount");
  }
}


template <typename Container>
static void MountReadOnly(const Container& container) {
  for (const auto& item : container) {
    const std::string full_sandbox_path(opt.sandbox_root + item);
  
    if (fs::exists(full_sandbox_path)) {
      const char* item_str = item.c_str();

      std::string first_folder = GetFirstFolder(item);
      if (! ( AutoFSRoots.find(first_folder) != AutoFSRoots.end())) {
        PRINT_DEBUG("%s already exists (%s). Not mounting again\n", full_sandbox_path.c_str(), item_str);      
        continue;
      }
      else {
        PRINT_DEBUG("%s already exists (%s) But mounting again cause in the AutoFSRoots\n", full_sandbox_path.c_str(), 
                        item_str); 
      }
    }
    MountAndRemountRO(full_sandbox_path, item);
  }
}

static void MountAllMounts() {
  for (const std::string &tmpfs_dir : opt.tmpfs_dirs) {

    const std::string full_sandbox_path(opt.sandbox_root + tmpfs_dir);
    CreateTarget(full_sandbox_path.c_str(), true);
    PRINT_DEBUG("tmpfs: %s", tmpfs_dir.c_str());
    if (mount("tmpfs", tmpfs_dir.c_str(), "tmpfs",
              MS_NOSUID | MS_NODEV | MS_NOATIME, nullptr) < 0) {
      DIE("mount(tmpfs, %s, tmpfs, MS_NOSUID | MS_NODEV | MS_NOATIME, nullptr)",
          tmpfs_dir.c_str());
    }
  }

  for (int i = 0; i < (signed)opt.bind_mount_sources.size(); i++) {
    if (global_debug) {
      if (strcmp(opt.bind_mount_sources[i].c_str(),
                 opt.bind_mount_targets[i].c_str()) == 0) {
        // The file is mounted to the same path inside the sandbox, as outside
        // (e.g. /home/user -> <sandbox>/home/user), so we'll just show a
        // simplified version of the mount command.
        PRINT_DEBUG("mount: %s\n", opt.bind_mount_sources[i].c_str());
      } else {
        // The file is mounted to a custom location inside the sandbox.
        // Create a user-friendly string for the sandboxed path and show it.
        const std::string user_friendly_mount_target("<sandbox>" +
                                                     opt.bind_mount_targets[i]);
        PRINT_DEBUG("mount: %s -> %s\n", opt.bind_mount_sources[i].c_str(),
                    user_friendly_mount_target.c_str());
      }
    }
    const std::string full_sandbox_path(opt.sandbox_root +
                                        opt.bind_mount_targets[i]);

    MountAndRemountRO(full_sandbox_path, opt.bind_mount_sources[i]);
  }

  MountReadOnly(ReadOnlyPaths);

  for (const std::string &writable_file : opt.writable_files) {
    PRINT_DEBUG("writable: %s", writable_file.c_str());
    const std::string full_writable_file_path(opt.sandbox_root + writable_file);
    const char *full_writable_path_str = full_writable_file_path.c_str();
    PRINT_DEBUG(" WRITABLE MAPPED TO: %s", full_writable_path_str);
    CreateTarget(full_writable_path_str,
                 fs::is_directory(writable_file.c_str()));
    if (mount(writable_file.c_str(), full_writable_path_str, nullptr,
              MS_BIND | MS_REC, nullptr) < 0) {
      DIE("mount(%s, %s, nullptr, MS_BIND | MS_REC, nullptr)",
          writable_file.c_str(), full_writable_path_str);
    }
  }
  // Make sure that the working directory is writable (unlike most of the rest
  // of the file system, which is read-only by default). The easiest way to do
  // this is by bind-mounting it upon itself.
  const std::string full_working_dir_path(opt.sandbox_root + opt.working_dir);

  CreateTarget(full_working_dir_path.c_str(), true);
  if (mount(opt.working_dir.c_str(), full_working_dir_path.c_str(), nullptr,
            MS_REC | MS_BIND, nullptr) < 0) {
    DIE("mount(%s, %s, nullptr, MS_BIND, nullptr)", opt.working_dir.c_str(),
        full_working_dir_path.c_str());
  } else {
    PRINT_DEBUG("Mounted CWD -> %s ->  %s\n", opt.working_dir.c_str(),
                full_working_dir_path.c_str());
  }
}

static void ChangeRoot() {
  // move the real root to old_root, then detach it
  char old_root[16] = "old-root-XXXXXX";
  if (mkdtemp(old_root) == NULL) {
    perror("mkdtemp");
    DIE("mkdtemp returned NULL\n");
  }
  // pivot_root has no wrapper in libc, so we need syscall()
  if (syscall(SYS_pivot_root, ".", old_root) < 0) {
    DIE("syscall");
  }
  if (chroot(".") < 0) {
    DIE("chroot");
  }
  if (umount2(old_root, MNT_DETACH) < 0) {
    DIE("umount2");
  }
  if (rmdir(old_root) < 0) {
    DIE("rmdir");
  }
}


void MountAllOverlayFs(std::vector<std::string> list_of_dirs, int depth) {
  if (depth > OVELAY_MAX_DEPTH) {
    // Most likely there is a critical error. Fail
    if (list_of_dirs.empty())
        return;
    std::string r = std::accumulate(list_of_dirs.begin(), list_of_dirs.end(),
                        std::string(""));
    DIE("ERROR: Cannot mount %s", r.c_str());
  }
  for (auto i : list_of_dirs) {
    PRINT_DEBUG("MOUNTING MountAllOvelrlayFs(%s, %d)", i.c_str(), depth);
    try {
      MountOverlayFs(i, depth);
    } catch (const fs::filesystem_error &e) {
      PRINT_DEBUG("Caught filesystem error when MountOverlayFS \n");
      std::string msg = e.what();
      PRINT_DEBUG("%s", msg.c_str());
    }
  }
}

std::vector<std::string> GenerateListForOverlayFS() {
  std::vector<std::string> paths = {"/boot",   "/etc",  "/lib", "/usr",
                                    "/lib64", "/mnt", "/sbin",
                                    "/srv",   "/bin",    "/cm",   "/emul",
                                    "/lib32", "/libx32",  "/dev/shm"};

  std::vector<std::string> existingPaths;

  for (const auto &path : paths) {
    if (fs::exists(path)) {
      existingPaths.push_back(path);
    }
  }

  for (const auto &path : opt.overlayfsmount) {
    if (fs::exists(path)) {
      addIfNotPresent(existingPaths, path.c_str());
    }
  }

  return existingPaths;
}

static void dumpOpt() {
#ifndef DEBUG
  return;
#endif

  std::cout << "Working Directory: " << opt.working_dir << "\n";
  std::cout << "Timeout (secs): " << opt.timeout_secs << "\n";
  std::cout << "Kill Delay (secs): " << opt.kill_delay_secs << "\n";
  std::cout << "SIGINT sends SIGTERM: " << std::boolalpha
            << opt.sigint_sends_sigterm << "\n";
  std::cout << "Writable Files: ";
  for (const auto &f : opt.writable_files)
    std::cout << f << " ";
  std::cout << "\nTmpfs Dirs: ";
  for (const auto &d : opt.tmpfs_dirs)
    std::cout << d << " ";
  std::cout << "\nBind Mount Sources: ";
  for (const auto &s : opt.bind_mount_sources)
    std::cout << s << " ";
  std::cout << "\nBind Mount Targets: ";
  for (const auto &t : opt.bind_mount_targets)
    std::cout << t << " ";
  std::cout << "\nFake Hostname: " << opt.fake_hostname << "\n";
  std::cout << "Fake Root: " << opt.fake_root << "\n";
  std::cout << "Fake Username: " << opt.fake_username << "\n";
  std::cout << "Enable PTY: " << opt.enable_pty << "\n";
  std::cout << "Debug Path: " << opt.debug_path << "\n";
  std::cout << "Hermetic: " << opt.hermetic << "\n";
  std::cout << "Sandbox Root: " << opt.sandbox_root << "\n";
  std::cout << "Use OverlayFS: " << opt.use_overlayfs << "\n";
  std::cout << "Tmp OverlayFS: " << opt.tmp_overlayfs << "\n";
  std::cout << "OverlayFS Mounts: ";
  for (const auto &m : opt.overlayfsmount)
    std::cout << m << " ";
  std::cout << "\nArgs: ";
  for (const auto &a : opt.args)
    std::cout << a << " ";
  std::cout << "\nUse Default: " << opt.use_default << "\n";
  //std::cout << "Firewall Rules Path: " << opt.firewall_rules_path << "\n";

}

static std::string TopLevelRelativeFolder(const std::string& mount_point, const std::string& workdir) {
  fs::path mount_fs = mount_point;
  fs::path wd_fs = workdir;

#ifndef  _EXPERIMENTAL_FILESYSTEM_
  fs::path rel = fs::relative(wd_fs, mount_fs);
#else
  fs::path rel = make_relative(wd_fs, mount_fs);
#endif
  PRINT_DEBUG("Relative path from WorkingDir to MountFS is: %s", rel.string().c_str());
  auto it = rel.begin();
  if (std::distance(it, rel.end()) >= 1) {
    if (rel.filename() == fs::path("."))
      return "";
    fs::path top_level_fs = mount_fs / *it;
    return fs::absolute(top_level_fs).string();
  } else {
    return "";
  }
}


static int MountOverlaySubfolders(std::string& top_level_dir, std::string& workdir) {
  fs::path top_level = top_level_dir;
  fs::path working_dir = workdir;

  top_level = fs::canonical(top_level);
  working_dir = fs::canonical(working_dir);

  PRINT_DEBUG("%s, work dir %s\n", __func__, working_dir.string().c_str());
  PRINT_DEBUG("%s, top level dir %s", __func__, top_level.string().c_str());

  if (working_dir.string().find(top_level.string()) != 0) {
      return 1;
  }
  fs::path current = top_level;
  while (true) {
      if (current == working_dir) {
          break;
      }
        
#ifndef  _EXPERIMENTAL_FILESYSTEM_
      fs::path rel = fs::relative(working_dir, current);
#else
      fs::path rel = make_relative(working_dir, current);
#endif
      if (rel.empty() || rel.filename() == fs::path(".") ) 
          break;
      std::string current_str = current.string();
      PRINT_DEBUG("Adding %s to the overlayfs\n", current_str.c_str());
      MiniSbxMountOverlay(current);
      current /= *rel.begin();
  }
  return 0;
}


static void MountWorkingDirMountPoint(const std::string& mount_point) {
  PRINT_DEBUG("mount_point -> %s\n", mount_point.c_str());
  if (mount_point.empty())
    return;

  // top_level is the first directory between our CWD and its respective mount
  // point. For instance, if our mount point is /A and CWD is /A/B/C/D,
  // the top_level dir will be /A/B . The top_level dirs and all subdirs until
  // the CWD will be mounted as overlay and their content will be mapped to allow
  // by default access to parent folder' files
  std::string top_level = TopLevelRelativeFolder(mount_point, opt.working_dir);

  if (top_level == "" ) {
    return;
  }

  // If the top_level folder contains the home dir we would end up mounting
  // the home dir as overlay but that might leak secrets so we try to get the
  // new top_level dir from the home_dir to our working dir
  // e.g., /home/user/top_level/working_dir -> top_level will be /home/user/top_level
  if (isSubpath(top_level, home_dir) ) {
    top_level = TopLevelRelativeFolder(home_dir, opt.working_dir);
  }
  PRINT_DEBUG("top_level -> %s\n", top_level.c_str());

  // if the top_level folder is empty just return.
  // The code later will mount this as overlay
  // without mapping the files in the parents folder tho
  if (top_level == "" ) {
    return;
  }

  int res = 0;

  //if ( mount_point != "/" && !isSubpath(mount_point, home_dir))
  //  res = MiniSbxMountOverlay(mount_point);

  if (opt.parents_writable)
    res += MiniSbxMountWrite(top_level);
  else {   
    if (isXFS(top_level)) {
       PRINT_DEBUG("XFS\n");
       MountOverlaySubfolders(top_level, opt.working_dir);
    }
    else {
        res += MiniSbxMountOverlay(top_level);
    }
  }

  if (res < 0)
      DIE("Error %d\n in %s", res, __func__);
}


static void MountOverlayDirAsTmpfs() {
  // This is needed cause we cannot mount overlayfs over another overlayfs
  if (mount("tmpfs", opt.tmp_overlayfs.c_str(), "tmpfs", 0, "") != 0) {
    DIE("Mount tmp_overlayfs as tmp");
  }
}


int Pid1Main(void *args) {
  PRINT_DEBUG("Pid1Main started with pid = %d", getpid());

  MiniSbxSetInternalEnv();

  home_dir = GetHomeDir();
  std::vector<std::string> overlay_dirs;
  int mounts = 0;

  Pid1Args pid1Args = {nullptr, nullptr};
  if (args != NULL) {
    pid1Args = *(static_cast<Pid1Args *>(args));
  }

  if (getpid() != 1) {
    DIE("Using PID namespaces, but we are not PID 1");
  }

  PRINT_DEBUG("Running in docker? %d\n", docker_mode);

  WaitPipe(pid1Args.pipe_from_parent);

  // Start with default signal handlers and an empty signal mask.
  ClearSignalMask();

#if (!(LIBMINISANDBOX))
  SetupSelfDestruction(pid1Args.pipe_to_parent);
#endif
  SetupMountNamespace();
  SetupUserNamespace();

  if (opt.fake_hostname) {
    SetupUtsNamespace();
  }

  dumpOpt();
  if (docker_mode == PRIVILEGED_CONTAINER) {
    if (opt.use_overlayfs)
        MountOverlayDirAsTmpfs();
  }

  if (opt.use_default | opt.hermetic | opt.use_overlayfs) {

    MountSandboxAndGoThere();
    CreateEmptyFile();
    MountDev();
    MountProcAndSys();

    if (opt.use_default) {
      PRINT_DEBUG("opt.default");
      const std::string mount_point = GetMountPointOf(opt.working_dir);
      mounts = CountMounts();
      MountWorkingDirMountPoint(mount_point);
      overlay_dirs = GenerateListForOverlayFS();
      MountAllOverlayFs(overlay_dirs, 0);

      AddLeftoverFoldersToBindMounts(overlay_dirs);
      MakeFilesystemPartiallyReadOnly(true, overlay_dirs, mounts);

      MountAllMounts();

      makeEmptyHome();
    } else if (opt.use_overlayfs){
      PRINT_DEBUG("opt.use_overlayfs");
      std::vector<std::string> list_of_dirs = opt.overlayfsmount;
      MountAllOverlayFs(list_of_dirs, 0);
      MountAllMounts();
      makeEmptyHome();
    } else if (opt.hermetic) {
      PRINT_DEBUG("opt.hermetic");
      MountAllMounts();
    } else {
      DIE("UNREACHABLE ELSE");
    }
    ChangeRoot();

  } else {
    // In this case the sandbox works in best effort mode
    // doesn't use overlayfs or chroot, it just re-mounts
    // everything as read-only
    PRINT_DEBUG("Warning: Sandbox enabled in read-only mode. This is not "
            "recommended for production scenarios. \n");

    MountFilesystems();
    mounts = CountMounts();
    // In this case overlay_dirs will be empty but we need it when
    // we call the same function and we're using the overlayfs at the 
    // same time
    MakeFilesystemPartiallyReadOnly(false, overlay_dirs, mounts);
    MountProcAndSys();
  }

  SetupNetworking();

  EnterWorkingDirectory();

  // Ignore terminal signals; we hand off the terminal to the child in
  // SpawnChild below.
  IgnoreSignal(SIGTTIN);
  IgnoreSignal(SIGTTOU);

#if (!(LIBMINISANDBOX))
  // Fork the child process.
  SpawnChild(false);
  InstallSignalHandler(SIGTERM, ForwardSignal);
  return WaitForChild();
#else
  return 0;
#endif
}
