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
    pid_t initial = getpid();
    mini_sandbox_setup_custom("/tmp/overlay_client", "/tmp/sandbox_client");
    mini_sandbox_mount_overlay("/bin");
    mini_sandbox_mount_overlay("/lib");
    mini_sandbox_mount_overlay("/lib64");
    // /etc/passwd is a file that we cannot mount as overlay. Mounting
    // as overlay a file will make Pid1 exit with failure. In this case we 
    // want to return and not exit
    mini_sandbox_mount_overlay("/etc/passwd");
    int res = mini_sandbox_start();
    printf("Sandbox didnt start (returned %d) but the application can still run\n", res);
    assert (res < 0);
    assert (getpid() == initial);    
    return 0;
}
