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

#include "src/main/tools/linux-sandbox-options.h"
#include "src/main/tools/docker-support.h"
#include "error-handling.h"

#include <errno.h>
#include <fstream>
#include <iostream>
#include <memory>
#include <regex>
#include <sched.h>
#include <stdarg.h>
#include <stdbool.h>
#include <stdexcept>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <string>
#include <sys/stat.h>
#include <sys/prctl.h>
#include <sys/wait.h>
#include <unistd.h>
#include <vector>
#if __has_include(<filesystem>)
#include <filesystem>
namespace fs = std::filesystem;
#else
#include <experimental/filesystem>
namespace fs = std::experimental::filesystem;
#define _EXPERIMENTAL_FILESYSTEM_
#endif
#include "src/main/tools/logging.h"
#include "src/main/tools/process-tools.h"
#include <algorithm>
#include <arpa/inet.h>
#include <fcntl.h>
#include <netdb.h>
#include <random>

using std::ifstream;
using std::unique_ptr;
using std::vector;

struct Options opt;
static int MiniSbxSetupSandboxRootWithOverlay(const std::string& path);
static int MiniSbxSetupOverlayfsFolder(std::string path);
static std::string CanonicPath(const std::string path_str,
                                bool resolve_symlink);


// Print out a usage error. argc and argv are the argument counter and vector,
// fmt is a format, string for the error message to print.
static void Usage(char *program_name, const char *fmt, ...) {
  va_list ap;
  va_start(ap, fmt);
  vfprintf(stderr, fmt, ap);
  va_end(ap);

  fprintf(stderr, "\nUsage: %s -- command arg1 @args\n", program_name);
  fprintf(
      stderr,
      "\nPossible arguments:\n"
      "  -W <working-dir>  working directory (uses current directory if "
      "not specified)\n"
      "  -T <timeout>  timeout after which the child process will be "
      "terminated with SIGTERM\n"
      "  -t <timeout>  in case timeout occurs, how long to wait before "
      "killing the child with SIGKILL\n"
      "  -i  on receipt of a SIGINT, forward it to the child process as a "
      "SIGTERM first and then as a SIGKILL after the -T timeout\n"
      "  -w <file>  make a file or directory writable for the sandboxed "
      "process\n"
      "  -e <dir>  mount an empty tmpfs on a directory\n"
      "  -M/-m <source/target>  directory to mount (bind) inside the sandbox\n"
      "    Multiple directories can be specified and each of them will be "
      "mounted readonly.\n"
      "    The -M option specifies which directory to mount, the -m option "
      "specifies where to\n"
      "  -H  if set, make hostname in the sandbox equal to 'localhost'\n"
      "  -n  if set, create a new network namespace\n"
      "  -N  if set, does not create a new network namespace\n"
      "        Only one of -n and -N may be specified.\n"
      "  -R  if set, make the uid/gid be root\n"
      "  -U  if set, make the uid/gid be nobody\n"
      "  -P  if set, make the gid be tty and make /dev/pts writable\n"
      "  -F <firewall-rules-file> if set, reads the firewall rules to enable "
      "(only in tap mode)\n"
      "  -D <debug-file> if set, debug info will be printed to this file\n"
      "  -d <overlayfs-directory> indicate the base folder to use for overlay"
      ", it's meant to be used together with -o\n"
      "  -o <sandbox-root-directory> enables the use of overlayfs and sets up "
      "the sandbox-root-directory. Typically to be used with '-d' to specify "
      "the overlayfs directory\n"
      "  -x if set, activates a default version of the sandbox (w/ overlay, no "
      "network, chroot) \n"
      " -k directory ot mount as overlayfs inside the sanbdox. Only available "
      "with -o\n"
      "  -h <sandbox-dir>  if set, chroot to sandbox-dir and only "
      " mount whats been specified with -M/-m for improved hermeticity. "
      " The working-dir should be a folder inside the sandbox-dir\n"
      "  @FILE  read newline-separated arguments from FILE\n"
      "  --  command to run inside sandbox, followed by arguments\n");
  exit(EXIT_FAILURE);
}

