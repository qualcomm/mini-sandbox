// Copyright 2017 The Bazel Authors. All rights reserved.
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

#include "src/main/tools/logging.h"
#include "src/main/tools/process-tools.h"

#include <sys/utsname.h>
#include <iostream>
#include <gnu/libc-version.h>
#include <fstream>
#include <string>
#include <stdexcept>


FILE *global_debug = nullptr;


void logOSKernel() {
  struct utsname buf;
  bool res = GetKernelInfo(&buf);
  if (res) {
    PRINT_DEBUG("OS: %s\n", buf.sysname );
    PRINT_DEBUG("Kernel: %s\n", buf.release );
    PRINT_DEBUG("Version: %s\n", buf.version );
    PRINT_DEBUG("Machine: %s\n", buf.machine );
  } else {
    perror("uname");
  }
}


void logLibc() {
  PRINT_DEBUG("libc: %s\n", gnu_get_libc_version());
}


void logLibstdcpp() {
#ifdef _GLIBCXX_RELEASE
    PRINT_DEBUG("libstdc++ release: %d\n", _GLIBCXX_RELEASE);
#endif

#ifdef __GLIBCXX__
    PRINT_DEBUG("__GLIBCXX__: %d\n", __GLIBCXX__);
#endif
}


void logOSName() {
  try {
    std::string pretty, version;
    const bool ok = GetOSName(pretty, version);

    if (!ok) {
      PRINT_DEBUG("Can't log OS info: /etc/os-release missing or keys not found");
      return;
    }

    if (!pretty.empty()) {
      PRINT_DEBUG("OS PRETTY_NAME: %s", pretty.c_str());
    } else {
      PRINT_DEBUG("OS PRETTY_NAME not found");
    }

    if (!version.empty()) {
      PRINT_DEBUG("OS VERSION_ID: %s", version.c_str());
    } else {
      PRINT_DEBUG("OS VERSION_ID not found");
    }
  } catch (const std::exception& e) {
    PRINT_DEBUG("Can't log OS info (exception): %s", e.what());
  } catch (...) {
    PRINT_DEBUG("Can't log OS info (unknown exception)");
  }
}


void logSystem() {
  logOSKernel();
  logOSName();
  logLibc();
  logLibstdcpp();
}

