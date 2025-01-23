//#define _GNU_SOURCE
#include <err.h>
#include <sched.h>
#include <unistd.h>
#include <stdint.h>
#include <stdlib.h>
#include <sys/wait.h>
#include <signal.h>
#include <fcntl.h>
#include <stdio.h>
#include <string.h>
#include <limits.h>
#include <errno.h>
#include <sys/ioctl.h>
#include <net/if.h>
#include <sys/mount.h>
#include <sys/types.h>
#include <string>

#include <mntent.h>

struct child_args {
	char **argv;        /* Command to be executed by child, with args */
	int    pipe_fd[2];  /* Pipe used to synchronize parent and child */
};

static int verbose;

	static void
usage(char *pname)
{
	fprintf(stderr, "Usage: %s [options] cmd [arg...]\n\n", pname);
	fprintf(stderr, "Create a child process that executes a shell "
			"command in a new user namespace, network namespace and mount namespace\n\n");
	fprintf(stderr, "Options can be:\n\n");
#define fpe(str) fprintf(stderr, "    %s", str);
	fpe("-v          Display verbose messages\n");
        fpe("-i          New IPC namespace\n");
        fpe("-p          New PID namespace\n");

	exit(EXIT_FAILURE);
}


static void
update_map(char *mapping, char *map_file)
{
	int fd;
	size_t map_len;     /* Length of 'mapping' */

	map_len = strlen(mapping);
	for (size_t j = 0; j < map_len; j++)
		if (mapping[j] == ',')
			mapping[j] = '\n';

	fd = open(map_file, O_RDWR);
	if (fd == -1) {
		fprintf(stderr, "ERROR: open %s: %s\n", map_file,
				strerror(errno));
		exit(EXIT_FAILURE);
	}

	if (write(fd, mapping, map_len) != map_len) {
		fprintf(stderr, "ERROR: write %s: %s\n", map_file,
				strerror(errno));
		exit(EXIT_FAILURE);
	}

	close(fd);
}

static void SetupMountNamespace() {
  // Fully isolate our mount namespace private from outside events, so that
  // mounts in the outside environment do not affect our sandbox.
  //fprintf(stderr, "AAAAAAAAAAAAAA\n");
  if (mount(NULL, "/", NULL, MS_REC | MS_PRIVATE, NULL) < 0) {
    fprintf(stderr, "SetupMountNamespace\n");
    exit(EXIT_FAILURE);
  }
}


static void
proc_setgroups_write(pid_t child_pid, char *str)
{
	char setgroups_path[PATH_MAX];
	int fd;

	snprintf(setgroups_path, PATH_MAX, "/proc/%jd/setgroups",
			(intmax_t) child_pid);

	fd = open(setgroups_path, O_RDWR);
	if (fd == -1) {

		if (errno != ENOENT)
			fprintf(stderr, "ERROR: open %s: %s\n", setgroups_path,
					strerror(errno));
		return;
	}

	if (write(fd, str, strlen(str)) == -1)
		fprintf(stderr, "ERROR: write %s: %s\n", setgroups_path,
				strerror(errno));

	close(fd);
}


static bool ShouldBeWritable(const std::string &mnt_dir) {
  char cwd[1024];
  if (getcwd(cwd, sizeof(cwd)) != NULL) {
	std::string currentDir(cwd);
        fprintf(stderr, "%s +++++++ CWD = %s\n", mnt_dir.c_str(), cwd);
  	if (mnt_dir == currentDir) {
                fprintf(stderr, "This SHould Be Writable\n");
    		return true;
  	}
  }

  if (mnt_dir == "/tmp") 
      return true;


  /*if (opt.enable_pty && mnt_dir == "/dev/pts") {
    return true;
  }*/

  /*for (const std::string &writable_file : opt.writable_files) {
    if (mnt_dir == writable_file) {
      return true;
    }
  }

  for (const std::string &tmpfs_dir : opt.tmpfs_dirs) {
    if (mnt_dir == tmpfs_dir) {
      return true;
    }
  }*/

  return false;
}