static void ValidateIsAbsolutePath(char *path, char *program_name, char flag) {
  if (path[0] != '/') {
    Usage(program_name, "The -%c option must be used with absolute paths only.",
          flag);
  }
}


static int ValidateDir(const std::string& dir) {

  fs::path path(dir);
  try {
    if (!(fs::exists(path)) || !(fs::is_directory(path))) {
      return MiniSbxReportError(__func__, ErrorCode::InvalidFolder);
    }
  } catch (const fs::filesystem_error &e) {
    PRINT_DEBUG("Filesystem error %s:", e.what());
    return MiniSbxReportError(__func__, ErrorCode::InvalidFolder);
  }
  return 0;
}




static int ValidatePath(const std::string &func_name,
                        const std::string &path) {
  if (path[0] != '/') {
    return MiniSbxReportError(func_name, ErrorCode::NotAnAbsolutePath);
  }
  fs::path fs_path(path);
  try {
    if (!(fs::exists(fs_path))) {
      return MiniSbxReportError(func_name, ErrorCode::PathDoesNotExist);
    }
  } catch (const fs::filesystem_error &e) {
    PRINT_DEBUG("Filesystem error %s:", e.what());
    return MiniSbxReportError(func_name, ErrorCode::PathDoesNotExist);
  }
  return 0;
}


int ValidateOverlayOutOfFolder(const std::string &overlay_dir,
                               const std::string &folder) {
  try {
    fs::path overlay_path = fs::canonical(overlay_dir);
    fs::path folder_path = fs::canonical(folder);

    // If the overlay_dir is shorter than sandbox_dir, then it must be out of it
    if (std::distance(overlay_path.begin(), overlay_path.end()) <
        std::distance(folder_path.begin(), folder_path.end()))
      return 0;

    auto rel = std::mismatch(folder_path.begin(), folder_path.end(),
                             overlay_path.begin());
    // this means that sandbox_dir contains overlay_dir -- we don't want this
    if (rel.first == folder_path.end())
      return -1;
    return 0;
  } catch (const fs::filesystem_error &e) {
    return -1;
  }
}


int ValidateReadWritePaths(std::vector<std::string>& readables, 
                           std::vector<std::string>& writables) {
  for (auto readable_path : readables) {
    for (auto writable_path : writables) {
      int result = readable_path.compare(writable_path);
      if (result == 0) {
        return -1;
      }
    }
  }
  return 0;
}

int ValidateTmpNotRemounted(std::vector<std::string>& paths) {
  std::string tmp(TMP);
  if (opt.use_default) {
    for (auto path : paths) {
      if ( path.compare(tmp) == 0) {
        return -1;
      }
    }
  }
  return 0;
}


static int CreateSandboxRoot(const std::string& base_dir) {
  std::string rand_sandbox_root;
  ValidateDir(base_dir);
  if (opt.sandbox_root.empty()) {
    rand_sandbox_root = CreateTempDirectory(base_dir);
    if (rand_sandbox_root.back() == '/') {
      opt.sandbox_root.assign(rand_sandbox_root, 0,
                              rand_sandbox_root.length() - 1);
      if (opt.sandbox_root.back() == '/') {
        return MiniSbxReportError(__func__, ErrorCode::InvalidFolder);
      }
    } else {
      opt.sandbox_root.assign(rand_sandbox_root);
    }
  }
  else {
    return MiniSbxReportError(__func__, ErrorCode::SandboxRootNotUnique);
  }
  return 0;
}

static int CreateOverlayfsDir(std::string& base_dir) {
  std::string tmp_overlayfs;
  int res = 0;
  res = ValidateDir(base_dir);
  if (res < 0)
      return res;

  if (!opt.use_default)
    tmp_overlayfs = CreateTempDirectory(base_dir);
  else {
    if (docker_mode == PRIVILEGED_CONTAINER) {
      tmp_overlayfs = CreateTempDirectory("/tmp");
    } else {
      tmp_overlayfs = CreateTempDirectory(base_dir);
    }
  }
  opt.tmp_overlayfs.assign(tmp_overlayfs, 0, tmp_overlayfs.length());
  return res;
}


