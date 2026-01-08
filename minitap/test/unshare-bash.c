
#define _GNU_SOURCE

#include <sched.h>
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <sys/wait.h>
#include <string.h>
#include <sys/types.h>
#include <fcntl.h>
#include <stdarg.h>

#include "utils.h"

#define STACK_SIZE (1024 * 1024)  // 1 MB stack


extern char **environ;

void write_to_file(const char *which, const char *format, ...) {
  FILE * fu = fopen(which, "w");
  va_list args;
  va_start(args, format);
  if (vfprintf(fu, format, args) < 0) {
    perror("cannot write");
    exit(1);
  }
  fclose(fu);
}

void write_map(const char *path, int inside_id, int outside_id) {
    char map[100];
    snprintf(map, sizeof(map), "%d %d 1\n", inside_id, outside_id);
    int fd = open(path, O_WRONLY);
    if (fd >= 0) {
        write(fd, map, strlen(map));
        close(fd);
    } else {
        perror(path);
    }
}
volatile sig_atomic_t signal_received = 0;
void handler(int sig) {
    signal_received = 1;

}

int run_tcpip() {
    signal(SIGUSR1, handler);
    char* minitap_bin = get_minitap_path();
    pid_t p = fork();
    if (p == 0) {
        
        char* const m_args[] = {minitap_bin, NULL};
        execvp(m_args[0], m_args);
    }
    else {
        while (signal_received == 0)
            pause();
        return p;

    }
    return -1;
}


int child_func(char** cmd_argv) {

    pid_t p = fork();
    if (p == 0) {

        execvp(cmd_argv[0], cmd_argv);
        perror("execvp failed");
    } else {
        waitpid(p, NULL, 0);
        exit(0);
    }

    return 1;
}


static int JoinNetNs(pid_t p) {
  char netns_path[256];
  snprintf(netns_path, sizeof(netns_path), "/proc/%d/ns/net", p);

  int fd = open(netns_path, O_RDONLY);
  if (fd == -1) {
    return -1;
  }

  if (setns(fd, CLONE_NEWNET) == -1) {
    close(fd);
    return -1;
  }

  close(fd);
  return 0;
}

int main(int argc, char* argv[]) {

    uid_t uid = getuid();
    gid_t gid = getgid();

    char * bash_args[] = { "/bin/bash", NULL };
    char** cmd_argv;
    if (argc == 1) {
       cmd_argv = bash_args;
    }
    else {
	   cmd_argv = &argv[1];
    }

    pid_t p = fork();
    if (p == 0)  {
        int r = unshare(CLONE_NEWUSER);
        if (r == -1) {
            perror("unshare"); 
            exit(EXIT_FAILURE);
        }
        write_map("/proc/self/uid_map", 0, uid);
        write_to_file("/proc/self/setgroups", "deny");
        write_map("/proc/self/gid_map", 0, gid);
    
        pid_t minitap_pid = fork();
        if (minitap_pid == 0) {
            pid_t tcp_pid = run_tcpip();
            JoinNetNs(tcp_pid);
            child_func(cmd_argv);
        } else {
            waitpid(minitap_pid, NULL, 0);
            exit(0);
        }
    }
    else {
        waitpid(p, NULL, 0);
        exit(0);
    }
    return 0;
}


