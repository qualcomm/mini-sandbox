FROM docker-registry-ro.qualcomm.com/ubuntu:20.04
RUN apt update -y
RUN apt install -y python3 python3-pip python3-setuptools python3-wheel python3-venv python3-dev
RUN DEBIAN_FRONTEND=noninteractive apt install -y -qq software-properties-common
RUN add-apt-repository -y ppa:ubuntu-toolchain-r/test 
RUN apt-get update 
RUN apt-get install -y gcc-11 g++-11 clang 
RUN update-alternatives --install /usr/bin/gcc gcc /usr/bin/gcc-11 110 --slave /usr/bin/g++ g++ /usr/bin/g++-11
RUN pip install pybind11