static void MakeFilesystemMostlyReadOnly() {
  FILE *mounts = setmntent("/proc/self/mounts", "r");
  if (mounts == NULL) {
    fprintf(stderr, "setmntent");
  }

  struct mntent *ent;
  while ((ent = getmntent(mounts)) != nullptr) {
    int mountFlags = MS_BIND | MS_REMOUNT;

    // MS_REMOUNT does not allow us to change certain flags. This means, we have
    // to first read them out and then pass them in back again. There seems to
    // be no better way than this (an API for just getting the mount flags of a
    // mount entry as a bitmask would be great).
    if (hasmntopt(ent, "nodev") != nullptr) {
      mountFlags |= MS_NODEV;
    }
    if (hasmntopt(ent, "noexec") != nullptr) {
      mountFlags |= MS_NOEXEC;
    }
    if (hasmntopt(ent, "nosuid") != nullptr) {
      mountFlags |= MS_NOSUID;
    }
    if (hasmntopt(ent, "noatime") != nullptr) {
      mountFlags |= MS_NOATIME;
    }
    if (hasmntopt(ent, "nodiratime") != nullptr) {
      mountFlags |= MS_NODIRATIME;
    }
    if (hasmntopt(ent, "relatime") != nullptr) {
      mountFlags |= MS_RELATIME;
    }

    if (!ShouldBeWritable(ent->mnt_dir)) {
      mountFlags |= MS_RDONLY;
    }

    //fprintf(stderr, "remount %s: %s", (mountFlags & MS_RDONLY) ? "ro" : "rw", ent->mnt_dir);

    if (mount(nullptr, ent->mnt_dir, nullptr, mountFlags, nullptr) < 0) {
      // If we get EACCES or EPERM, this might be a mount-point for which we
      // don't have read access. Not much we can do about this, but it also
      // won't do any harm, so let's go on. The same goes for EINVAL or ENOENT,
      // which are fired in case a later mount overlaps an earlier mount, e.g.
      // consider the case of /proc, /proc/sys/fs/binfmt_misc and /proc, with
      // the latter /proc being the one that an outer sandbox has mounted on
      // top of its parent /proc. In that case, we're not allowed to remount
      // /proc/sys/fs/binfmt_misc, because it is hidden. If we get ESTALE, the
      // mount is a broken NFS mount. In the ideal case, the user would either
      // fix or remove that mount, but in cases where that's not possible, we
      // should just ignore it. Similarly, one can get ENODEV in case of
      // autofs/automount failure.
      switch (errno) {
        case EACCES:
        case EPERM:
        case EINVAL:
        case ENOENT:
        case ESTALE:
        case ENODEV:
          fprintf(stderr,
              "remount(nullptr, %s, nullptr, %d, nullptr) failure (%m) ignored",
              ent->mnt_dir, mountFlags);
          break;
        default:
          fprintf(stderr, "remount(nullptr, %s, nullptr, %d, nullptr)", ent->mnt_dir,
              mountFlags);
      }
    }
  }

  endmntent(mounts);
}

static int setup_network() {
    int sockfd;
    struct ifreq ifr;

    sockfd = socket(AF_INET, SOCK_DGRAM, 0);
    if (sockfd < 0) {
        perror("socket");
        return 1;
    }

    strncpy(ifr.ifr_name, "lo", IFNAMSIZ);

    if (ioctl(sockfd, SIOCGIFFLAGS, &ifr) < 0) {
        perror("ioctl(SIOCGIFFLAGS)");
        close(sockfd);
        return 1;
    }

    ifr.ifr_flags |= IFF_UP;
    if (ioctl(sockfd, SIOCSIFFLAGS, &ifr) < 0) {
        perror("ioctl(SIOCSIFFLAGS)");
        close(sockfd);
        return 1;
    }

    close(sockfd);

    printf("Interface lo is up\n");
    return 0;
}

