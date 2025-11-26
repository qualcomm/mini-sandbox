#include "libminitap.h"
#include <stdio.h>
#include <unistd.h>
#include <stdlib.h>
#include <sys/types.h>
#include <sys/wait.h>

extern char **environ;

int main() {

    MiniTapSetupFirewallRule("-A OUTPUT -j DROP");
    int r = MiniTapUserTCPIP();
    if (r != 0) {
        waitpid(r, NULL, 0);
        exit(0);
    }


    pid_t pid = fork();

    if (pid < 0) {
        perror("fork failed");
        return 1;
    }

    if (pid == 0) {
        // Child process

        char *argv[] = {"/usr/bin/wget", "-T", "1", "-t", "1", "qualcomm.com", NULL};
        char *envp[] = {NULL}; // Inherit environment

        execve("/usr/bin/wget", argv, environ);

        // If execve fails
        perror("execve failed");
        return 1;
    } else {
        // Parent process
        wait(NULL); // Wait for child to finish
        printf("Child process exited\n");
    }

    return 0;
}
