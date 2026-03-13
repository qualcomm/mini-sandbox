/*
 * Copyright (c) 2025 Qualcomm Technologies, Inc. and/or its subsidiaries.
 * SPDX-License-Identifier: MIT
 */


#ifndef SRC_MAIN_TOOLS_LINUX_SANDBOX_RUNTIME_H_
#define SRC_MAIN_TOOLS_LINUX_SANDBOX_RUNTIME_H_

#include "src/main/tools/constants.h"
#include <memory>

class MiniSbxNetwork;
class MiniSbxIsolation;

class MiniSbxRunTime {
  public:

    explicit MiniSbxRunTime(std::unique_ptr<MiniSbxIsolation> isolation,
                            std::unique_ptr<MiniSbxNetwork> net)
        : isolation_(std::move(isolation)), network_(std::move(net)) {}
  
    MiniSbxRunTime(const MiniSbxRunTime&) = delete;
    MiniSbxRunTime& operator=(const MiniSbxRunTime&) = delete;
    MiniSbxRunTime(MiniSbxRunTime&&) noexcept = default;
    MiniSbxRunTime& operator=(MiniSbxRunTime&&) noexcept = default;
    virtual ~MiniSbxRunTime() = default;

    virtual void HandleSignals() = 0;
    virtual void MaybeCloseFds() = 0;
    virtual int RunTime() = 0;
    int RunNet();

  protected:
    MiniSbxIsolation& isolation() { return *isolation_; }
    MiniSbxNetwork& network() { return *network_; }

  private:
    std::unique_ptr<MiniSbxIsolation> isolation_;
    std::unique_ptr<MiniSbxNetwork> network_;
};

class MiniSbxCLIRunTime final : public MiniSbxRunTime {
  public:  
    using MiniSbxRunTime::MiniSbxRunTime;

    void HandleSignals() override;
    void MaybeCloseFds() override;
    int RunTime() override;
};


class MiniSbxLibRunTime final : public MiniSbxRunTime {
  public:
    using MiniSbxRunTime::MiniSbxRunTime;

    void HandleSignals() override;
    void MaybeCloseFds() override;
    int RunTime() override;
};

std::unique_ptr<MiniSbxRunTime> MakeMiniSbxRunTime(MiniSbxExecMode mode);



#endif
