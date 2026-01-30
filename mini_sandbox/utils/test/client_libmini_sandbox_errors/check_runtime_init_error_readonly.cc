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
    int res = mini_sandbox_start();
    printf("Try to write into init file\n");
    FILE* f = fopen("/tmp/mini-sandbox-init", "w");
    assert (f == NULL);
    return 0;
}
