# Build the project and the library

`build.sh --release` will build all the subprojects and place the output in a local `release/` folder. 

In case you need to build only a subproject, we support 4 different flavors of builds, as listed here. Before running the build `cd mini_sandbox/src/main/tools` . Then:

### Vanilla build CLI

- `make mini-sandbox`

### TAP mode enabled CLI

- `make mini-tapbox`

### libmini-sandbox

- `make libmini-sandbox`

### libmini-sandbox + Firewall APIs

- `make libmini-tapbox`

# Compatibility and support 

The project has been tested on Docker Ubuntu 18.04 until Ubuntu 24.04 and is known to work on those systems. However, in Ubuntu 18.04, users that want to use the static library (`libmini-sandbox`) will have to link with `-lstdc++fs`. Also, in Ubuntu 18.04 the `iptables-nft` frontend that we currently rely on to set up the firewall rules does not exist so we have to use the raw `nft`.

On Ubuntu 24.04 and onwards, mini-sandbox requires manual set up for the full sandbox. Refer to 
[Ubuntu Restrictions](./ubuntu_restrictions.md)


For the compiler, the project is known to build with clang-6 up to clang-19 as well as different version of gcc/g++ from 7.5 up to 13.3.0
