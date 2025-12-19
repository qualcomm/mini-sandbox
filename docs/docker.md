# Support Docker

All the observations reported here can be generalized to the various tools compiled in this repository (mini-sandbox, libminisandbox, mini-tapbox, etc.)

mini-sandbox runs inside Docker only when run with --privileged. The reason is that Docker by default sets up seccomp filter that block the use of clone and unshare to create usernamesapces. The problem is well known in the community and other projects have the same issue (e.g., bazel, etc.) but nevertheless no real solution exists except for changing the seccomp profile or run with --privileged.

If you need to run mini-sandbox inside a privileged Docker container you want to export the environment variable `MINI_SANDBOX_DOCKER_PRIVILEGED=1`. This way, mini-sandbox will set up the environment in a slightly different way compared to a bare-metal host machine keeping into account the fact that is already running on a pre-existing overlayfs and few other aspects.

For non privileged Docker containers we have to give up many of the benefits we are used to with mini-sandbox. Also, if a non privileged Docker container is deployed properly, it would already provide pretty much the same security assurances and thus there is no point of having two layers of sandboxing. As a best effort solution, the only thing that mini-sandbox will try to do for these scenarios is to drop some of the capabilities that normally `root` has inside the Docker container. If you need to run mini-sandbox inside a non-privileged Docker container export the environment variable `MINI_SANDBOX_DOCKER_UNPRIVILEGED=1`. 

We also implement a set of basic heuristics to automatically detect if we are running in a Docker container or not. In case we detect that the execution environment is Docker-based we will default to the unprivileged case just shown, as it is the most generic one.