// This function is intended for usage in the APIs, so it's safe to assume that
// every error is not recoverable
std::string CanonicPath(const std::string path_str, bool resolve_symlink) {
  try {
    fs::path path(path_str);
    if (fs::exists(path)) {
      if (!resolve_symlink && fs::is_symlink(path)) {
        return fs::absolute(path).string();
      }
      return fs::canonical(path).string();
    } else {
      return path.string(); 
      // If the path doesn't exist the best that we can do is
      // to return the original path. The parsing will likely
      // fail when we call ValidatePath, which instead requires
      // that the path exists.
    }
  } catch (const fs::filesystem_error &e) {
    PRINT_DEBUG("Filesystem error %s:", e.what());
    MiniSbxReport("Fs exception");
    return nullptr;    
  }
}

// Parses command line flags from an argv array and puts the results into an
// Options structure passed in as an argument.
static void ParseCommandLine(unique_ptr<vector<char *>> args) {
  extern char *optarg;
  extern int optind, optopt;
  int c;

  while ((c = getopt(args->size(), args->data(),
                     ":S:G:W:T:t:il:L:w:e:M:m:h:HnNRUPF:D:o:d:k:x")) != -1) {

    switch (c) {
    case 'W':
      if (opt.working_dir.empty()) {
        ValidateIsAbsolutePath(optarg, args->front(), static_cast<char>(c));
        opt.working_dir.assign(optarg);
      } else {
        Usage(args->front(),
              "Multiple working directories (-W) specified, expected one.");
      }
      break;
    case 'T':
      if (sscanf(optarg, "%d", &opt.timeout_secs) != 1 ||
          opt.timeout_secs < 0) {
        Usage(args->front(), "Invalid timeout (-T) value: %s", optarg);
      }
      break;
    case 't':
      if (sscanf(optarg, "%d", &opt.kill_delay_secs) != 1 ||
          opt.kill_delay_secs < 0) {
        Usage(args->front(), "Invalid kill delay (-t) value: %s", optarg);
      }
      break;
    case 'i':
      opt.sigint_sends_sigterm = true;
      break;
    case 'w':
      if (MiniSbxMountWrite(std::string(optarg)) < 0) {
        Usage(args->front(), MiniSbxGetErrorMsg());
      }
      break;
    case 'e':
      if (MiniSbxMountTmpfs(std::string(optarg)) < 0) {
        Usage(args->front(), MiniSbxGetErrorMsg());
      }
      break;
    case 'M':
      if (MiniSbxMountBind(std::string(optarg)) < 0) {
        Usage(args->front(), MiniSbxGetErrorMsg());
      }
      break;
    case 'h':
      if (MiniSbxSetupHermetic(optarg) < 0) {
        Usage(args->front(), MiniSbxGetErrorMsg());
      }
      break;
    case 'H':
      opt.fake_hostname = true;
      break;
    case 'n':
      if (opt.create_netns == NO_NETNS) {
        Usage(args->front(), "Only one of -n and -N may be specified.");
      }
      opt.create_netns = NETNS;
      break;
#ifndef MINITAP
    case 'N':
      if (opt.create_netns == NETNS) {
        Usage(args->front(), "Only one of -n and -N may be specified.");
      }
      MiniSbxShareNetNamespace();
      break;
#else
    case 'F':
      if (opt.firewall_rules_path.empty()) {
        ValidateIsAbsolutePath(optarg, args->front(), static_cast<char>(c));
        opt.firewall_rules_path.assign(optarg);
        MiniSbxAllowConnections(opt.firewall_rules_path.c_str());
      } else {
        Usage(args->front(),
              "Cannot write firewall rule in more than one file.");
      }
      break;
#endif
    case 'R':
      if (opt.fake_username) {
        Usage(args->front(),
              "The -R option cannot be used at the same time us the -U "
              "option.");
      }
      opt.fake_root = true;
      break;
    case 'U':
      if (opt.fake_root) {
        Usage(args->front(),
              "The -U option cannot be used at the same time us the -R "
              "option.");
      }
      opt.fake_username = true;
      break;
    case 'P':
      opt.enable_pty = true;
      break;
    case 'D':
      if (MiniSbxEnableLog(std::string(optarg)) < 0) {
        Usage(args->front(), MiniSbxGetErrorMsg());
      }
      break;
    case 'o':
      if (MiniSbxSetupSandboxRootWithOverlay(std::string(optarg)) < 0) {
        Usage(args->front(), MiniSbxGetErrorMsg());
      }
      break;
    case 'd':
      if (MiniSbxSetupOverlayfsFolder(std::string(optarg)) < 0) {
        Usage(args->front(), MiniSbxGetErrorMsg());
      }
      break;
    case 'k':
      if (MiniSbxMountOverlay(optarg) < 0) {
        Usage(args->front(), MiniSbxGetErrorMsg());
      }
      break;
    case 'x':
      if (MiniSbxSetupDefault() < 0) {
          Usage(args->front(), MiniSbxGetErrorMsg());
      }
      break;
    case '?':
      Usage(args->front(), "Unrecognized argument: -%c (%d)", optopt, optind);
      break;
    case ':':
      Usage(args->front(), "Flag -%c requires an argument", optopt);
      break;
    }
  }

  if (!opt.working_dir.empty() && !opt.sandbox_root.empty() &&
      opt.working_dir.find(opt.sandbox_root) == std::string::npos) {
    Usage(args->front(),
          "working-dir %s (-W) should be a "
          "subdirectory of sandbox-dir %s (-h)",
          opt.working_dir.c_str(), opt.sandbox_root.c_str());
  }
  if (optind < static_cast<int>(args->size())) {
    if (opt.args.empty()) {
      opt.args.assign(args->begin() + optind, args->end());
    } else {
      Usage(args->front(), "Merging commands not supported.");
    }
  }

  if (opt.use_overlayfs) {
    if (ValidateOverlayOutOfFolder(opt.tmp_overlayfs, opt.sandbox_root) < 0)
      Usage(args->front(),
            "Illegal configuration: overlayfs folder inside sandbox root.");
  }
}

