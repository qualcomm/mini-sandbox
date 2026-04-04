/*
 * Copyright (c) 2025 Qualcomm Technologies, Inc. and/or its subsidiaries.
 * SPDX-License-Identifier: MIT
 */


#ifndef SRC_MAIN_TOOLS_LINUX_SANDBOX_NETWORK_H_
#define SRC_MAIN_TOOLS_LINUX_SANDBOX_NETWORK_H_

#include <memory>


enum class MiniSbxNetworkType {
    SIMPLE,
    TAP,
};


class MiniSbxNetwork {
  public:
    virtual ~MiniSbxNetwork() = default;
    virtual int RunNetwork () = 0;
    virtual MiniSbxNetworkType Type() const = 0;
};

// Class used to share network with the host
class MiniSbxNetworkSimple final : public MiniSbxNetwork {
  public:  
    int RunNetwork() override;
    MiniSbxNetworkType Type() const override;
};


// Class used to set up network through TAP device
class MiniSbxNetworkTap final : public MiniSbxNetwork {
  public:  
    int RunNetwork() override;
    MiniSbxNetworkType Type() const override;
};

// We can add other classes to implement other ways to handle
// the network for instance via seccomp


// A Factory method that returns the method type
std::unique_ptr<MiniSbxNetwork> MakeMiniSbxNetwork();

#endif
