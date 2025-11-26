#include <stdio.h>
#include <iostream>
#include <sys/socket.h>
#include <arpa/inet.h>
#include <string.h>
#include <unistd.h>
#include <pthread.h>
#include <stdlib.h>

#include "linux-sandbox-api.h"

int main() {
    printf("starting program out of the sandbox pid=%d\n", getpid());

    
    //mini_sandbox_mount_bind("/dev");

    mini_sandbox_setup_default();

    int res = mini_sandbox_start();
    if (res < 0) {
      printf("Didnt start the sandbox the first time (not expected)\n");
      return 0;
    }
    res = mini_sandbox_start();
    if (res < 0) {
      printf("Didnt start the sandbox the second time (as expected)\n");

      int err_code = mini_sandbox_get_last_error_code();
      printf("error code set to %d\n", err_code);
      const char* msg = mini_sandbox_get_last_error_msg();
      printf("%s\n\n", msg);
      return 0;
    }

    printf("\n\nSandbox started with pid=%d. First Trying to connect to 8.8.8.8 via socket...\n", getpid());
    return 0;
}
