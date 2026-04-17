/*
 * Copyright (c) 2025 Qualcomm Technologies, Inc. and/or its subsidiaries.
 * SPDX-License-Identifier: MIT
 */


#ifndef SRC_MAIN_TOOLS_CAPS_ISOLATION_H_
#define SRC_MAIN_TOOLS_CAPS_ISOLATION_H_

#include <memory>
#include "src/main/tools/constants.h"
#include <linux/capability.h>

#define CAP_VERSION _LINUX_CAPABILITY_VERSION_3
#define CAP_WORDS   _LINUX_CAPABILITY_U32S_3

struct Options;
extern Options opt;

void DropCapabilitiesExcept(uint64_t keep);
void DropCapabilities();
int CapsRunTimeLib();
int CapsRunTimeCLI();


#endif
