/*
 * Copyright (c) 2025 Qualcomm Technologies, Inc. and/or its subsidiaries.
 * SPDX-License-Identifier: MIT
 */


#ifndef SRC_MAIN_TOOLS_LINUX_SANDBOX_RUNTIME_H_
#define SRC_MAIN_TOOLS_LINUX_SANDBOX_RUNTIME_H_


void RunTimeCli();

class MiniSbxRunTime {
  public:
    virtual ~MiniSbxRunTime() = default;
    virtual int HandleUnprivilegedContainer () = 0;
    virtual void HandleSignals() = 0;
    virtual void MaybeCloseFds() = 0;
    virtual int RunTime() = 0;
};

class MiniSbxCLIRunTime final : public MiniSbxRunTime {
  public:  
    int HandleUnprivilegedContainer() override;
    void HandleSignals() override;
    void MaybeCloseFds() override;
    int RunTime() override;
};


class MiniSbxLibRunTime final : public MiniSbxRunTime {
  public:
    int HandleUnprivilegedContainer() override;
    void HandleSignals() override;
    void MaybeCloseFds() override;
    int RunTime() override;
};


#endif