// Expands a single argument, expanding options @filename to read in the content
// of the file and add it to the list of processed arguments.
static unique_ptr<vector<char *>>
ExpandArgument(unique_ptr<vector<char *>> expanded, char *arg) {
  if (arg[0] == '@') {
    const char *filename = arg + 1; // strip off the '@'.
    ifstream f(filename);

    if (!f.is_open()) {
      MiniSbxReport("opening argument file %s failed", filename);
    }

    for (std::string line; std::getline(f, line);) {
      if (!line.empty()) {
        expanded = ExpandArgument(std::move(expanded), strdup(line.c_str()));
      }
    }

    if (f.bad()) {
      MiniSbxReport("error while reading from argument file %s", filename);

    }
  } else {
    expanded->push_back(arg);
  }

  return expanded;
}

// Pre-processes an argument list, expanding options @filename to read in the
// content of the file and add it to the list of arguments. Stops expanding
// arguments once it encounters "--".
static unique_ptr<vector<char *>> ExpandArguments(const vector<char *> &args) {
  unique_ptr<vector<char *>> expanded(new vector<char *>());
  expanded->reserve(args.size());
  for (auto arg = args.begin(); arg != args.end(); ++arg) {
    if (strcmp(*arg, "--") != 0) {
      expanded = ExpandArgument(std::move(expanded), *arg);
    } else {
      expanded->insert(expanded->end(), arg, args.end());
      break;
    }
  }
  return expanded;
}

void ParseOptions(int argc, char *argv[]) {
  vector<char *> args(argv, argv + argc);
  ParseCommandLine(ExpandArguments(args));

  if (opt.working_dir.empty()) {
    opt.working_dir = "";
    if (GetCWD(opt.working_dir) < 0)
      Usage(args.front(), "Could not obtain CWD.");
  }

  if (opt.args.empty()) {
    Usage(args.front(), "No command specified.");
  }
}


#ifdef MINITAP

