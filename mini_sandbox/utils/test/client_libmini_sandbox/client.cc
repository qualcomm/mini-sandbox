/*
 * Copyright (c) 2025 Qualcomm Technologies, Inc. and/or its subsidiaries.
 * SPDX-License-Identifier: MIT
 */
#include <stdio.h>

#include <sys/stat.h>
#include <sys/types.h>

#include <iostream>
#include <sys/socket.h>
#include <arpa/inet.h>
#include <string.h>
#include <unistd.h>
#include <pthread.h>
#include <stdlib.h>
#include <limits.h>
#include <libgen.h>
#include <assert.h>

#include "linux-sandbox-api.h"



int try_file_write(const char* dst) {
    int res = 0;
    FILE* f = fopen(dst, "w");
    if (f != nullptr) {
        printf("fopen() worked\n");
    	fprintf(f, "libmini-sandbox\n");
        printf("write() worked\n");
    	fclose(f);
    }
    else {
        printf("Couldn`t open the file for writing\n");
        res = 1;
    }
    return res;
}

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

int main(int argc, char* argv[]) {
    if (argc > 0)
        printf("\n\nExecutable name: %s\n\n", argv[0]);
    printf("starting program out of the sandbox pid=%d\n", getpid());
    int res = 0;

#if defined(WORKDIR)
    char* buf = (char*) malloc(PATH_MAX);
    size_t n = readlink("/proc/self/exe", buf, PATH_MAX);
    printf("set work dir to %s\n", dirname(buf));
    res = mini_sandbox_set_working_dir(dirname(buf));
    assert (res == 0);
#endif 
#if defined(DEFAULT)
    res = mini_sandbox_setup_default();
    assert (res == 0);
    //mini_sandbox_mount_write(getenv("HOME"));
#if defined(DEFAULT_WRITE_PARENTS)
    res = mini_sandbox_mount_parents_write();
    assert (res == 0);
#endif
#elif defined(CUSTOM)
    const char* overlayfs_dir = "/tmp/overlay_client";
    const char* sandbox_root = "/tmp/sandbox_client";

    const char* bin = "/bin";
    const char* lib = "/lib";
    const char* lib64 = "/lib64";

    res = mini_sandbox_setup_custom(overlayfs_dir, sandbox_root);
    assert (res == 0);
    res = mini_sandbox_mount_overlay(bin);
    assert (res == 0);
    res = mini_sandbox_mount_overlay(lib);
    assert (res == 0);
    res = mini_sandbox_mount_overlay(lib64);
    assert (res == 0);
#elif defined(HERMETIC)
    const char* sandbox_root = "/tmp/";
    const char* bin = "/bin";
    const char* lib = "/lib";
    const char* lib64 = "/lib64";

    res = mini_sandbox_setup_hermetic(sandbox_root);
    assert (res == 0);

    res = mini_sandbox_mount_bind(bin);
    assert (res == 0);
    res = mini_sandbox_mount_bind(lib);
    assert (res == 0);
    res = mini_sandbox_mount_bind(lib64);
    assert (res == 0);
#endif
    // If no option is specified at compile time 
    // the sandbox will run in 'read-only' mode

    res = mini_sandbox_enable_log("/tmp/sandbox_log");
    assert (res == 0);
    res = mini_sandbox_start();
    assert (res == 0);

    

    printf("\n\nSandbox started with pid=%d. First Trying to connect to 8.8.8.8 via socket...\n", getpid());
    int sock = socket(AF_INET, SOCK_STREAM, 0);
    if (sock < 0) {
        perror("Socket creation failed");
    } else {
        struct sockaddr_in server;
        server.sin_family = AF_INET;
        server.sin_port = htons(80);
        server.sin_addr.s_addr = inet_addr("8.8.8.8");

        if (connect(sock, (struct sockaddr *)&server, sizeof(server)) < 0) {
            printf("[+++] Connection to 8.8.8.8 failed as expected [---]\n");
        } else {
            printf("[+++] Connection to 8.8.8.8 succeeded (Shouldve failed) [---]\n");
            abort();
        }
        close(sock);
    }
    printf("\n\nTesting multi-threading inside the sandbox\n");
    start_thread();
    printf("Back in main thread.\n");

    char* home = getenv("HOME");
    const char* test = "/libminisandbox.test";
    if (home == NULL) {
        printf("Home directory is NULL. Trying to write in ./\n");
    }
    size_t size = strlen(home) + strlen(test) + 1;
    char* dst = (char*) malloc(size);
    memset(dst, 0, size);
    strncat(dst, home, strlen(home));
    strncat(dst, test, strlen(test));

    printf("\n\nTrying to write into %s\n", dst);
    // $HOME/libminisandbox.test
    int file_written = try_file_write(dst);
#if defined(DEFAULT)
    assert(file_written == 0);
#elif defined(CUSTOM)
    assert(file_written == 0);
#elif defined(HERMETIC)
    assert(file_written == 1);
#else
	assert(file_written == 1);
#endif
    free(dst);
#if defined(WORKDIR)
    char* parent = dirname(dirname(buf));
    char parent_file[PATH_MAX];
    snprintf(parent_file, sizeof(parent_file), "%s/%s", parent, "libminisandbox.test");
    printf("\n\nTrying to write in the parent %s\n", parent_file);
    file_written = try_file_write(parent_file);
    assert (file_written == 0);
#else
    char cwd[PATH_MAX];
    char parent_file[PATH_MAX];
    if (getcwd(cwd, sizeof(cwd)) != NULL) {
      char* parent = dirname(cwd);
      // CWD/../libminisandbox.test
      snprintf(parent_file, sizeof(parent_file), "%s/%s", parent, "libminisandbox.test");
      printf("\n\nTrying to write in the parent %s\n", parent_file);
      file_written = try_file_write(parent_file);
#if defined(DEFAULT)
      assert(file_written == 0);
#elif defined(CUSTOM)
      assert(file_written == 0);
#elif defined(HERMETIC)
      assert(file_written == 0);
#else
	  assert(file_written == 1);
#endif
    }

#endif // ENDS if not def WORKDIR

    printf("\n\nLaunching `ls` command in subprocess\n");
    launch_interactive_bash();
    return 0;
}
