/*
 * Copyright (c) 2025 Qualcomm Technologies, Inc. and/or its subsidiaries.
 * SPDX-License-Identifier: MIT
 */
#include <stdio.h>
#include <string.h>
#include <iostream>
#include <sys/socket.h>
#include <arpa/inet.h>
#include <unistd.h>
#include <pthread.h>
#include <stdlib.h>
#include <assert.h>

#include "connect_with_timeout.h"

#include "linux-sandbox-api.h"



void launch_interactive_bash() {
	char *args[] = {"/bin/ls", NULL};

	if (execvp(args[0], args) == -1) {
		perror("Failed to launch bash");
		exit(EXIT_FAILURE);
	}
}

// Thread function
void* thread_function(void* arg) {
    printf("Hello from the thread! Argument: %s\n", (char*)arg);
    return NULL;
}

// Function that starts the thread
void start_thread() {
    pthread_t thread;
    const char* message = "Thread argument";

    // Create the thread
    int result = pthread_create(&thread, NULL, thread_function, (void*)message);
    if (result != 0) {
        fprintf(stderr, "Error creating thread: %d\n", result);
        exit(EXIT_FAILURE);
    }

    // Wait for the thread to finish
    pthread_join(thread, NULL);
}

#define ALLOWED_IP "142.250.176.14"
#define ALLOWED_DOMAIN "google.com"


#define NOT_ALLOWED_IP "129.46.98.181"
#define NOT_ALLOWED_DOMAIN "qualcomm.com"

int main(int argc, char* argv[]) {
    if (argc > 0) 
        printf("\n\nExecutable name: %s\n\n", argv[0]);

    printf("Starting program out of the sandbox with pid %d\n", getpid());
    int res = 0;

    res = mini_sandbox_setup_default();
    assert(res == 0);
    res = mini_sandbox_mount_write(getenv("HOME"));
    assert(res == 0);

    // Old style API to set firewall
    //mini_sandbox_set_firewall_rule("-A OUTPUT -d 142.250.189.14 -j ACCEPT");

    // New style API to set firewall
    res = mini_sandbox_allow_ipv4("8.8.8.8"); // DNS is always useful :)
    assert(res == 0);
    res = mini_sandbox_allow_ipv4(ALLOWED_IP);
    assert(res == 0);
    res = mini_sandbox_allow_domain(ALLOWED_DOMAIN);
    assert(res == 0);
    res = mini_sandbox_start();
    assert(res == 0);
    

    printf("\n\n!!!! Sandbox started with pid: %d.\n\nFirst Trying to connect to %s (%s)...\n", getpid(), ALLOWED_IP, ALLOWED_DOMAIN);
    int sock = socket(AF_INET, SOCK_STREAM, 0);
    if (sock < 0) {
        perror("Socket creation failed");
    } else {
        struct sockaddr_in server;
        server.sin_family = AF_INET;
        server.sin_port = htons(80);
        server.sin_addr.s_addr = inet_addr(ALLOWED_IP);

        if (connect(sock, (struct sockaddr *)&server, sizeof(server)) < 0) {
            printf("[---] Connection to %s (%s) failed (not expected) [---]", ALLOWED_IP, ALLOWED_DOMAIN);
            abort();
        } else {
            printf("[+++] Connection to %s (%s) succeeded (as expected) [+++]\n", ALLOWED_IP, ALLOWED_DOMAIN);
        }
        close(sock);
    }

    printf("\n\nNow trying to connect to %s (%s)...\n", NOT_ALLOWED_IP, NOT_ALLOWED_DOMAIN);
    sock = socket(AF_INET, SOCK_STREAM, 0);
    if (sock < 0) {
        perror("Socket creation failed");
    } else {
        struct sockaddr_in server;
        server.sin_family = AF_INET;
        server.sin_port = htons(80);
        server.sin_addr.s_addr = inet_addr(NOT_ALLOWED_IP);
        int timeout = 2;

        if (connect_with_timeout(sock, (struct sockaddr *)&server, sizeof(server), timeout ) == 0)  {
            printf("[---] Connection to %s (%s) succeeded (not expected) [---]\n", NOT_ALLOWED_IP, NOT_ALLOWED_DOMAIN);
            abort();
        }
        else {
            printf("[+++] Connection to %s (%s) failed (as expected) [+++]", NOT_ALLOWED_IP, NOT_ALLOWED_DOMAIN);
        }

        close(sock);
    }


    printf("\n\nTesting multi-threading inside the sandbox\n");
    start_thread();
    printf("Back in main thread.\n\n");

    char* home = getenv("HOME");
    const char* test = "/test2";
    if (home == NULL) {
        printf("Home directory is NULL. Trying to write in ./\n");
    }
    size_t size = strlen(home) + strlen(test) + 1;
    char* dst = (char*) malloc(size);
    memset(dst, 0, size);
    strncat(dst, home, strlen(home));
    strncat(dst, test, strlen(test));

    printf("\nTrying to write into %s\n", dst);

    FILE* f = fopen(dst, "w");
    if (f != nullptr) {
        printf("fopen() worked\n");
    	fprintf(f, "AA");
    	fclose(f);
    }
    else {
        printf("Couldn`t open the file for writing (unexpected)\n");
        abort();
    }
    free(dst);

    printf("\n\nLaunching `ls` command in subprocess\n\n");
    launch_interactive_bash();


    return 0;
}
