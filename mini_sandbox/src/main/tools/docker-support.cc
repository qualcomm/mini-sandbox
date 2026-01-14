/*
 * Copyright (c) 2025 Qualcomm Technologies, Inc. and/or its subsidiaries.
 * SPDX-License-Identifier: MIT
 */

#include "docker-support.h"
#include "src/main/tools/process-tools.h"

static bool isDockerEnvPresent() {
  fs::path path("/.dockerenv");
  return fs::exists(path);
}

static bool isRootInOverlay() {
  FILE *mounts = setmntent("/proc/self/mounts", "r");
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
 
  if (std::getenv("MINI_SANDBOX_DOCKER_UNPRIVILEGED") != nullptr)
    res = UNPRIVILEGED_CONTAINER;
  else if (std::getenv("MINI_SANDBOX_DOCKER_PRIVILEGED"))
    res = PRIVILEGED_CONTAINER;
  else if (isRunningInDocker())
    res = UNPRIVILEGED_CONTAINER;
  else 
    res = NO_CONTAINER;

  bool user_namespace = UserNamespaceSupported();
  // We treat systems where user namespaces are not supported
  // as unprivileged containers where clone()/unshare() are not accessible
  // and in those cases our sandbox will work best effort.
  if (!user_namespace) 
    res = UNPRIVILEGED_CONTAINER;

  return res;
}
