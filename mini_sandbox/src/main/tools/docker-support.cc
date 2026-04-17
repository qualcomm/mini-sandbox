/*
 * Copyright (c) 2025 Qualcomm Technologies, Inc. and/or its subsidiaries.
 * SPDX-License-Identifier: MIT
 */

#include "src/main/tools/docker-support.h"
#include "src/main/tools/process-tools.h"
#include "src/main/tools/constants.h"

static bool isDockerEnvPresent() {
  fs::path path(kDockerPath);
  return fs::exists(path);
}

static bool isRootInOverlay() {
  FILE *mounts = setmntent(kMounts, "r");
  struct mntent *ent;
  if (mounts == nullptr) {
    return false;
  }

  while ((ent = getmntent(mounts)) != nullptr) {
    if (strcmp(ent->mnt_dir, "/") == 0) {
      return (strcmp(ent->mnt_type, "overlay") == 0);
    }
  }
  return false;
}

bool isRunningInDocker() { return isDockerEnvPresent() || isRootInOverlay(); }

enum DockerMode CheckDockerMode() {
  enum DockerMode res;
 
  if (std::getenv(kDockerUnprivilegedEnv) != nullptr)
    res = UNPRIVILEGED_CONTAINER;
  else if (std::getenv(kDockerPrivilegedEnv))
    res = PRIVILEGED_CONTAINER;
  else if (isRunningInDocker())
    res = UNPRIVILEGED_CONTAINER;
  else 
    res = NO_CONTAINER;

  return res;
}
