<!--
Copyright (c) 2025 Qualcomm Technologies, Inc. and/or its subsidiaries.
SPDX-License-Identifier: MIT
-->
# How to run

This is a sample Dockerfile to run mini-sandbox. Build with:

`docker build . -t test_mini_sandbox`

Run with

`docker run --privileged -it test_mini_sandbox  /bin/bash`

If you need to mount some volumes inside Docker and want mini-sandbox to be able to write into a mounted volume you'll need to run with the uid of the external user (the one who's logged into the machine `id -u`). For that just uncomment the two optional lines in the Dockerfile. Example:

`docker run --privileged -v /local/mnt/workspace/Repositories/QCBuildSandbox:/local/mnt/workspace/Repositories/QCBuildSandbox -it test_mini_sandbox  /bin/bash`

If you don't need the writes in the -v volume to be visible outside you can just execute as root inside the container without changing the Dockerfile and just mount it with `-k` within mini-sandbox.

## Environment Variables

By default mini-sandbox has the logic to understand if it's running inside DOcker but it's not easy to figure out if we're running with `--privileged` model. For this reason you can configure one between these two configuration variables"

- MINI_SANDBOX_DOCKER_UNPRIVILEGED=1 if you wanna force mini-sandbox to run in an unprivileged Docker env
- MINI_SANDBOX_DOCKER_PRIVILEGED=1 if you wanna force mini-sandbox to run in a privileged Docker env (i.e., can use clone(), unshare(), ..)
