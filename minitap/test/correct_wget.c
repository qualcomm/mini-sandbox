#include "libminitap.h"
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/wait.h>

extern char **environ;

int main() {


    MiniTapSetupFirewallRule("-A OUTPUT -j DROP");
    MiniTapSetupFirewallRule("-A OUTPUT -d 142.250.189.14 -j ACCEPT");
    MiniTapSetupFirewallRule("-A OUTPUT -d 142.250.176.14 -j ACCEPT");
    MiniTapSetupFirewallRule("-A OUTPUT -d 142.250.72.164 -j ACCEPT");
    int child_pid = MiniTapUserTCPIP();
    if (child_pid != 0) {
        waitpid(child_pid, NULL, 0);
        exit(0);
    }

    pid_t pid = fork();

    if (pid < 0) {
        perror("fork failed");
        return 1;
    }

    if (pid == 0) {
        // Child process

        char *argv[] = {"/usr/bin/wget", "google.com", NULL};
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
