/*
 * Copyright (c) 2025 Qualcomm Technologies, Inc. and/or its subsidiaries.
 * SPDX-License-Identifier: MIT
 */
#include <stdio.h>
#include <iostream>
#include <sys/socket.h>
#include <arpa/inet.h>
#include <string.h>
#include <unistd.h>
#include <pthread.h>
#include <stdlib.h>
#include <assert.h>

#include "linux-sandbox-api.h"

int main() {
    printf("starting program out of the sandbox pid=%d\n", getpid());

    mini_sandbox_setup_default();
    mini_sandbox_enable_log("/tmp");
    mini_sandbox_enable_log("/tmp");
    int err_code = mini_sandbox_get_last_error_code();
    assert (err_code < 0);
    return 0;
}
