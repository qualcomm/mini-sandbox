/*
 * Copyright (c) 2025 Qualcomm Technologies, Inc. and/or its subsidiaries.
 * SPDX-License-Identifier: MIT
 */


#ifndef LANDLOCK_TEST_H
#define LANDLOCK_TEST_H

#include <cstdlib>   // getenv
#include <cstring>   // strcmp

inline int landlock_test_enabled(void)
{
    const char *env = std::getenv("LANDLOCK_TEST");
    return env != nullptr && std::strcmp(env, "1") == 0;
}

#endif // LANDLOCK_TEST_H

