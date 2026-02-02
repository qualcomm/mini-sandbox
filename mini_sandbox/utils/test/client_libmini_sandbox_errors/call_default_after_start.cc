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
    mini_sandbox_mount_write("/a/b/c");
    int err_code = mini_sandbox_get_last_error_code();
    printf("error code set to %d\n", err_code);
    const char* msg = mini_sandbox_get_last_error_msg();
    printf("%s\n", msg);
    assert (err_code < 0);
    int res = mini_sandbox_setup_hermetic("/local/mnt/workspace");
    assert (res < 0);
    return 0;
}
