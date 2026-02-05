# Build the project and the library

## Dependencies

- `clang`
- `golang` (only if you need to run the TAP mode / root-less firewall). To install golang you can run the following commands

```bash
GO_VERSION=1.25.3
wget "https://go.dev/dl/go${GO_VERSION}.linux-amd64.tar.gz"
tar -C /usr/local -xzf go${GO_VERSION}.linux-amd64.tar.gz
```

## Build

`build.sh --release` will build all the subprojects and place the output in a local `release/` folder. If you wish to install the command line binaries and libraries system-wide , `build.sh --install` .

In case you need to build only a subproject, we have four different build steps, as listed here. Before running the build `cd mini_sandbox/src/main/tools` . Then:

### Vanilla build CLI

- `make mini-sandbox`

### TAP mode enabled CLI

- `make mini-tapbox`

### libmini-sandbox

- `make libmini-sandbox`

### libmini-sandbox + Firewall APIs

- `make libmini-tapbox`

## Compatibility and support 

The project has been tested on Docker Ubuntu 18.04 until Ubuntu 24.04 and is known to work on those systems. We have also tested on few other distros such as RedHat, OpenSUSE, Debian, CentOS and WSL.

On Ubuntu 24.04 and onwards, mini-sandbox requires manual set up for the full sandbox. Refer to 
[Ubuntu Restrictions](./ubuntu_restrictions.md)


For the compiler, the project is known to build with clang-6 up to clang-19 as well as different version of gcc/g++ from 7.5 up to 13.3.0
