/*
 * Copyright (c) 2025 Qualcomm Technologies, Inc. and/or its subsidiaries.
 * SPDX-License-Identifier: MIT
 */

#include "src/main/tools/linux-sandbox-options.h"
#include "src/main/tools/linux-sandbox-pid1.h"
#include "src/main/tools/linux-sandbox-runtime.h"
#include "src/main/tools/linux-sandbox-network.h"
#include "src/main/tools/linux-sandbox-isolation.h"
#include "src/main/tools/logging.h"
#include "src/main/tools/ns-isolation.h"
#include "src/main/tools/ll-isolation.h"
#include "src/main/tools/minitap-interface.h"
#include "src/main/tools/process-tools.h"
#include "src/main/tools/error-handling.h"
#include "src/main/tools/constants.h"


int MiniSbxIsolationNamespaces::RunIsolation(MiniSbxExecMode mode) {
  if (mode == MiniSbxExecMode::CLI) {
    return NSRunTimeCLI();
  } else {
    return NSRunTimeLib();
  }
}

MiniSbxIsolationType MiniSbxIsolationNamespaces::Type() const {
  return MiniSbxIsolationType::NAMESPACES;
}

int MiniSbxIsolationCapabilities::RunIsolation(MiniSbxExecMode mode) {
  DropCapabilities();
  if (mode == MiniSbxExecMode::CLI) {
    SpawnChild(true);
  } 
  return 0;
}

MiniSbxIsolationType MiniSbxIsolationCapabilities::Type() const {
  return MiniSbxIsolationType::CAPABILITIES;
}

int MiniSbxIsolationLandlock::RunIsolation(MiniSbxExecMode mode) {
  if (mode == MiniSbxExecMode::CLI) {
    return LLRunTimeCLI();
  } else {
    return LLRunTimeLib();
  } 
}

MiniSbxIsolationType MiniSbxIsolationLandlock::Type() const {
  return MiniSbxIsolationType::LANDLOCK;
}


static MiniSbxIsolationType ParseIsolationType(const char* s) {
  if (!s) return MiniSbxIsolationType::NONE;

  if (strcmp(s, "namespace") == 0 ||
      strcmp(s, "namespaces") == 0) {
    return MiniSbxIsolationType::NAMESPACES;
  }

  if (strcmp(s, "cap") == 0 ||
      strcmp(s, "caps") == 0 ||
      strcmp(s, "capabilities") == 0) {
    return MiniSbxIsolationType::CAPABILITIES;
  }

  if (strcmp(s, "landlock") == 0) {
    return MiniSbxIsolationType::LANDLOCK;
  }

  return MiniSbxIsolationType::NONE;
}


std::unique_ptr<MiniSbxIsolation> MakeMiniSbxIsolation() {
  const char* env = std::getenv(kIsolationModeEnv);
  auto kind = ParseIsolationType(env);

  
  switch (kind) {
    case MiniSbxIsolationType::NAMESPACES:
      return std::make_unique<MiniSbxIsolationNamespaces>();

    case MiniSbxIsolationType::CAPABILITIES:
      return std::make_unique<MiniSbxIsolationCapabilities>();

    case MiniSbxIsolationType::LANDLOCK:
      return std::make_unique<MiniSbxIsolationLandlock>();;

    default:
      break;
  }


  if (UserNamespaceSupported()) {
    return std::make_unique<MiniSbxIsolationNamespaces>();
  }
  else {
    return std::make_unique<MiniSbxIsolationCapabilities>();
  }
}
