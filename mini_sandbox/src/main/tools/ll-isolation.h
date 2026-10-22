/*
 * Copyright (c) 2025 Qualcomm Technologies, Inc. and/or its subsidiaries.
 * SPDX-License-Identifier: MIT
 */


#ifndef SRC_MAIN_TOOLS_LL_ISOLATION_H_
#define SRC_MAIN_TOOLS_LL_ISOLATION_H_

#include <memory>
#include "src/main/tools/constants.h"

// Landlock ABI < 4 does break basic operations such as rename
// and others
#define MIN_LL_ABI 4

struct Options;
extern Options opt;

// In namespace based isolation we would get these from /proc/self/mounts
// but in landlock we don't know "what's already been mounted" so for now
// it's just easier to hardcode these
inline const char* ll_devs[] = {"/dev/pts", "/dev/shm", 
                                "/dev/hugepages", 
                                "/dev/mqueue", NULL};


int LLRunTimeLib();
int LLRunTimeCLI();
int LandlockSupported();

#endif
