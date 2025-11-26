# Update g++ to g++-11

- sudo add-apt-repository ppa:ubuntu-toolchain-r/test
- sudo apt-get update
- sudo apt-get install gcc-11 g++-11
- sudo update-alternatives --install /usr/bin/gcc gcc /usr/bin/gcc-11 110 --slave /usr/bin/g++ g++ /usr/bin/g++-11
