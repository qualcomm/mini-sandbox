/*
 * Copyright (c) 2025 Qualcomm Technologies, Inc. and/or its subsidiaries.
 * SPDX-License-Identifier: MIT
 */


#ifndef SRC_MAIN_TOOLS_LINUX_SANDBOX_ISOLATION_H_
#define SRC_MAIN_TOOLS_LINUX_SANDBOX_ISOLATION_H_

#include <memory>
#include "src/main/tools/constants.h"
#include "src/main/tools/linux-sandbox-runtime.h"

struct Options;
extern Options opt;


enum class MiniSbxIsolationType {
  NONE,
  NAMESPACES,
  CAPABILITIES,
  LANDLOCK,
};


class MiniSbxIsolation {
public:
  virtual ~MiniSbxIsolation() = default;
  virtual int RunIsolation(MiniSbxExecMode mode) = 0;
  virtual MiniSbxIsolationType Type() const = 0;
};

class MiniSbxIsolationNamespaces final : public MiniSbxIsolation {
public:
  int RunIsolation(MiniSbxExecMode mode) override;
  MiniSbxIsolationType Type() const override;
};

class MiniSbxIsolationCapabilities final : public MiniSbxIsolation {
public:
  int RunIsolation(MiniSbxExecMode mode) override;
  MiniSbxIsolationType Type() const override;
};

// TODO: Landlock isolation backend (placeholder for now)
// class MiniSbxIsolationLandlock final : public MiniSbxIsolation {
// public:
//   int RunIsolation() override;
// };

std::unique_ptr<MiniSbxIsolation> MakeMiniSbxIsolation();

#endif
