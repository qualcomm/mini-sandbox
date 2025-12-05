FROM ubuntu:18.04

RUN apt-get update && \
    apt-get install -y \
        software-properties-common \
        build-essential clang iputils-ping wget \
        python3 python3-pip \
        iptables-nftables-compat iptables vim && \
        apt-get clean

# install "iptables-nftables-compat" for ubuntu:18.04 to install iptables-nft subsystem
RUN pip3 install --no-cache-dir requests


WORKDIR /app/
RUN mkdir /app/mini-sandbox
RUN mkdir /rel


ARG VER
ENV VERSION=$VER

ENV GO_VERSION=1.25.3
ENV DOCKER_BUILD=1

RUN wget "https://go.dev/dl/go${GO_VERSION}.linux-amd64.tar.gz" && \
    tar -C /usr/local -xzf go${GO_VERSION}.linux-amd64.tar.gz && \
    rm go${GO_VERSION}.linux-amd64.tar.gz

ENV PATH="/usr/local/go/bin:${PATH}"

COPY ./ /app/mini-sandbox

ENV PATH="/app/mini-sandbox/mini_sandbox/src/main/tools/out/:${PATH}"


RUN /app/mini-sandbox/clean.sh
RUN /app/mini-sandbox/build.sh --release
RUN make -C /app/mini-sandbox/mini_sandbox/utils/test/client_libmini_sandbox/
ENV LD_LIBRARY_PATH="/app/mini-sandbox/release/lib/"
RUN make -C /app/mini-sandbox/mini_sandbox/utils/test/client_libmini_tapbox/
ENV MINI_SANDBOX_DOCKER_PRIVILEGED=1
ENV PATH="/app/mini-sandbox/mini_sandbox/src/main/tools/out/:${PATH}"
ENV PYTHON=python3
ENV WORKSPACE="/app/mini-sandbox"
#ENV PACKAGE_DIR="/app/mini-sandbox/release/"


ENTRYPOINT ["sh", "-c", "set -e && cat /etc/resolv.conf && /app/mini-sandbox/mini_sandbox/utils/test/cli_tests/run_all.sh && cp -r /app/mini-sandbox/release /rel"]
#ENTRYPOINT ["sh", "-c", "cd /app && /bin/bash"]


