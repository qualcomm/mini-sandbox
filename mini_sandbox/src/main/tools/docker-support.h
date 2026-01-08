/*
 * Copyright (c) 2025 Qualcomm Technologies, Inc. and/or its subsidiaries.
 * SPDX-License-Identifier: MIT
 */


#ifndef DOCKER_H_
#define DOCKER_H_

#include "src/main/tools/logging.h"
#include "src/main/tools/linux-sandbox-options.h"
#include <iostream>
#include <mntent.h>
#if __has_include(<filesystem>)
#include <filesystem>
namespace fs = std::filesystem;
#else
#include <experimental/filesystem>
namespace fs = std::experimental::filesystem;
#define _EXPERIMENTAL_FILESYSTEM_
#endif
#include <cstring>
#include <sys/mount.h>
#include <errno.h>
#include <fstream>
#include <unistd.h>
#include <vector>
#include <cstdlib>


bool isRunningInDocker();
enum DockerMode CheckDockerMode();


#endif
