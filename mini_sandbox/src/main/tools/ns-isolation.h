/*
 * Copyright (c) 2025 Qualcomm Technologies, Inc. and/or its subsidiaries.
 * SPDX-License-Identifier: MIT
 */


#ifndef SRC_MAIN_TOOLS_NS_ISOLATION_H_
#define SRC_MAIN_TOOLS_NS_ISOLATION_H_

#include <memory>
#include "src/main/tools/constants.h"

struct Options;
extern Options opt;

int NSRunTimeLib();
int NSRunTimeCLI();

#endif
