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

#include "linux-sandbox-api.h"

int main() {
    printf("starting program out of the sandbox pid=%d\n", getpid());

    
    //mini_sandbox_mount_bind("/dev");

    //mini_sandbox_setup_default();
    mini_sandbox_mount_overlay("/bin");
    //int res = mini_sandbox_setup_hermetic(sandbox_root);

    int err_code = mini_sandbox_get_last_error_code();
    if (err_code < 0) {
        printf("error code set to %d\n", err_code);
        const char* msg = mini_sandbox_get_last_error_msg();
        printf("%s\n\n", msg);
        return 0;
    }
    mini_sandbox_start();
    printf("\n\nSandbox started with pid=%d. First Trying to connect to 8.8.8.8 via socket...\n", getpid());
    return 0;
}
