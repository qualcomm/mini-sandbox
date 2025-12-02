/*
 * Copyright (c) 2025 Qualcomm Technologies, Inc. and/or its subsidiaries.
 * SPDX-License-Identifier: MIT
 */

/*
This test makes sure that the filedescriptors opened before the libmini* are carried over and are meaningful after mini_sandbox start
The expected result is a file called "test_open" containing
testing out \n
Giberish\m

We shuffle the filedescriptors by opening file 100 files in temp and closing them
*/
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

int opened_fds[100] = {};
int index_opened_fds=0;
void open_files_in_dir(const char *dir_path) {
    DIR *dir = opendir(dir_path);
    if (!dir) {
        perror("opendir");
        return;
    }

    struct dirent *entry;
    char full_path[4096];

    while ((entry = readdir(dir)) != NULL) {
        // Skip "." and ".."
        if (strcmp(entry->d_name, ".") == 0 || strcmp(entry->d_name, "..") == 0)
            continue;

        snprintf(full_path, sizeof(full_path), "%s/%s", dir_path, entry->d_name);

        struct stat st;
        if (stat(full_path, &st) == -1) {
            perror("stat");
            continue;
        }

        // Only open regular files
        if (S_ISREG(st.st_mode)) {
            if(index_opened_fds>=100){
                return;
            }
            // printf("openedfd %d",index_opened_fds);
            int fd = open(full_path, O_RDONLY);
            opened_fds[index_opened_fds++]=fd;
            if (fd == -1) {
                perror(full_path);
            }
        }
    }

    closedir(dir);
}

void close_fds(){
    for(int i=0;i < index_opened_fds; i++){
        close(opened_fds[i]);
    }

}


int main(){


    int open_fd=open("./test_open",O_RDWR );
    // int dupped_fd=dup3(open_fd,4,0 ); //dup stderr

    int dupped_fd=dup(open_fd); //dup stderr

    if (dupped_fd<0){
        perror("error creating dup3");
        return -1;
    }
    open_files_in_dir("/tmp/");

    mini_sandbox_setup_custom("../overlay_fs_dir","../sandbox_root");
    mini_sandbox_mount_write("/proc");
    printf("starting sandbox\n");
    mini_sandbox_start();
    close_fds();
    
    char string_to_write[] = "testing out \n";
    char string_to_write2[] = "Giberish \n";

    if(write(dupped_fd, string_to_write,strlen(string_to_write))<0){
        perror("Error open fd ");
        return -1;
    }
    if(write(open_fd, string_to_write2,strlen(string_to_write2))<0){
        perror("Error dupped fd ");
        return -1;
    }

    if(close(dupped_fd)<0){
        perror("Error ");
        return -1;
    }
    if(close(open_fd)<0){
        perror("Error ");
        return -1;
    }
}
