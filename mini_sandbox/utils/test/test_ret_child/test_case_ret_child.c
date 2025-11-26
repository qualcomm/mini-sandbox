
#include <sched.h>
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <string.h>
#include <sys/syscall.h>
#include <sys/mount.h>
#include "linux-sandbox-api.h"
#include <fcntl.h> 
#include <sys/stat.h> 
#include <dirent.h>

#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <sys/wait.h>

int main(){

    // mini_sandbox_setup_custom("../overlay_fs_dir","../sandbox_root");
    // mini_sandbox_mount_write("/proc");
    // mini_sandbox_mount_write("/tmp/pytest-of-rstellut/pytest-59/tmp_sectools_binary0/");
    mini_sandbox_mount_bind("/usr2/rstellut/.pyenv/");
    mini_sandbox_setup_default();
    printf("starting sandbox\n");
    mini_sandbox_start();
  
    pid_t pid = fork();

    if (pid < 0) {
        // Fork failed
        perror("fork");
        return 1;
    } else if (pid == 0) {
        // Child process
        printf("Child process (PID: %d) exiting with -5\n", getpid());
        exit(-5);  // This will be interpreted as 251 by the parent
    } else {
        // Parent process
        int status;
        waitpid(pid, &status, 0);

        if (WIFEXITED(status)) {
            int exit_code = WEXITSTATUS(status);
            printf("Parent: Child exited with code %d\n", exit_code);
        } else {
            printf("Parent: Child did not exit normally\n");
        }
    }

    
    int exit_code;

    // Launch "myprog" using system()
    exit_code = system("exit -5");

    // Check if system() call was successful
    if (exit_code == -1) {
        perror("system");
        return 1;
    }

    // Extract and print the actual exit status of "myprog"
    if (WIFEXITED(exit_code)) {
        printf("myprog exited with status %d\n", WEXITSTATUS(exit_code));
    } else {
        printf("myprog did not terminate normally\n");
    }

}
