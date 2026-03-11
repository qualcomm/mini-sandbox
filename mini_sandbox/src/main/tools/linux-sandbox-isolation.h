/*
 * Copyright (c) 2025 Qualcomm Technologies, Inc. and/or its subsidiaries.
 * SPDX-License-Identifier: MIT
 */


#ifndef SRC_MAIN_TOOLS_LINUX_SANDBOX_ISOLATION_H_
#define SRC_MAIN_TOOLS_LINUX_SANDBOX_ISOLATION_H_

#include <memory>
#include "linux-sandbox-runtime.h"

struct Options;
extern Options opt;


enum class MiniSbxIsolationKind {
  None,
  Namespaces,
  Capabilities,
  Landlock,
};


class MiniSbxIsolation {
public:
  virtual ~MiniSbxIsolation() = default;
  virtual int RunIsolation(MiniSbxExecMode mode) = 0;
};

class MiniSbxIsolationNamespaces final : public MiniSbxIsolation {
public:
  int RunIsolation(MiniSbxExecMode mode) override;
};

class MiniSbxIsolationCapabilities final : public MiniSbxIsolation {
public:
  int RunIsolation(MiniSbxExecMode mode) override;
};

// TODO: Landlock isolation backend (placeholder for now)
// class MiniSbxIsolationLandlock final : public MiniSbxIsolation {
// public:
//   int RunIsolation() override;
// };

std::unique_ptr<MiniSbxIsolation> MakeMiniSbxIsolation();

#endif