std::regex ipv4_regex(R"(^(\d{1,3}\.){3}\d{1,3}$)");
std::regex domain_regex(R"(^([a-zA-Z0-9-]+\.)+[a-zA-Z]{2,}$)");
std::regex subnet_regex(R"(^(\d{1,3}\.){3}\d{1,3}/\d{1,2}$)");

static int ReadAllowedConnections(std::ifstream& file) {
    int res = 0;
    std::string line;

    while (std::getline(file, line)) {
      line.erase(0, line.find_first_not_of(" \t\r\n"));
      line.erase(line.find_last_not_of(" \t\r\n") + 1);

      if (std::regex_match(line, ipv4_regex)) {
          MiniSbxAllowIpv4(line.c_str());
      } else if (std::regex_match(line, domain_regex)) {
          MiniSbxAllowDomain(line.c_str());
      } else if (std::regex_match(line, subnet_regex)) {
          MiniSbxAllowIpv4Subnet(line.c_str());
      } else {
          std::cerr << "Warning: Unrecognized rule format: " << line << "\n";
          res = -1;
      }
   } 
   return res;
}

int MiniSbxAllowConnections(const std::string& path) {
    int res = 0;
    if ((res = ValidatePath(__func__, path)) < 0)
      return res;
    std::ifstream file(path);
    if (!file.is_open()) {
        std::cerr << "Warning: Failed to open file.\n";
        return -1;
    }
    res = ReadAllowedConnections(file);
    file.close();
    return res;
}

int MiniSbxAllowMaxConnections(int max_connections) {
  return set_max_connections(max_connections, &(opt.fw_rules));
}

int MiniSbxAllowAllDomains() {
  PRINT_DEBUG("Allow all domains");
  if(reset_firewall_rules(&opt.fw_rules) == 0){
    return 0;
  }else{
    return MiniSbxReportError(__func__, ErrorCode::LogFileNotUnique);
  }
  
}

int MiniSbxAllowDomain(const std::string& domain) {
  const char* domain_str = domain.c_str();
  PRINT_DEBUG("allow domain %s", domain_str);
  if(set_firewall_rule(domain_str, &(opt.fw_rules))<0){
    return MiniSbxReportError(__func__,ErrorCode::IllegalNetworkConfiguration);
  }else{
    return 0;
  }
}

int MiniSbxAllowIpv4(const std::string& ip) {
  const char* ip_str = ip.c_str();
  PRINT_DEBUG("allow ip %s", ip_str);
  if(set_firewall_rule(ip_str, &(opt.fw_rules))<0){
    return MiniSbxReportError(__func__,ErrorCode::IllegalNetworkConfiguration);
  }else{
    return 0;
  }
  return 0;
}

int MiniSbxAllowIpv4Subnet(const std::string& subnet) {
  return MiniSbxAllowIpv4(subnet);
}
#endif

#ifndef MINITAP
int MiniSbxShareNetNamespace() {
    opt.create_netns = NO_NETNS;
    return 0;
}
#endif


int MiniSbxEnableLog(const std::string &path) {
  int res = 0;
  if (opt.debug_path.empty()) {
    fs::path fs_path(path);
    fs::path base_dir = fs_path.parent_path();
    res = ValidatePath(__func__, base_dir.string());
    opt.debug_path.assign(path);
  } else {
    res = MiniSbxReportError(__func__, ErrorCode::LogFileNotUnique);
  }
  return res;
}


bool isInsideHomeDir(const fs::path path){
  fs::path path_canon = fs::path(CanonicPath(path,true));
  fs::path home_dir = GetHomeDir();
  return isSubpath( home_dir,path_canon);
}

std::vector<std::string> getHomeSymlinkedDirs(std::string path){
  std::vector<std::string> result;

  fs::path path_canon = fs::path(CanonicPath(path,true));
  // If the path itself doesn't exist or isn't a directory, return empty
  if (!fs::exists(path_canon) || !fs::is_directory(path_canon)) {
      return result;
  }

  // Iterate entries 
  for (const auto& entry : fs::directory_iterator(path_canon)) {
      // Check if the entry is a symbolic link
      if (fs::is_symlink(entry.symlink_status())) {
          fs::path resolved_symlink=fs::canonical(entry.path());
          fs::path parent_path= resolved_symlink.parent_path();
          result.push_back(parent_path.string());
      }
  }

  return result;
}

