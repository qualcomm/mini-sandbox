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

# install "iptables-nftables-compat" for ubuntu:18.04 to install iptables-nft subsystem

# Optional if we wanna run with the external user uid
#ARG DOCKER_USER=<username>
#RUN addgroup --system --gid 200 users2 && adduser --system --uid 4138198 --gid 200 $DOCKER_USER

COPY ./QCBuildSandbox /app/QCBuildSandbox
WORKDIR /app/

# Optional if we wanna run with the external user uid
#RUN chown -R $DOCKER_USER:users2 /app
#USER $DOCKER_USER

RUN make -C /app/QCBuildSandbox/mini_sandbox/src/main/tools/

ENV MINI_SANDBOX_DOCKER_PRIVILEGED=1

ENTRYPOINT ["sh", "-c", "cd /app && /bin/bash"]

