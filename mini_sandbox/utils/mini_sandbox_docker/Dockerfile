##
## Copyright (c) 2025 Qualcomm Technologies, Inc. and/or its subsidiaries.
## SPDX-License-Identifier: MIT
##
FROM ubuntu:22.04

RUN apt-get update && \
    apt-get install -y \
        build-essential clang iputils-ping \
        wget \
        iptables vim && \
    apt-get clean

COPY ./QCBuildSandbox /app/QCBuildSandbox
WORKDIR /app/

RUN make -C /app/QCBuildSandbox/mini_sandbox/src/main/tools/

ENV MINI_SANDBOX_DOCKER_PRIVILEGED=1

ENTRYPOINT ["sh", "-c", "cd /app && /bin/bash"]