int MiniSbxMountBind(const std::string &input_path) { // -M
  std::string path = CanonicPath(input_path, false);
  int res = 0;
  if ((res = ValidatePath(__func__, path)) < 0) {
    return res;
  }
  // Add the current source path to both source and target lists
  opt.bind_mount_sources.emplace_back(path);
  opt.bind_mount_targets.emplace_back(path);
  if(path!=input_path){
    opt.bind_mount_sources.emplace_back(input_path);
    opt.bind_mount_targets.emplace_back(input_path);
  }
  if(isInsideHomeDir(path)){

    std::vector<std::string> add_folder=getHomeSymlinkedDirs(path);
    for (auto i : add_folder){
      opt.bind_mount_sources.emplace_back(i);
      opt.bind_mount_targets.emplace_back(i);

    }
  }

  PRINT_DEBUG("%s(%s)\n", __func__, path.c_str());
  return res;
}

int MiniSbxMountBindSourceToTarget(const std::string &c_source, const std::string& c_target) {
  std::string source = CanonicPath(c_source, false);
  std::string target = CanonicPath(c_target, false);
  ValidatePath(__func__, source);
  opt.bind_mount_sources.emplace_back(source);
  opt.bind_mount_targets.emplace_back(target);
  return 0;
}

int MiniSbxMountWrite(const std::string &input_path) { // -w
  std::string path = CanonicPath(input_path, false);
  int res = 0;
  if ((res = ValidatePath(__func__, path)) < 0)
    return res;
  if(path!=input_path){
      opt.writable_files.emplace_back(input_path);
  }
  opt.writable_files.emplace_back(path);

  if(isInsideHomeDir(path)){

    std::vector<std::string> add_folder=getHomeSymlinkedDirs(path);
    for (auto i : add_folder){
      opt.bind_mount_sources.emplace_back(i);
      opt.bind_mount_targets.emplace_back(i);

    }
  }
  PRINT_DEBUG("%s(%s)\n", __func__, path.c_str());
  return res;
}

int MiniSbxMountTmpfs(const std::string &input_path) { // -w
  std::string path = CanonicPath(input_path, false);
  int res = 0;
  if ((res = ValidatePath(__func__, path)) < 0)
    return res;
  opt.tmpfs_dirs.emplace_back(path);
  PRINT_DEBUG("%s(%s)\n", __func__, path.c_str());
  return res;
}

int MiniSbxMountOverlay(const std::string &input_path) {
  std::string path = CanonicPath(input_path, false);
  int res = 0;
  if (opt.use_overlayfs) {
    std::string overlayfsmount(path);
    if ((res = ValidatePath(__func__, path)) < 0)
      return res;
    opt.overlayfsmount.emplace_back(overlayfsmount, 0, overlayfsmount.length());
    if(overlayfsmount!=input_path){
      opt.overlayfsmount.emplace_back(input_path, 0, overlayfsmount.length());
    }
    if(isInsideHomeDir(path)){

      std::vector<std::string> add_folder=getHomeSymlinkedDirs(path);
      for (auto i : add_folder){
        opt.bind_mount_sources.emplace_back(i);
        opt.bind_mount_targets.emplace_back(i);
      }
    }
  } else {
    res = MiniSbxReportError(__func__, ErrorCode::OverlayOptionNotSet);
  }
  return res;
}

