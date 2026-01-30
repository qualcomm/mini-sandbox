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
    int res = mini_sandbox_start();
    assert (res == 0);
    exit(0);
    // We should never hit the abort() because mini-sandbox should understand that 
    // the exit signal comes from a subprocess and not pid1 init 
    abort();
    return 0;
}
