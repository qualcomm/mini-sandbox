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
#include <errno.h>
#include <netdb.h>
#include <string>

#include "connect_with_timeout.h"

#include "linux-sandbox-api.h"
#include "utils.h"



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




// Diagnostic for CI: when the connection that must succeed fails, try a few
// other destinations and print the errno of each. EACCES means landlock
// denied it, anything else (ETIMEDOUT, ENETUNREACH, ECONNREFUSED...) means
// the connection was allowed by the sandbox and the network refused it.
static void probe_connect(const char* ip, int port) {
    int s = socket(AF_INET, SOCK_STREAM, 0);
    if (s < 0) {
        printf("[diag] %s:%d socket() failed: errno=%d (%s)\n", ip, port, errno, strerror(errno));
        return;
    }
    struct sockaddr_in addr;
    memset(&addr, 0, sizeof(addr));
    addr.sin_family = AF_INET;
    addr.sin_port = htons(port);
    addr.sin_addr.s_addr = inet_addr(ip);
    int rc = connect_with_timeout(s, (struct sockaddr *)&addr, sizeof(addr), 3);
    printf("[diag] %s:%d -> %s errno=%d (%s)\n", ip, port,
           rc == 0 ? "connected" : "failed", errno, strerror(errno));
    close(s);
}

static void print_proc_status_lines() {
    FILE* f = fopen("/proc/self/status", "r");
    if (!f) return;
    char line[256];
    while (fgets(line, sizeof(line), f)) {
        if (!strncmp(line, "NoNewPrivs", 10) || !strncmp(line, "Seccomp", 7))
            printf("[diag] %s", line);
    }
    fclose(f);
}

static void diagnose_connect_failure() {
    printf("[diag] LANDLOCK_TEST=%s MINI_SANDBOX_ISOLATION_MODE=%s\n",
           getenv("LANDLOCK_TEST") ? getenv("LANDLOCK_TEST") : "(unset)",
           getenv("MINI_SANDBOX_ISOLATION_MODE") ? getenv("MINI_SANDBOX_ISOLATION_MODE") : "(unset)");
    print_proc_status_lines();
    probe_connect(ALLOWED_IP, 80);   // the one that failed, errno again
    probe_connect(ALLOWED_IP, 443);  // allowed port, same host
    probe_connect("8.8.8.8", 53);    // allowed port, different host
    probe_connect("8.8.8.8", 443);   // allowed port, different host
    probe_connect("8.8.8.8", 21);    // port not allowed: EACCES expected under landlock
    probe_connect("127.0.0.1", 80);  // loopback
}


// The hardcoded ALLOWED_IP is not reachable from every network (it timed out
// from the GitHub runners). Landlock filters by port only, so the address does
// not matter to the sandbox: resolve the domain before the sandbox starts and
// use whatever it currently points to, falling back to the hardcoded one.
static std::string resolve_allowed_ip() {
    struct addrinfo hints;
    memset(&hints, 0, sizeof(hints));
    hints.ai_family = AF_INET;
    hints.ai_socktype = SOCK_STREAM;
    struct addrinfo* res = nullptr;
    std::string ip = ALLOWED_IP;
    if (getaddrinfo(ALLOWED_DOMAIN, "80", &hints, &res) == 0 && res) {
        char buf[INET_ADDRSTRLEN];
        auto* a = (struct sockaddr_in*)res->ai_addr;
        if (inet_ntop(AF_INET, &a->sin_addr, buf, sizeof(buf)))
            ip = buf;
    }
    if (res) freeaddrinfo(res);
    return ip;
}

int main(int argc, char* argv[]) {
    // This test reports every failure through abort(), which does not flush
    // stdio, and under CI stdout is a pipe and therefore fully buffered. The
    // printf right before an abort is the only thing that says which check
    // failed, so keep stdout unbuffered or the log stops at "Aborted" with no
    // explanation.
    setvbuf(stdout, nullptr, _IONBF, 0);

    if (argc > 0)
        printf("\n\nExecutable name: %s\n\n", argv[0]);

    printf("Starting program out of the sandbox with pid %d\n", getpid());
    int res = 0;
    int ll_enabled = landlock_test_enabled();
    std::string allowed_ip = ll_enabled ? resolve_allowed_ip() : std::string(ALLOWED_IP);
    printf("Allowed IP for %s: %s\n", ALLOWED_DOMAIN, allowed_ip.c_str());

    res = mini_sandbox_setup_default();
    assert(res == 0);
    res = mini_sandbox_mount_write(getenv("HOME"));
    assert(res == 0);
    if (ll_enabled) {
      res = mini_sandbox_allow_port("80");
      assert(res == 0);
      res = mini_sandbox_allow_port("443");
      assert(res == 0);
      res = mini_sandbox_allow_port("53");
      assert(res == 0);
    }

#ifdef MAX_CONN
    res = mini_sandbox_allow_max_connections(1);
    assert(res == 0);
#else
    res = mini_sandbox_allow_ipv4("8.8.8.8"); // DNS is always useful :)
    assert(res == 0);
    res = mini_sandbox_allow_ipv4(allowed_ip.c_str());
    assert(res == 0);
    res = mini_sandbox_allow_domain(ALLOWED_DOMAIN);
    assert(res == 0);
#endif
    res = mini_sandbox_start();
    assert(res == 0);
    

    printf("\n\n!!!! Sandbox started with pid: %d.\n\nFirst Trying to connect to %s (%s)...\n", getpid(), allowed_ip.c_str(), ALLOWED_DOMAIN);
    int sock = socket(AF_INET, SOCK_STREAM, 0);
    if (sock < 0) {
        perror("Socket creation failed");
    } else {
        struct sockaddr_in server;
        server.sin_family = AF_INET;
        server.sin_port = htons(80);
        server.sin_addr.s_addr = inet_addr(allowed_ip.c_str());

        if (connect(sock, (struct sockaddr *)&server, sizeof(server)) < 0) {
            printf("[---] Connection to %s (%s) failed (not expected): errno=%d (%s) [---]\n",
                   allowed_ip.c_str(), ALLOWED_DOMAIN, errno, strerror(errno));
            diagnose_connect_failure();
            abort();
        } else {
            printf("[+++] Connection to %s (%s) succeeded (as expected) [+++]\n", allowed_ip.c_str(), ALLOWED_DOMAIN);
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
        int port = 80;
        if (ll_enabled)
          port = 21;
        server.sin_port = htons(port);
        server.sin_addr.s_addr = inet_addr(NOT_ALLOWED_IP);
        int timeout = 2;

        bool conn = connect_with_timeout(sock, (struct sockaddr *)&server, sizeof(server), timeout ) == 0;

        if (conn)  {
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