int MiniSbxSetupDefault() {
  int res = 0;
  if (opt.hermetic || opt.use_overlayfs) {
    res = MiniSbxReportError(__func__, ErrorCode::InvalidFunctioningMode);
    return res;
  }
#ifndef MINITAP
  opt.create_netns = NETNS_WITH_LOOPBACK;
#else
  opt.create_netns = NO_NETNS;
#endif

#if defined(LIBMINISANDBOX)
  // Working_dir is assigned in ParseOption for minisandbox and tapbox
  opt.working_dir = "";
  res += GetCWD(opt.working_dir);
#endif
  opt.use_default = true;
  opt.use_overlayfs = true;
  std::string tmp(TMP);
  std::string sbx_temp_dir;
  res = CreateDirectory(TMP, MINI_SBX_TMP, sbx_temp_dir);
  if (res < 0)
    return res;
  MiniSbxMountBindSourceToTarget(sbx_temp_dir, tmp);
  SetupDefaultMounts(opt.bind_mount_sources, opt.bind_mount_targets);
  res += CreateSandboxRoot(tmp);
  res += CreateOverlayfsDir(tmp);
  PRINT_DEBUG("Sandbox root %s\n", opt.sandbox_root.c_str());
  return res;
}

int MiniSbxSetupCustom(const std::string &overlayfs_dir,
                              const std::string &sdbx_root) {
  int res = 0;
  if ((res = MiniSbxSetupSandboxRootWithOverlay(sdbx_root)) < 0)
    return res;
  if ((res = MiniSbxSetupOverlayfsFolder(overlayfs_dir)) < 0)
    return res;
  res = ValidateOverlayOutOfFolder(opt.tmp_overlayfs, opt.sandbox_root);
  return res;
}

int MiniSbxSetupHermetic(const std::string &sdbx_root) {
  int res = 0;
  if (opt.use_default || opt.use_overlayfs) {
    res = MiniSbxReportError(__func__, ErrorCode::InvalidFunctioningMode);
  }
  opt.hermetic = true;
  res = CreateSandboxRoot(sdbx_root);
  return res;
}

void SetupDefaultMounts(std::vector<std::string> &bind_mount_sources,
                        std::vector<std::string> &bind_mount_targets) {
  // Add ~/.local/lib|bin that contain python modules
  std::vector<std::string> default_mounts = {"/proc", "/var", "/opt"};
  std::string local_bin = GetLocalBin();
  std::string local_lib = GetLocalLib();

  try {
    if (fs::exists(local_bin))
      default_mounts.push_back(local_bin);
  } catch (const fs::filesystem_error &e) {
    PRINT_DEBUG("Filesystem error: %s\n", local_bin.c_str());
  }

  try {
    if (fs::exists(local_lib))
      default_mounts.push_back(local_lib);
  } catch (const fs::filesystem_error &e) {
    PRINT_DEBUG("Filesystem error: %s\n", local_lib.c_str());
  }


  for (auto mount : default_mounts) {
    bind_mount_sources.emplace_back(mount);
    bind_mount_targets.emplace_back(mount);
  }
}


int MiniSbxSetupOverlayfsFolder(std::string input_path) {
  std::string path = CanonicPath(input_path, true);

  if (opt.use_overlayfs == true) {
    return CreateOverlayfsDir(path);
  } else {
    return MiniSbxReportError(__func__, ErrorCode::OverlayOptionNotSet);
  }
}

int MiniSbxSetupSandboxRootWithOverlay(const std::string& input_path) {
  int res = 0;
  std::string path = CanonicPath(input_path, true);
  if (opt.use_default || opt.hermetic) {
    res = MiniSbxReportError(__func__, ErrorCode::InvalidFunctioningMode);
    return res;
  }
  opt.use_overlayfs = true;
  res = CreateSandboxRoot(input_path);
  return res;
}

// This function is useful when you want to mount a single file as output,
// instead of a whole directory The file must exists or is created.
int MiniSbxMountEmptyOutputFile(const std::string &path_str) {
  std::string path = CanonicPath(path_str, false);
  if (!fs::exists(path)) {
    int handle = open(path.c_str(), O_CREAT | O_WRONLY | O_EXCL, 0666);
    if (handle < 0) {
      MiniSbxReport("%s: open failed", __func__);
    }
    if (close(handle) < 0) {
      MiniSbxReport("%s: close failed", __func__);
    }
  }
  MiniSbxMountWrite(path);
  return 0;
}

int MiniSbxMountParentsWrite() {
  opt.parents_writable = true;
  return 0;
}
