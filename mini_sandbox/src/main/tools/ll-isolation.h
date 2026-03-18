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

int LLRunTimeLib();
int LLRunTimeCLI();
int LandlockSupported();

#endif
