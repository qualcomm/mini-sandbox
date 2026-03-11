/*
 * Copyright (c) 2025 Qualcomm Technologies, Inc. and/or its subsidiaries.
 * SPDX-License-Identifier: MIT
 */

#include "src/main/tools/linux-sandbox.h"
#include "src/main/tools/linux-sandbox-options.h"
#include "src/main/tools/linux-sandbox-pid1.h"
#include "src/main/tools/linux-sandbox-network.h"
#include "src/main/tools/logging.h"
#include "src/main/tools/minitap-interface.h"
#include "src/main/tools/process-tools.h"
#include "src/main/tools/error-handling.h"
#include "src/main/tools/firewall.h"

#include <memory>
#include <utility>

int MiniSbxNetworkSimple::RunNetwork() {
  // This is just a pass-through as we have
  // already set somewhere in the code
  // opt.create_netns which is the only thing
  // we need for the "Simple" network
  return 0;
}

MiniSbxNetworkType MiniSbxNetworkSimple::Type() const {
  return MiniSbxNetworkType::SIMPLE;
}


int MiniSbxNetworkTap::RunNetwork() {
  return RunMiniTap(); 
}

MiniSbxNetworkType MiniSbxNetworkTap::Type() const {
  return MiniSbxNetworkType::TAP;
}



// This should return a network management depending on opt
// and what we want to use as default. For now we 
// only have two network management which are defined
// at compile time
std::unique_ptr<MiniSbxNetwork> MakeMiniSbxNetwork() {
#ifndef MINITAP
  return std::make_unique<MiniSbxNetworkSimple>();
#else
  return std::make_unique<MiniSbxNetworkTap>();
#endif
}