static int
childFunc(void *arg)
{
	struct child_args *args = (struct child_args*) arg;
	char ch;

	close(args->pipe_fd[1]);    /* Close our descriptor for the write
				       end of the pipe so that we see EOF
				       when parent closes its descriptor. */
	if (read(args->pipe_fd[0], &ch, 1) != 0) {
		fprintf(stderr,
				"Failure in child: read from pipe returned != 0\n");
		exit(EXIT_FAILURE);
	}

	close(args->pipe_fd[0]);

 	SetupMountNamespace();
        char cwd[1024];
        if (getcwd(cwd, sizeof(cwd)) != NULL) {
	
	if (mount(cwd, cwd, nullptr, MS_BIND | MS_REC, nullptr) < 0) {
      		fprintf(stderr, "mount(%s, %s, nullptr, MS_BIND | MS_REC, nullptr)", cwd, "/tmp/sandbox");
    	}
        }
	MakeFilesystemMostlyReadOnly();
       
        /* set up network interface */
        if (setup_network() != 0) {
		fprintf(stderr, "Failure in child: could not set up lo network interface\n");
		exit(EXIT_FAILURE);
	}
	/* Execute a shell command. */

	printf("About to exec %s\n", args->argv[0]);
	execvp(args->argv[0], args->argv);
	err(EXIT_FAILURE, "execvp");
}

#define STACK_SIZE (1024 * 1024)

static char child_stack[STACK_SIZE];    /* Space for child's stack */


int main(int argc, char *argv[])
{
	int flags, opt, map_zero;
	pid_t child_pid;
	struct child_args args;
	char *uid_map, *gid_map;
	const int MAP_BUF_SIZE = 100;
	char map_buf[MAP_BUF_SIZE];
	char map_path[PATH_MAX];

        flags = CLONE_NEWNET | CLONE_NEWNS | CLONE_NEWUSER;
	verbose = 0;
	gid_map = NULL;
	uid_map = NULL;
	map_zero = 0;

	while ((opt = getopt(argc, argv, "+ipv")) != -1) {
		switch (opt) {
			case 'i': flags |= CLONE_NEWIPC;        break;
			//case 'm': flags |= CLONE_NEWNS;         break;
			//case 'n': flags |= CLONE_NEWNET;        break;
			case 'p': flags |= CLONE_NEWPID;        break;
			//case 'u': flags |= CLONE_NEWUTS;        break;
			case 'v': verbose = 1;                  break;
			default:  usage(argv[0]);
		}
	}
	args.argv = &argv[optind];

	/* We use a pipe to synchronize the parent and child, in order to
	   ensure that the parent sets the UID and GID maps before the child
	   calls execve(). This ensures that the child maintains its
	   capabilities during the execve() in the common case where we
	   want to map the child's effective user ID to 0 in the new user
	   namespace. Without this synchronization, the child would lose
	   its capabilities if it performed an execve() with nonzero
	   user IDs (see the capabilities(7) man page for details of the
	   transformation of a process's capabilities during execve()). */

	if (pipe(args.pipe_fd) == -1)
		err(EXIT_FAILURE, "pipe");

	child_pid = clone(childFunc, child_stack + STACK_SIZE,
			flags | SIGCHLD, &args);
	if (child_pid == -1)
		err(EXIT_FAILURE, "clone");

	if (verbose)
		printf("%s: PID of child created by clone() is %jd\n",
				argv[0], (intmax_t) child_pid);

	snprintf(map_path, PATH_MAX, "/proc/%jd/uid_map", (intmax_t) child_pid);
	snprintf(map_buf, MAP_BUF_SIZE, "0 %jd 1", (intmax_t) getuid());
	uid_map = map_buf;
	update_map(uid_map, map_path);


	proc_setgroups_write(child_pid, "deny");

	snprintf(map_path, PATH_MAX, "/proc/%jd/gid_map", (intmax_t) child_pid);
	snprintf(map_buf, MAP_BUF_SIZE, "0 %ld 1", (intmax_t) getgid());
	gid_map = map_buf;
	update_map(gid_map, map_path);


	/* Close the write end of the pipe, to signal to the child that we
	   have updated the UID and GID maps. */

	close(args.pipe_fd[1]);

	if (waitpid(child_pid, NULL, 0) == -1)
		err(EXIT_FAILURE, "waitpid");

	if (verbose)
		printf("%s: terminating\n", argv[0]);

	exit(EXIT_SUCCESS);
}
