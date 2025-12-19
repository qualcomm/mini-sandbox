/*
 * Copyright (c) 2025 Qualcomm Technologies, Inc. and/or its subsidiaries.
 * SPDX-License-Identifier: MIT
 */

#include "src/main/tools/docker_support.h"
#include "src/main/tools/linux-sandbox-options.h"
#include "src/main/tools/linux-sandbox.h"

int main(int argc, char *argv[]) {
  int exit_code = 0;
  docker_mode = CheckDockerMode();
  ParseOptions(argc, argv);
  exit_code = MiniSbxStart();
  return exit_code;

}
