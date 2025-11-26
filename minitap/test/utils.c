#include "utils.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <libgen.h>
#include <limits.h>

char* get_minitap_path() {
    char exe_path[PATH_MAX];
    ssize_t len = readlink("/proc/self/exe", exe_path, sizeof(exe_path) - 1);
    if (len == -1) {
        perror("readlink");
        return NULL;
    }
    exe_path[len] = '\0';

    // Get the directory name
    char* dir = dirname(exe_path);

    // Allocate memory for the final path
    size_t final_len = strlen(dir) + strlen("/../out/minitap") + 1;
    char* final_path = malloc(final_len);
    if (!final_path) {
        perror("malloc");
        return NULL;
    }

    snprintf(final_path, final_len, "%s/../out/minitap", dir);
    return final_path;
}

