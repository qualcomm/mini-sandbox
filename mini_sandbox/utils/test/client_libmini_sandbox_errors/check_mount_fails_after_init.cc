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
#include <sys/stat.h>
#include <sys/mount.h>

#include "linux-sandbox-api.h"

int main() {
    printf("starting program out of the sandbox pid=%d\n", getpid());
    pid_t initial = getpid();
    mini_sandbox_setup_default();
    int res = mini_sandbox_start();
    assert (res == 0);
    assert (getpid() != initial);  
    assert (access("/tmp", F_OK) == 0);
    printf("Setup ok.\n");
    res = mount("/tmp", "/tmp", NULL, MS_BIND, NULL);
    assert (res < 0);

    return 0;
}
